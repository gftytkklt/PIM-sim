#include <gtest/gtest.h>
#include <boost/json.hpp>
#include <set>
#include <tuple>
#include "simulator/Workload.h"

namespace {
namespace json = boost::json;
std::string timing = R"({"layer_cycles":{"0":2,"1":3},"element_bits":8,"link_bytes_per_cycle":1,"hop_latency_cycles":1,"network_capacity":1,"clock_period_ns":2.0})";
std::vector<NNkernel> chain() {
    return {{0,{1,1},{1,1},{{1,{1,1}}},{1,1},{1,1}},
            {1,{1,1},{1,1},{{-1,{0,0}}},{1,1},{1,1}}};
}
json::object workload(bool same_tile = false) {
    return json::parse(pim::export_workload(chain(), {{1,1},same_tile?2:1,{1,2},1},
                                          {same_tile,same_tile},timing)).as_object();
}
json::object simulate(json::object w, uint64_t max_cycles = 100) {
    return json::parse(pim::simulate_workload(json::serialize(w),max_cycles)).as_object();
}
json::object projected_workload(const std::vector<NNkernel>& kernels, HWInfo hardware,
                               bool same_tile = false, const std::string& durations = timing) {
    return json::parse(pim::export_workload(kernels, hardware, {same_tile, same_tile}, durations)).as_object();
}
}

TEST(Workload, ActualMappingToEngineHasHandCalculatedTimeline) {
    auto w = workload();
    ASSERT_EQ(w.at("tasks").as_array().size(),2u);
    ASSERT_EQ(w.at("dependencies").as_array().size(),1u);
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("stop_reason").as_string(),"Completed");
    // task0 [0,2], one byte / one byte per cycle + one-hop latency: [2,4], task1 [4,7].
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),7);
    EXPECT_DOUBLE_EQ(result.at("latency_ns").to_number<double>(),14.0);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),1);
    EXPECT_EQ(result.at("received_bytes").to_number<int>(),1);
    const auto& tasks=result.at("tasks").as_array();
    EXPECT_EQ(tasks[0].at("finish_cycle").to_number<int>(),2);
    EXPECT_EQ(tasks[1].at("start_cycle").to_number<int>(),4);
    EXPECT_EQ(result.at("peak_network_transfers").to_number<int>(),1);
    EXPECT_EQ(json::serialize(result),json::serialize(simulate(w)));
}

TEST(Workload, LocalDependencyIsRetainedAndConsumesNoNetworkSlot) {
    auto result=simulate(workload(true));
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),5);
    EXPECT_EQ(result.at("delivered_dependencies").to_number<int>(),1);
    EXPECT_EQ(result.at("peak_network_transfers").to_number<int>(),0);
    EXPECT_EQ(result.at("sent_bytes"),result.at("received_bytes"));
}

TEST(Workload, BoundedAdmissionRetainsProducerUntilAllOutputsAreSent) {
    auto w=workload();
    // Parallel dependencies are two independent transfers, not one overwritten edge.
    w.at("dependencies").as_array().push_back(w.at("dependencies").as_array().front());
    auto result=simulate(w);
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),9);
    EXPECT_EQ(result.at("tasks").as_array()[0].at("release_cycle").to_number<int>(),4);
    EXPECT_EQ(result.at("tasks").as_array()[1].at("start_cycle").to_number<int>(),6);
    EXPECT_EQ(result.at("peak_network_transfers").to_number<int>(),1);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),2);
    EXPECT_EQ(result.at("received_bytes").to_number<int>(),2);
}

TEST(Workload, MaxCyclesDoesNotExportSuccessLatency) {
    auto result=simulate(workload(),3);
    EXPECT_FALSE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("stop_reason").as_string(),"MaxCycles");
    EXPECT_TRUE(result.at("makespan_cycles").is_null());
    EXPECT_TRUE(result.at("latency_ns").is_null());
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),1);
    EXPECT_EQ(result.at("received_bytes").to_number<int>(),0);
    EXPECT_TRUE(result.at("tasks").as_array()[1].at("start_cycle").is_null());
    EXPECT_TRUE(result.at("transfers").as_array()[0].at("receive_cycle").is_null());
}

TEST(Workload, ZeroDurationAndEmptyWorkAreWellDefined) {
    auto w=workload(true);
    for(auto& task:w.at("tasks").as_array()) task.as_object()["duration_cycles"]=0;
    auto result=simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),0);
    w["tasks"]=json::array{}; w["dependencies"]=json::array{};
    result=simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("total_tasks").to_number<int>(),0);
}

TEST(Workload, RejectsCorruptUnitsVolumesRoutesAndCycles) {
    auto w=workload(); w["time_unit"]="ns";
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w=workload(); w.at("dependencies").as_array()[0].as_object()["bytes"]=2;
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w=workload(); w.at("dependencies").as_array()[0].as_object()["channels"]=json::array{0,1};
    EXPECT_THROW(simulate(w),std::invalid_argument);
    // Legacy channels-only documents still require matching producer/consumer spatial extents.
    w=workload(); w.at("dependencies").as_array()[0].as_object().erase("projection");
    w.at("tasks").as_array()[1].as_object()["ifmap_elements"]=100;
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w=workload(); w.at("tasks").as_array()[1].as_object()["xbar_slot"]=1;
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w=workload(); w.at("dependencies").as_array()[0].as_object()["route"]=json::array{json::array{0,0},json::array{0,2}};
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w=workload(); auto e=w.at("dependencies").as_array()[0].as_object();
    e["src"]=1; e["dst"]=0;
    auto& route=e.at("route").as_array(); std::reverse(route.begin(),route.end());
    w.at("dependencies").as_array().push_back(e);
    EXPECT_THROW(simulate(w),std::invalid_argument);
    EXPECT_THROW(simulate(workload(),0),std::invalid_argument);
}

TEST(Workload, PoolProjectionKeepsMvmShapeAndTransfersReducedVolume) {
    std::vector<NNkernel> kernels{
        {0,{1,1},{1,2},{{1,{1,2},{1,2},8,"pool"}},{4,4},{4,4}},
        {1,{1,1},{2,1},{{-1,{0,0}}},{2,2},{2,2}}};
    auto w = projected_workload(kernels, {{2,2},1,{1,2},1});
    EXPECT_EQ(w.at("tasks").as_array()[0].at("ofmap_elements").to_number<int>(),32);
    EXPECT_EQ(w.at("tasks").as_array()[1].at("ifmap_elements").to_number<int>(),8);
    ASSERT_EQ(w.at("dependencies").as_array().size(),1u);
    const auto& dependency = w.at("dependencies").as_array()[0];
    EXPECT_EQ(dependency.at("elements").to_number<int>(),8);
    EXPECT_EQ(dependency.at("projection").at("tensor_id").as_string(),"pool");
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    // Compute [0,2], eight serialized bytes plus one hop [2,11], compute [11,14].
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),14);
    EXPECT_EQ(result.at("tasks").as_array()[1].at("start_cycle").to_number<int>(),11);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),8);
    EXPECT_EQ(result.at("received_bytes"),result.at("sent_bytes"));
    EXPECT_EQ(result,simulate(w));
}

TEST(Workload, ConcatProjectionPreservesDestinationChannelOffsets) {
    std::vector<NNkernel> kernels{
        {0,{1,1},{1,1},{{2,{1,1},{1,1},1,"concat-a"}},{1,1},{1,1}},
        {1,{1,1},{1,1},{{2,{1,1},{2,2},1,"concat-b"}},{1,1},{1,1}},
        {2,{1,1},{2,1},{{-1,{0,0}}},{1,1},{1,1}}};
    auto times = json::parse(timing).as_object();
    times.at("layer_cycles").as_object()["1"] = 2;
    times.at("layer_cycles").as_object()["2"] = 3;
    auto w = projected_workload(kernels, {{2,1},3,{1,1},1},true,json::serialize(times));
    ASSERT_EQ(w.at("dependencies").as_array().size(),2u);
    for (const auto& dependency : w.at("dependencies").as_array()) {
        const auto source = dependency.at("src").to_number<int>();
        EXPECT_EQ(dependency.at("channels"),json::value(json::array{1,1}));
        EXPECT_EQ(dependency.at("projection").at("destination_channels"),
                  json::value(json::array{source+1,source+1}));
    }
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),7);
    EXPECT_EQ(result.at("delivered_dependencies").to_number<int>(),2);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),2);
    EXPECT_EQ(result.at("received_bytes"),result.at("sent_bytes"));
    EXPECT_EQ(result.at("peak_network_transfers").to_number<int>(),0);
}

TEST(Workload, FlattenProjectionMapsSpatialElementsToInputChannels) {
    std::vector<NNkernel> kernels{
        {0,{1,1},{1,2},{{1,{1,2},{1,8},8,"flatten"}},{2,2},{2,2}},
        {1,{1,1},{8,1},{{-1,{0,0}}},{1,1},{1,1}}};
    auto w = projected_workload(kernels, {{8,2},1,{1,2},1});
    const auto& projection = w.at("dependencies").as_array()[0].at("projection");
    EXPECT_EQ(projection.at("source_channels"),json::value(json::array{1,2}));
    EXPECT_EQ(projection.at("destination_channels"),json::value(json::array{1,8}));
    EXPECT_EQ(projection.at("stream_range"),json::value(json::array{0,8}));
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),14);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),8);
    EXPECT_EQ(result.at("received_bytes"),result.at("sent_bytes"));
}

TEST(Workload, FlattenAcrossChannelBlocksConservesThreeOneTwoTwoElements) {
    std::vector<NNkernel> kernels{
        {0,{1,1},{1,2},{{1,{1,2},{1,8},8,"flatten"}},{2,2},{2,2}},
        {1,{1,1},{8,1},{{-1,{0,0}}},{1,1},{1,1}}};
    auto w = projected_workload(kernels, {{3,1},5,{1,1},1},true);
    ASSERT_EQ(w.at("tasks").as_array().size(),5u);
    std::set<std::tuple<int,int,int,int,int>> propagation;
    int accumulated_elements = 0;
    for (const auto& dependency : w.at("dependencies").as_array()) {
        const auto elements = dependency.at("elements").to_number<int>();
        if (dependency.at("kind").as_string() == "accumulation") {
            EXPECT_FALSE(dependency.as_object().contains("projection"));
            accumulated_elements += elements;
        } else {
            const auto& range = dependency.at("projection").at("stream_range").as_array();
            propagation.emplace(dependency.at("src").to_number<int>(),dependency.at("dst").to_number<int>(),
                                elements,range[0].to_number<int>(),range[1].to_number<int>());
        }
    }
    const std::set<std::tuple<int,int,int,int,int>> expected{
        {0,2,3,0,3},{0,3,1,3,4},{1,3,2,4,6},{1,4,2,6,8}};
    EXPECT_EQ(propagation,expected);
    EXPECT_EQ(accumulated_elements,2);
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("completed_tasks").to_number<int>(),5);
    EXPECT_EQ(result.at("delivered_dependencies").to_number<int>(),6);
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),13);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),10);
    EXPECT_EQ(result.at("received_bytes"),result.at("sent_bytes"));
}

TEST(Workload, NonuniformProjectionUsesIntegerPrefixAndExplicitVolume) {
    std::vector<NNkernel> kernels{
        {0,{1,1},{1,3},{{1,{1,3},{1,2},5,"nonuniform"}},{1,1},{1,1}},
        {1,{1,1},{2,1},{{-1,{0,0}}},{1,1},{1,1}}};
    auto w = projected_workload(kernels, {{1,3},3,{1,1},1},true);
    std::set<std::pair<int,int>> ranges;
    for (const auto& dependency : w.at("dependencies").as_array()) {
        if (dependency.at("kind").as_string() != "propagation") continue;
        const auto& stream = dependency.at("projection").at("stream_range").as_array();
        ranges.emplace(stream[0].to_number<int>(),stream[1].to_number<int>());
        EXPECT_EQ(dependency.at("channels"),json::value(json::array{1,3}));
    }
    const std::set<std::pair<int,int>> expected{{0,2},{2,5}};
    EXPECT_EQ(ranges,expected);
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),8);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),6);
    EXPECT_EQ(result.at("received_bytes"),result.at("sent_bytes"));
}

TEST(Workload, ProjectedZeroVolumeStillDeliversControlDependency) {
    auto w = workload();
    auto& dependency = w.at("dependencies").as_array()[0].as_object();
    dependency["elements"] = 0;
    dependency["bytes"] = 0;
    dependency.at("projection").as_object()["elements"] = 0;
    dependency.at("projection").as_object()["stream_range"] = json::array{0,0};
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),5);
    EXPECT_EQ(result.at("delivered_dependencies").to_number<int>(),1);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(),0);
    EXPECT_EQ(result.at("peak_network_transfers").to_number<int>(),0);
}

TEST(Workload, LegacyChannelsOnlyDocumentRetainsStrictValidation) {
    auto w = workload();
    w.at("dependencies").as_array()[0].as_object().erase("projection");
    auto result = simulate(w);
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("makespan_cycles").to_number<int>(),7);
    w.at("tasks").as_array()[1].as_object()["ifmap_elements"] = 2;
    EXPECT_THROW(simulate(w),std::invalid_argument);
}

TEST(Workload, RejectsForgedProjectionDescriptorSlicesAndEnvelope) {
    auto reject = [](const char* key, json::value value) {
        auto w = workload();
        w.at("dependencies").as_array()[0].at("projection").as_object()[key] = std::move(value);
        EXPECT_THROW(simulate(w),std::invalid_argument) << key;
    };
    reject("dep_layer",0);
    reject("source_channels",json::array{1,2});
    reject("destination_channels",json::array{1,2});
    reject("elements",-1);
    reject("elements",2);
    reject("stream_range",json::array{0,0});
    reject("stream_range",json::array{0,2});
    reject("stream_range",json::array{-1,1});
    auto w = workload();
    auto& dependency = w.at("dependencies").as_array()[0].as_object();
    dependency["elements"] = 0; dependency["bytes"] = 0;
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w = workload();
    w.at("dependencies").as_array()[0].as_object()["channels"] = json::array{1,2};
    EXPECT_THROW(simulate(w),std::invalid_argument);
    w = workload();
    w.at("dependencies").as_array()[0].as_object()["kind"] = "accumulation";
    EXPECT_THROW(simulate(w),std::invalid_argument);
}
