#include "scheduler.h"
#include <cmath>

Scheduler::Scheduler(std::pair<int, int> tile_size)
    : tile_size(tile_size) {
    if (tile_size.first <= 0 || tile_size.second <= 0) throw pim::ConfigError("mesh dimensions must be positive");
    // init graph
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            boost::add_vertex(SNode{i, j}, graph);
        }
    }
    // 2D mesh connection & init edge_map
    size_t id = 0;
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            size_t src, dst;
            if (i > 0) {
                src = i * tile_size.second + j;
                dst = (i - 1) * tile_size.second + j;
                boost::add_edge(src, dst, graph);
                edge_map[UnorderedPair{src, dst}] = id++;
            }
            if (j > 0) {
                src = i * tile_size.second + j;
                dst = i * tile_size.second + j - 1;
                boost::add_edge(src, dst, graph);
                edge_map[UnorderedPair{src, dst}] = id++;
            }
        }
    }
}

void Scheduler::set_path_set(std::vector<std::shared_ptr<Path>> paths) {
    auto valid = [&](auto point) {
        return point.first >= 0 && point.second >= 0 &&
               point.first < tile_size.first && point.second < tile_size.second;
    };
    std::set<size_t> ids;
    for (const auto& path : paths) {
        if (!path || !valid(path->src) || !valid(path->dst) || path->datavolume < 0 ||
            !ids.insert(path->id).second) {
            throw pim::SchedulingError("paths require unique ids, valid mesh endpoints and nonnegative volume");
        }
    }
    path_set = std::move(paths);
}

long long Scheduler::schedule() {
    set_path_set(path_set); // callers retain mutable Path pointers
    init_bce();
    congestion_aware_routing();
    long long total = 0;
    for (const auto& [_, value] : congestion_map) total += value.getCSum();
    return total;
}

long long Scheduler::xy_routing() {
    set_path_set(path_set);
    congestion_map.clear();
    for (const auto& path : path_set) {
        path->via = xy_init(path->src, path->dst);
        if (path->datavolume == 0) continue;
        for (size_t i = 0; i + 1 < path->via.size(); ++i) {
            auto src = xy_to_id(path->via[i]);
            auto dst = xy_to_id(path->via[i + 1]);
            congestion_map[edge_map.at(UnorderedPair{src, dst})].insert(path->datavolume);
        }
    }
    long long total = 0;
    for (const auto& [_, value] : congestion_map) total += value.getCSum();
    return total;
}

void Scheduler::init_bce() {
    bce_map.clear();
    congestion_map.clear();
    // Compute ratios in log space: integer shortest-path counts overflow on
    // ordinary larger meshes even when the final EBC is small.
    auto log_paths = [](auto source, auto destination) {
        const long double dx = std::abs(static_cast<long double>(source.first) - destination.first);
        const long double dy = std::abs(static_cast<long double>(source.second) - destination.second);
        return std::lgamma(dx + dy + 1) - std::lgamma(dx + 1) - std::lgamma(dy + 1);
    };
    for (const auto& path : path_set) {
        if (path->datavolume == 0 || path->src == path->dst) continue;
        const auto distance = manhattan_distance(path->src, path->dst);
        const auto denominator = log_paths(path->src, path->dst);
        for (const auto& edge : boost::make_iterator_range(edges(graph))) {
            auto first = boost::source(edge, graph);
            auto second = boost::target(edge, graph);
            auto a = id_to_xy(first);
            auto b = id_to_xy(second);
            if (manhattan_distance(path->src, a) > manhattan_distance(path->src, b)) {
                std::swap(a, b);
            }
            if (manhattan_distance(path->src, a) + 1 + manhattan_distance(b, path->dst) != distance) continue;
            auto ratio = std::exp(log_paths(path->src, a) + log_paths(b, path->dst) - denominator);
            bce_map[edge_map.at(UnorderedPair{first, second})] += static_cast<double>(ratio * path->datavolume);
        }
    }
    if (bce_map.empty()) return;
    const double maximum = std::max_element(bce_map.begin(), bce_map.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; })->second;
    if (!(maximum > 0) || !std::isfinite(maximum)) {
        throw pim::SchedulingError("invalid edge betweenness normalization");
    }
    for (auto& [_, value] : bce_map) value /= maximum;
}

void Scheduler::congestion_aware_routing() {
    std::sort(path_set.begin(), path_set.end(), [](const auto& a, const auto& b) {
        auto da = manhattan_distance(a->src, a->dst);
        auto db = manhattan_distance(b->src, b->dst);
        if (da != db) return da < db;
        if (a->datavolume != b->datavolume) return a->datavolume > b->datavolume;
        return a->id < b->id;
    });
    for (auto& path : path_set) {
        if (path->datavolume == 0 || path->src == path->dst) {
            path->via = xy_init(path->src, path->dst);
            continue;
        }
        // Restrict transitions to Manhattan-distance-decreasing edges. A
        // bounding rectangle alone permits detours and does not ensure the
        // shortest-hop constraint of section 3.3.3.
        const auto src = xy_to_id(path->src);
        const auto dst = xy_to_id(path->dst);
        const auto count = boost::num_vertices(graph);
        std::vector<double> cost(count, std::numeric_limits<double>::infinity());
        std::vector<size_t> previous(count, count);
        std::vector<size_t> order;
        for (int x = std::min(path->src.first, path->dst.first); x <= std::max(path->src.first, path->dst.first); ++x) {
            for (int y = std::min(path->src.second, path->dst.second); y <= std::max(path->src.second, path->dst.second); ++y) {
                order.push_back(xy_to_id({x, y}));
            }
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            auto da = manhattan_distance(path->src, id_to_xy(a));
            auto db = manhattan_distance(path->src, id_to_xy(b));
            return da != db ? da < db : a < b;
        });
        cost[src] = 0;
        for (auto vertex : order) {
            if (!std::isfinite(cost[vertex])) continue;
            auto point = id_to_xy(vertex);
            for (auto edge : boost::make_iterator_range(out_edges(vertex, graph))) {
                auto next = boost::target(edge, graph);
                auto next_point = id_to_xy(next);
                if (manhattan_distance(next_point, path->dst) + 1 != manhattan_distance(point, path->dst)) continue;
                auto edge_id = edge_map.at(UnorderedPair{vertex, next});
                auto load = congestion_map.find(edge_id);
                long long congestion = load == congestion_map.end() ? 1 : load->second.getCSum(path->datavolume);
                double candidate = cost[vertex] + bce_map.at(edge_id) * congestion;
                if (candidate < cost[next] || (candidate == cost[next] && vertex < previous[next])) {
                    cost[next] = candidate;
                    previous[next] = vertex;
                }
            }
        }
        if (!std::isfinite(cost[dst])) throw pim::SchedulingError("no shortest route available");
        std::vector<std::pair<int, int>> route;
        for (auto vertex = dst; vertex != src; vertex = previous[vertex]) {
            if (previous[vertex] == count) throw pim::SchedulingError("broken route predecessor");
            route.push_back(id_to_xy(vertex));
            congestion_map[edge_map.at(UnorderedPair{previous[vertex], vertex})].insert(path->datavolume);
        }
        route.push_back(path->src);
        std::reverse(route.begin(), route.end());
        path->via = std::move(route);
    }
}
