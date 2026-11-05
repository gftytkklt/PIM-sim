#include "simulator/Workload.h"

#include <algorithm>
#include <boost/json.hpp>
#include <cmath>
#include <climits>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include "simulator/Simulator.h"

namespace pim {
namespace {
namespace json = boost::json;
using Coord = std::pair<int, int>;
uint64_t positive(const json::value& value, const char* name, bool zero = false) {
    uint64_t n;
    if (value.is_uint64()) n = value.as_uint64();
    else if (value.is_int64() && value.as_int64() >= 0) n = value.as_int64();
    else throw std::invalid_argument(std::string(name) + " must be a nonnegative integer");
    if (!zero && n == 0) throw std::invalid_argument(std::string(name) + " must be positive");
    return n;
}
uint64_t add(uint64_t a, uint64_t b) {
    if (b > std::numeric_limits<uint64_t>::max() - a)
        throw std::overflow_error("Workload cycle/volume overflow");
    return a + b;
}
uint64_t mul(uint64_t a, uint64_t b) {
    if (b && a > std::numeric_limits<uint64_t>::max() / b)
        throw std::overflow_error("Workload volume overflow");
    return a * b;
}
uint64_t bytes_for(uint64_t elements, uint64_t bits) {
    auto total = mul(elements, bits);
    return total / 8 + (total % 8 != 0);
}
Coord coord(const json::value& v) {
    const auto& a = v.as_array();
    if (a.size() != 2) throw std::invalid_argument("Coordinate requires two integers");
    auto x = positive(a[0], "coordinate", true), y = positive(a[1], "coordinate", true);
    if (x > INT_MAX || y > INT_MAX) throw std::invalid_argument("Coordinate exceeds int range");
    return {static_cast<int>(x), static_cast<int>(y)};
}
json::array xy(Coord c) { return {c.first, c.second}; }

struct Task {
    size_t id;
    uint64_t layer;
    Coord tile;
    uint64_t duration;
    Coord input_channels, output_channels;
    uint64_t input_elements = 0, output_elements = 0;
    size_t remaining = 0;
    std::vector<size_t> outputs;
    enum State { Waiting, Computing, Sending, Done } state = Waiting;
    uint64_t start = 0, finish = 0, release = 0;
};
struct Transfer {
    size_t src, dst;
    uint64_t elements, bytes;
    std::vector<Coord> route;
    bool sent = false, received = false;
    uint64_t send = 0, receive = 0;
};
struct Work {
    std::vector<Task> tasks;
    std::vector<Transfer> transfers;
    uint64_t bandwidth, hop_latency, capacity;
    double clock_ns;
};

using StreamRange = std::pair<uint64_t, uint64_t>;
using LayerChannels = std::map<uint64_t, std::vector<Coord>>;

// floor(elements * index / count), without overflowing the intermediate product.
uint64_t stream_prefix(uint64_t elements, uint64_t index, uint64_t count) {
    return mul(elements / count, index) + mul(elements % count, index) / count;
}

Coord channel_intersection(Coord descriptor, Coord block) {
    Coord range{std::max(descriptor.first, block.first),
                std::min(descriptor.second, block.second)};
    if (range.first > range.second)
        throw std::invalid_argument("Projection channel interval does not intersect its task block");
    return range;
}

StreamRange project_channels(Coord descriptor, Coord block, uint64_t elements) {
    auto range = channel_intersection(descriptor, block);
    const uint64_t count = descriptor.second - descriptor.first + 1;
    return {stream_prefix(elements, range.first - descriptor.first, count),
            stream_prefix(elements, range.second - descriptor.first + 1, count)};
}

LayerChannels collect_layer_channels(const Work& work, bool output) {
    LayerChannels layers;
    for (const auto& task : work.tasks)
        layers[task.layer].push_back(output ? task.output_channels : task.input_channels);
    for (auto& [layer, blocks] : layers) {
        std::sort(blocks.begin(), blocks.end());
        std::vector<Coord> merged;
        for (auto block : blocks) {
            if (!merged.empty() && int64_t(block.first) <= int64_t(merged.back().second) + 1)
                merged.back().second = std::max(merged.back().second, block.second);
            else merged.push_back(block);
        }
        blocks = std::move(merged);
    }
    return layers;
}

bool covered_by_layer(const LayerChannels& layers, uint64_t layer, Coord range) {
    int64_t next = range.first;
    for (auto block : layers.at(layer)) {
        if (block.second < next) continue;
        if (block.first > next) return false;
        next = int64_t(block.second) + 1;
        if (next > range.second) return true;
    }
    return false;
}

void validate_projection(const json::object& projection, const LayerChannels& layer_inputs,
                         const LayerChannels& layer_outputs,
                         const Task& source, const Task& destination,
                         Coord channels, uint64_t elements) {
    const auto target_layer = positive(projection.at("dep_layer"), "projection destination layer", true);
    const auto src_range = coord(projection.at("source_channels"));
    const auto dst_range = coord(projection.at("destination_channels"));
    const auto total = positive(projection.at("elements"), "projection elements", true);
    projection.at("tensor_id").as_string();
    if (target_layer != destination.layer ||
        src_range.first == 0 || src_range.first > src_range.second ||
        dst_range.first == 0 || dst_range.first > dst_range.second ||
        total > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
        !covered_by_layer(layer_outputs, source.layer, src_range) ||
        !covered_by_layer(layer_inputs, destination.layer, dst_range))
        throw std::invalid_argument("Projection descriptor is outside its producer/consumer layer");

    const auto src_stream = project_channels(src_range, source.output_channels, total);
    const auto dst_stream = project_channels(dst_range, destination.input_channels, total);
    const StreamRange expected{std::max(src_stream.first, dst_stream.first),
                               std::min(src_stream.second, dst_stream.second)};
    // An empty slice can carry a control token; mere adjacency of two nonempty
    // slices is not a dependency in the projected element stream.
    if (expected.first > expected.second ||
        (expected.first == expected.second && src_stream.first != src_stream.second &&
         dst_stream.first != dst_stream.second))
        throw std::invalid_argument("Projection task slices do not share an element stream");
    const auto& stream = projection.at("stream_range").as_array();
    if (stream.size() != 2 ||
        positive(stream[0], "stream start", true) != expected.first ||
        positive(stream[1], "stream end", true) != expected.second ||
        elements != expected.second - expected.first)
        throw std::invalid_argument("Projection stream range/volume disagrees with task channel slices");

    auto envelope = channel_intersection(src_range, source.output_channels);
    const uint64_t source_count = src_range.second - src_range.first + 1;
    if (total && total % source_count == 0 && expected.first != expected.second) {
        const uint64_t per_channel = total / source_count;
        envelope.first = std::max(envelope.first,
            src_range.first + static_cast<int>(expected.first / per_channel));
        envelope.second = std::min(envelope.second,
            src_range.first + static_cast<int>((expected.second - 1) / per_channel));
    }
    if (channels != envelope)
        throw std::invalid_argument("Dependency channels disagree with projected source envelope");
}

Work parse(const std::string& input) {
    auto document = json::parse(input);
    const auto& obj = document.as_object();
    if (positive(obj.at("schema_version"), "schema_version") != 1 ||
        obj.at("model").as_string() != "aggregate_tile_v1" ||
        obj.at("time_unit").as_string() != "cycle" ||
        obj.at("data_unit").as_string() != "element")
        throw std::invalid_argument("Unsupported workload schema/model/units");
    const auto& timing = obj.at("timing").as_object();
    const auto& hw = obj.at("hardware").as_object();
    Coord shape = coord(hw.at("tile_shape"));
    if (shape.first <= 0 || shape.second <= 0) throw std::invalid_argument("Empty hardware mesh");
    const uint64_t xbars = positive(hw.at("xbars_per_tile"), "xbars_per_tile");
    positive(hw.at("pipeline_depth"), "pipeline_depth");
    auto xbar_shape = coord(hw.at("xbar_shape"));
    if (xbar_shape.first == 0 || xbar_shape.second == 0) throw std::invalid_argument("Empty crossbar");
    const uint64_t bits = positive(timing.at("element_bits"), "element_bits");
    Work w;
    w.bandwidth = positive(timing.at("link_bytes_per_cycle"), "link_bytes_per_cycle");
    w.hop_latency = positive(timing.at("hop_latency_cycles"), "hop_latency_cycles", true);
    w.capacity = positive(timing.at("network_capacity"), "network_capacity");
    w.clock_ns = timing.at("clock_period_ns").to_number<double>();
    if (!(w.clock_ns > 0) || !std::isfinite(w.clock_ns))
        throw std::invalid_argument("clock_period_ns must be finite and positive");
    auto in_mesh = [shape](Coord c) { return c.first < shape.first && c.second < shape.second; };
    std::set<std::pair<Coord, uint64_t>> slots;
    for (const auto& value : obj.at("tasks").as_array()) {
        const auto& t = value.as_object();
        const auto id = positive(t.at("id"), "task id", true);
        if (id != w.tasks.size()) throw std::invalid_argument("Task IDs must be dense and ordered from zero");
        auto tile = coord(t.at("tile"));
        auto slot = positive(t.at("xbar_slot"), "xbar_slot", true);
        if (!in_mesh(tile) || slot >= xbars || !slots.insert({tile, slot}).second)
            throw std::invalid_argument("Task allocation exceeds hardware or repeats a crossbar slot");
        const auto layer = positive(t.at("layer"), "layer", true);
        for (const char* field : {"input_channels", "output_channels"}) {
            auto range = coord(t.at(field));
            if (range.first == 0 || range.second < range.first)
                throw std::invalid_argument("Channel ranges are positive, inclusive intervals");
        }
        auto input_elements = positive(t.at("ifmap_elements"), "ifmap_elements");
        auto output_elements = positive(t.at("ofmap_elements"), "ofmap_elements");
        auto ci = coord(t.at("input_channels")), co = coord(t.at("output_channels"));
        const uint64_t input_count = ci.second-ci.first+1, output_count = co.second-co.first+1;
        auto kernel = coord(t.at("kernel_shape"));
        if (kernel.first == 0 || kernel.second == 0 ||
            mul(input_count, mul(kernel.first,kernel.second)) > static_cast<uint64_t>(xbar_shape.first) ||
            output_count > static_cast<uint64_t>(xbar_shape.second) ||
            input_elements % input_count || output_elements % output_count)
            throw std::invalid_argument("Task block dimensions disagree with crossbar capacity or feature-map counts");
        Task task{};
        task.id = id; task.layer = layer; task.tile = tile;
        task.input_channels = ci; task.output_channels = co;
        task.input_elements = input_elements; task.output_elements = output_elements;
        task.duration = positive(t.at("duration_cycles"), "duration_cycles", true);
        w.tasks.push_back(std::move(task));
    }
    const auto layer_inputs = collect_layer_channels(w, false);
    const auto layer_outputs = collect_layer_channels(w, true);
    for (const auto& value : obj.at("dependencies").as_array()) {
        const auto& e = value.as_object();
        auto src = positive(e.at("src"), "source", true), dst = positive(e.at("dst"), "destination", true);
        if (src >= w.tasks.size() || dst >= w.tasks.size() || src == dst)
            throw std::invalid_argument("Invalid dependency endpoint/self loop");
        auto kind = e.at("kind").as_string();
        if (kind != "accumulation" && kind != "propagation")
            throw std::invalid_argument("Unknown dependency kind");
        Transfer transfer{};
        transfer.src = src; transfer.dst = dst;
        transfer.elements = positive(e.at("elements"), "elements", true);
        auto channels = coord(e.at("channels"));
        const auto& source = w.tasks[src];
        const auto& destination = w.tasks[dst];
        if (auto projection = e.if_contains("projection")) {
            if (kind != "propagation")
                throw std::invalid_argument("Projection metadata is only valid for propagation dependencies");
            validate_projection(projection->as_object(), layer_inputs, layer_outputs,
                                source, destination, channels, transfer.elements);
        } else {
            auto destination_channels = kind == "accumulation" ? destination.output_channels : destination.input_channels;
            if (channels.first == 0 || channels.second < channels.first ||
                channels.first < source.output_channels.first || channels.second > source.output_channels.second ||
                channels.first < destination_channels.first || channels.second > destination_channels.second)
                throw std::invalid_argument("Dependency channel interval is outside producer/consumer block");
            auto expected_elements = mul(source.output_elements / (source.output_channels.second-source.output_channels.first+1),
                                         channels.second-channels.first+1);
            if (transfer.elements && transfer.elements != expected_elements)
                throw std::invalid_argument("Dependency volume disagrees with producer block/channel interval");
            const uint64_t destination_elements = kind == "accumulation"
                ? destination.output_elements : destination.input_elements;
            if (transfer.elements && source.output_elements /
                    (source.output_channels.second-source.output_channels.first+1) !=
                    destination_elements / (destination_channels.second-destination_channels.first+1))
                throw std::invalid_argument("Dependency producer/consumer spatial extents disagree");
        }
        transfer.bytes = bytes_for(transfer.elements, bits);
        if (positive(e.at("bytes"), "bytes", true) != transfer.bytes)
            throw std::invalid_argument("Dependency byte count disagrees with element count and bitwidth");
        for (const auto& hop : e.at("route").as_array()) {
            auto c = coord(hop);
            if (!in_mesh(c)) throw std::invalid_argument("Route outside hardware mesh");
            if (!transfer.route.empty()) {
                auto prev = transfer.route.back();
                if (std::abs(int64_t(prev.first)-c.first) + std::abs(int64_t(prev.second)-c.second) != 1)
                    throw std::invalid_argument("Route contains non-neighbor hop");
            }
            transfer.route.push_back(c);
        }
        if (transfer.route.empty() || transfer.route.front() != w.tasks[src].tile ||
            transfer.route.back() != w.tasks[dst].tile)
            throw std::invalid_argument("Route endpoints disagree with task allocation");
        std::set<Coord> unique(transfer.route.begin(), transfer.route.end());
        if (unique.size() != transfer.route.size()) throw std::invalid_argument("Route contains cycle");
        w.tasks[src].outputs.push_back(w.transfers.size());
        ++w.tasks[dst].remaining;
        w.transfers.push_back(std::move(transfer));
    }
    std::vector<size_t> degree;
    std::queue<size_t> ready;
    for (const auto& task : w.tasks) {
        degree.push_back(task.remaining);
        if (!task.remaining) ready.push(task.id);
    }
    size_t visited = 0;
    while (!ready.empty()) {
        auto id = ready.front(); ready.pop(); ++visited;
        for (auto ei : w.tasks[id].outputs) if (--degree[w.transfers[ei].dst] == 0) ready.push(w.transfers[ei].dst);
    }
    if (visited != w.tasks.size()) throw std::invalid_argument("Workload dependency graph contains a cycle");
    return w;
}

class WorkloadModule : public ModuleBase {
public:
    WorkloadModule(const std::string& id, Work w) : ModuleBase(id), work(std::move(w)) {}
    void register_processes() override {}
    void register_message_handlers() override {}
    Work work;
    uint64_t completed = 0, delivered = 0, in_flight = 0, peak_in_flight = 0;
    uint64_t sent_bytes = 0, received_bytes = 0;
    std::map<Coord, size_t> owners;
    std::map<std::pair<Coord, Coord>, uint64_t> link_free;
    bool complete() const { return completed == work.tasks.size() && delivered == work.transfers.size(); }
    void finish(size_t id, uint64_t cycle) {
        auto& task = work.tasks.at(id);
        if (task.state != Task::Computing) throw std::logic_error("Duplicate computation completion");
        task.finish = cycle; task.state = Task::Sending;
    }
    void receive(size_t ei) {
        auto& edge = work.transfers.at(ei);
        if (!edge.sent || edge.received) throw std::logic_error("Duplicate/unmatched receive");
        edge.received = true; ++delivered;
        received_bytes = add(received_bytes, edge.bytes);
        if (edge.route.size() > 1 && edge.bytes) --in_flight;
        auto& remaining = work.tasks[edge.dst].remaining;
        if (!remaining) throw std::logic_error("Dependency token underflow");
        --remaining;
    }
    std::vector<SimulatorEvent> evaluate(uint64_t cycle) override {
        for (auto& task : work.tasks) {
            if (task.state != Task::Sending) continue;
            bool all_sent = true;
            for (auto ei : task.outputs) {
                auto& edge = work.transfers[ei];
                if (edge.sent) continue;
                const bool remote = edge.route.size() > 1 && edge.bytes;
                if (remote && in_flight >= work.capacity) { all_sent = false; break; }
                edge.sent = true; edge.send = cycle; edge.receive = cycle;
                sent_bytes = add(sent_bytes, edge.bytes);
                if (remote) {
                    auto serialization = edge.bytes / work.bandwidth + (edge.bytes % work.bandwidth != 0);
                    for (size_t h = 1; h < edge.route.size(); ++h) {
                        auto key = std::make_pair(edge.route[h-1], edge.route[h]);
                        auto start = std::max(edge.receive, link_free[key]);
                        link_free[key] = add(start, serialization);
                        edge.receive = add(link_free[key], work.hop_latency);
                    }
                    ++in_flight; peak_in_flight = std::max(peak_in_flight, in_flight);
                    submit_message("workload.receive", ei, edge.receive - cycle);
                } else receive(ei);
            }
            if (all_sent) { task.release = cycle; task.state = Task::Done; owners.erase(task.tile); ++completed; }
        }
        // Stable ID arbitration; one active computation/output owner per tile.
        for (auto& task : work.tasks) {
            if (task.state != Task::Waiting || task.remaining || owners.count(task.tile)) continue;
            owners.emplace(task.tile, task.id); task.state = Task::Computing; task.start = cycle;
            submit_message("workload.finish", task.id, task.duration);
        }
        return std::exchange(pending_events_, {});
    }
};
} // namespace

std::string export_workload(const std::vector<NNkernel>& kernels, const HWInfo& hw,
                            const OptInfo& opt, const std::string& timing_json) {
    auto timing_value = json::parse(timing_json);
    const auto& timing = timing_value.as_object();
    const auto& durations = timing.at("layer_cycles").as_object();
    auto bits = positive(timing.at("element_bits"), "element_bits");
    Analyzer analyzer(kernels, hw, opt);
    const auto& cg = analyzer.cgraph().get_graph();
    const auto& tg = analyzer.tgraph().get_graph();
    json::array tasks, edges;
    std::vector<size_t> tiles(boost::num_vertices(cg)), slots(tiles.size());
    std::map<int,Coord> kernel_shapes;
    for (const auto& kernel : kernels) kernel_shapes.emplace(kernel.layer,kernel.wsize);
    for (auto tv : boost::make_iterator_range(boost::vertices(tg))) {
        size_t slot = 0;
        for (auto cv : tg[tv].cnode_id) { tiles.at(cv) = tv; slots.at(cv) = slot++; }
    }
    for (auto cv : boost::make_iterator_range(boost::vertices(cg))) {
        const auto& node = cg[cv];
        std::string layer = std::to_string(node.layer);
        if (!durations.contains(layer)) throw std::invalid_argument("Missing explicit duration for layer " + layer);
        auto duration = positive(durations.at(layer), "layer duration", true);
        tasks.push_back(json::object{{"id", cv}, {"layer", node.layer},
            {"tile", xy(analyzer.hgraph().get_hnode(tiles[cv]))}, {"xbar_slot", slots[cv]},
            {"input_channels", xy(node.id_cin)}, {"output_channels", xy(node.id_cout)},
            {"ifmap_elements", node.ifmap_size}, {"ofmap_elements", node.ofmap_size},
            {"kernel_shape", xy(kernel_shapes.at(node.layer))},
            {"duration_cycles", duration}});
    }
    for (auto ce : boost::make_iterator_range(boost::edges(cg))) {
        auto src = boost::source(ce,cg), dst = boost::target(ce,cg);
        auto a = analyzer.hgraph().get_hnode(tiles[src]), b = analyzer.hgraph().get_hnode(tiles[dst]);
        std::vector<Coord> route;
        if (a == b) route.push_back(a);
        else {
            for (const auto& [layer, path_id] : analyzer.dgraph().get_path_map(tiles[src])) {
                if (layer != cg[src].layer) continue;
                auto paths = analyzer.dgraph().get_pathset({path_id});
                if (paths[0]->src == a && paths[0]->dst == b) { route = paths[0]->via; break; }
            }
            if (route.empty()) throw std::runtime_error("Mapped dependency has no matching DGraph route");
        }
        json::array hops;
        for (auto c : route) hops.push_back(xy(c));
        if (cg[ce].datavolume < 0) throw std::invalid_argument("Negative dependency volume");
        json::object dependency{{"src", src}, {"dst", dst},
            {"kind", cg[ce].c_type == DepType::Accum ? "accumulation" : "propagation"},
            {"channels", xy(cg[ce].channel_id)}, {"elements", cg[ce].datavolume},
            {"bytes", bytes_for(cg[ce].datavolume,bits)}, {"route", std::move(hops)}};
        if (cg[ce].c_type == DepType::Prop) {
            const auto& projection = cg[ce].projection;
            dependency["projection"] = json::object{
                {"dep_layer", projection.dep_layer},
                {"source_channels", xy(projection.dep_chan)},
                {"destination_channels", xy(projection.dst_chan)},
                {"elements", projection.elements}, {"tensor_id", projection.tensor_id},
                {"stream_range", json::array{cg[ce].stream_range.first, cg[ce].stream_range.second}}};
        }
        edges.push_back(std::move(dependency));
    }
    json::object result{{"schema_version",1}, {"model","aggregate_tile_v1"},
        {"time_unit","cycle"}, {"data_unit","element"},
        {"hardware",json::object{{"tile_shape",xy(analyzer.hgraph().get_shape())}, {"xbar_shape",xy(hw.xbar_size)},
                                 {"xbars_per_tile",hw.xbar_num}, {"pipeline_depth",hw.pipeline_depth}}},
        {"strategy",json::object{{"mapping_opt",opt.mapping_opt},{"sched_opt",opt.sched_opt}}},
        {"timing",timing}, {"tasks",std::move(tasks)}, {"dependencies",std::move(edges)}};
    auto output = json::serialize(result);
    parse(output); // Export and import share the same invariant checks.
    return output;
}

std::string simulate_workload(const std::string& workload_json, uint64_t max_cycles) {
    auto work = parse(workload_json);
    if (!max_cycles) throw std::invalid_argument("max_cycles must be positive");
    CycleAccurateSimulator simulator(max_cycles);
    simulator.set_max_delta_steps(add(2, mul(2, add(work.tasks.size(),work.transfers.size()))));
    auto module = simulator.register_module<WorkloadModule>("workload", 0, std::move(work));
    simulator.register_task_handler<size_t>("workload.finish", [&simulator,module](const size_t& id) {
        module->finish(id, simulator.get_current_cycle());
    });
    simulator.register_task_handler<size_t>("workload.receive", [module](const size_t& id) { module->receive(id); });
    simulator.set_completion_condition([module] { return module->complete(); });
    simulator.run();
    json::array tasks, transfers;
    std::map<Coord,uint64_t> busy;
    uint64_t makespan = 0;
    const uint64_t observed_until = simulator.get_total_cycles();
    for (const auto& task : module->work.tasks) {
        const bool done = task.state == Task::Done;
        static constexpr const char* names[] = {"Waiting", "Computing", "Sending", "Done"};
        tasks.push_back(json::object{{"id",task.id},{"completed",done},{"state",names[task.state]},
            {"start_cycle",task.state == Task::Waiting ? json::value(nullptr) : json::value(task.start)},
            {"finish_cycle",task.state < Task::Sending ? json::value(nullptr) : json::value(task.finish)},
            {"release_cycle",done ? json::value(task.release) : json::value(nullptr)}});
        if (done) makespan = std::max(makespan,task.release);
        if (task.state != Task::Waiting) busy[task.tile] = add(busy[task.tile],
            (done ? task.release : observed_until) - task.start);
    }
    for (const auto& edge : module->work.transfers) {
        transfers.push_back(json::object{{"src",edge.src},{"dst",edge.dst},{"bytes",edge.bytes},
            {"sent",edge.sent},{"received",edge.received},
            {"send_cycle",edge.sent ? json::value(edge.send) : json::value(nullptr)},
            {"receive_cycle",edge.received ? json::value(edge.receive) : json::value(nullptr)},
            {"scheduled_receive_cycle",edge.sent ? json::value(edge.receive) : json::value(nullptr)}});
        if (edge.received) makespan = std::max(makespan,edge.receive);
    }
    json::array resources;
    for (const auto& [tile,cycles] : busy) {
        json::object resource{{"tile",xy(tile)}, {"busy_cycles",cycles}};
        resource["utilization"] = simulator.is_simulation_done()
            ? json::value(makespan ? double(cycles)/makespan : 0.0) : json::value(nullptr);
        resources.push_back(std::move(resource));
    }
    json::object result{{"schema_version",1},{"model","aggregate_tile_v1"},
        {"completed",simulator.is_simulation_done()},{"stop_reason",simulator.get_stop_reason_name()},
        {"elapsed_cycles",simulator.get_total_cycles()}, {"completed_tasks",module->completed},
        {"total_tasks",module->work.tasks.size()}, {"delivered_dependencies",module->delivered},
        {"sent_bytes",module->sent_bytes},{"received_bytes",module->received_bytes},
        {"peak_network_transfers",module->peak_in_flight},{"tasks",std::move(tasks)},
        {"transfers",std::move(transfers)},{"resources",std::move(resources)}};
    if (simulator.is_simulation_done()) {
        result["makespan_cycles"] = makespan;
        const double latency = makespan * module->work.clock_ns;
        if (!std::isfinite(latency)) throw std::overflow_error("Latency in ns exceeds numeric range");
        result["latency_ns"] = latency;
    } else { result["makespan_cycles"] = nullptr; result["latency_ns"] = nullptr; }
    return json::serialize(result);
}
} // namespace pim
