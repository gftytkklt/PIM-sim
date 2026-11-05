#include "analyzer.h"
#include "simulator/Workload.h"
#include <boost/json.hpp>
#include <gtest/gtest.h>
#include <set>

namespace {
NNkernel kernel(int layer, int next = -1, int channels = 4) {
    return {layer, {1, 1}, {channels, channels}, {{next, {1, channels}}}, {2, 2}, {2, 2}};
}
std::shared_ptr<CGraph> crossbars(std::vector<NNkernel> kernels) {
    return std::make_shared<CGraph>(std::move(kernels), std::pair{8, 8},
                                    std::make_shared<CStrategyMNSIM>());
}
}

TEST(MappingInvariants, TopologicalDepthDoesNotDependOnInputOrder) {
    auto cg = crossbars({kernel(2), kernel(1, 2), kernel(0, 1)});
    EXPECT_EQ(cg->get_depth_map().at(0), 0);
    EXPECT_EQ(cg->get_depth_map().at(1), 1);
    EXPECT_EQ(cg->get_depth_map().at(2), 2);
}

TEST(MappingInvariants, SparseLayerIdsPreserveDependencies) {
    auto cg = crossbars({kernel(30), kernel(10, 30)});
    ASSERT_EQ(boost::num_vertices(cg->get_graph()), 2);
    ASSERT_EQ(boost::num_edges(cg->get_graph()), 1);
    auto edge = *boost::edges(cg->get_graph()).first;
    EXPECT_EQ(cg->get_graph()[boost::source(edge, cg->get_graph())].layer, 10);
    EXPECT_EQ(cg->get_graph()[boost::target(edge, cg->get_graph())].layer, 30);
    EXPECT_EQ(cg->get_graph()[edge].datavolume, 16);
}

TEST(MappingInvariants, InvalidDependenciesFailExplicitly) {
    EXPECT_THROW(crossbars({kernel(0, 0)}), pim::GraphError);
    EXPECT_THROW(crossbars({kernel(0, 1), kernel(1, 0)}), pim::GraphError);
    EXPECT_THROW(crossbars({kernel(0, 7)}), pim::GraphError);
    EXPECT_THROW(crossbars({kernel(0), kernel(0)}), pim::GraphError);
}

TEST(MappingInvariants, InvalidDimensionsFailBeforePartitioning) {
    auto k = kernel(0);
    k.wsize = {3, 3};
    EXPECT_THROW(crossbars({k}), pim::GraphError);
    EXPECT_THROW(CGraph({kernel(0)}, {0, 8}, std::make_shared<CStrategyMNSIM>()), pim::ConfigError);
    auto cg = crossbars({kernel(0)});
    EXPECT_THROW(TGraph(cg, 0, std::make_shared<TStrategyMNSIM>()), pim::ConfigError);
    EXPECT_THROW(Mapper(0, 2), pim::ConfigError);
    EXPECT_THROW(Scheduler(std::pair{1, 0}), pim::ConfigError);
}

TEST(MappingInvariants, ChannelRestrictionIsApplied) {
    auto src = kernel(0, 1);
    src.depinfo[0].dep_chan = {2, 3};
    auto cg = crossbars({src, kernel(1)});
    ASSERT_EQ(boost::num_edges(cg->get_graph()), 1);
    auto edge = *boost::edges(cg->get_graph()).first;
    EXPECT_EQ(cg->get_graph()[edge].channel_id, (std::pair{2, 3}));
    EXPECT_EQ(cg->get_graph()[edge].datavolume, 8);
}

TEST(MappingInvariants, PoolProjectionPreservesRawPartialSums) {
    NNkernel source{0, {1, 1}, {5, 2}, {{1, {1, 2}, {1, 2}, 8, "pooled"}}, {4, 4}, {4, 4}};
    NNkernel destination{1, {1, 1}, {2, 1}, {{-1, {0, 0}}}, {2, 2}, {2, 2}};
    CGraph cg({source, destination}, {4, 4}, std::make_shared<CStrategyMNSIM>());
    int accumulation = 0, propagation = 0;
    for (auto edge : boost::make_iterator_range(boost::edges(cg.get_graph()))) {
        const auto& data = cg.get_graph()[edge];
        if (data.c_type == DepType::Accum) accumulation += data.datavolume;
        else propagation += data.datavolume;
    }
    EXPECT_EQ(cg.get_graph()[0].ofmap_size, 32);
    EXPECT_EQ(cg.get_graph()[1].ofmap_size, 32);
    EXPECT_EQ(accumulation, 32);  // MVM partials exist before pooling.
    EXPECT_EQ(propagation, 8);    // Two channels after 4x4 -> 2x2 pooling.
}

TEST(MappingInvariants, FlattenSplitsElementsInsideSourceChannels) {
    NNkernel source{0, {1, 1}, {1, 2}, {{1, {1, 2}, {1, 8}, 8, "pool_flatten"}}, {3, 3}, {3, 3}};
    NNkernel destination{1, {1, 1}, {8, 1}, {{-1, {0, 0}}}, {1, 1}, {1, 1}};
    auto cg = std::make_shared<CGraph>(std::vector{source, destination}, std::pair{3, 4},
                                      std::make_shared<CStrategyMNSIM>());
    std::vector<int> volumes;
    std::vector<std::pair<int64_t, int64_t>> ranges;
    for (auto edge : boost::make_iterator_range(boost::out_edges(0, cg->get_graph()))) {
        const auto& data = cg->get_graph()[edge];
        volumes.push_back(data.datavolume);
        ranges.push_back(data.stream_range);
    }
    EXPECT_EQ(volumes, (std::vector<int>{3, 3, 2}));
    EXPECT_EQ(ranges, (std::vector<std::pair<int64_t, int64_t>>{{0, 3}, {3, 6}, {6, 8}}));
    EXPECT_EQ(cg->get_graph()[0].ofmap_size, 18);
    TGraph tg(cg, 3, std::make_shared<TStrategyMNSIM>());
    ASSERT_EQ(boost::num_edges(tg.get_graph()), 1);
    const auto& edge = tg.get_graph()[*boost::edges(tg.get_graph()).first];
    EXPECT_EQ(edge.propvolume, 8);  // Partial-channel streams form an exact union.
}

TEST(MappingInvariants, ConcatUsesDestinationChannelOffsets) {
    NNkernel first{0, {1, 1}, {1, 2}, {{2, {1, 2}, {1, 2}, 8, "first"}}, {2, 2}, {2, 2}};
    NNkernel second{1, {1, 1}, {1, 3}, {{2, {1, 3}, {3, 5}, 12, "second"}}, {2, 2}, {2, 2}};
    NNkernel destination{2, {1, 1}, {5, 1}, {{-1, {0, 0}}}, {2, 2}, {2, 2}};
    CGraph cg({first, second, destination}, {2, 4}, std::make_shared<CStrategyMNSIM>());
    int first_volume = 0, second_volume = 0;
    for (auto edge : boost::make_iterator_range(boost::edges(cg.get_graph()))) {
        const auto& data = cg.get_graph()[edge];
        if (data.c_type != DepType::Prop) continue;
        const auto& src = cg.get_graph()[boost::source(edge, cg.get_graph())];
        const auto& dst = cg.get_graph()[boost::target(edge, cg.get_graph())];
        if (src.layer == 0) {
            first_volume += data.datavolume;
            EXPECT_EQ(dst.id_cin, (std::pair{1, 2}));
        } else {
            second_volume += data.datavolume;
            EXPECT_GE(dst.id_cin.first, 3);
        }
    }
    EXPECT_EQ(first_volume, 8);
    EXPECT_EQ(second_volume, 12);
}

TEST(MappingInvariants, DistinctProjectedViewsDoNotCollapseInMulticast) {
    auto source = kernel(0, 1, 2);
    source.depinfo = {{1, {1, 2}, {1, 2}, 8, "raw"},
                      {1, {1, 2}, {1, 2}, 8, "raw"},
                      {1, {1, 2}, {1, 2}, 8, "sigmoid"}};
    auto cg = crossbars({source, kernel(1, -1, 2)});
    TGraph tg(cg, 1, std::make_shared<TStrategyMNSIM>());
    ASSERT_EQ(boost::num_edges(tg.get_graph()), 1);
    EXPECT_EQ(tg.get_graph()[*boost::edges(tg.get_graph()).first].propvolume, 16);
}

TEST(MappingInvariants, OverlappingLegacyProjectionRangesStillDeduplicate) {
    auto source = kernel(0, 1, 2);
    source.depinfo = {{1, {1, 1}}, {1, {1, 2}}};
    auto cg = crossbars({source, kernel(1, -1, 2)});
    TGraph tg(cg, 1, std::make_shared<TStrategyMNSIM>());
    ASSERT_EQ(boost::num_edges(tg.get_graph()), 1);
    EXPECT_EQ(tg.get_graph()[*boost::edges(tg.get_graph()).first].propvolume, 8);
}

TEST(MappingInvariants, NonuniformProjectionConservesElementsAndZeroSliceControl) {
    NNkernel source{0, {1, 1}, {1, 2}, {{1, {1, 2}, {1, 1}, 1, "reduced"}}, {1, 1}, {1, 1}};
    NNkernel destination{1, {1, 1}, {1, 1}, {{-1, {0, 0}}}, {1, 1}, {1, 1}};
    auto cg = std::make_shared<CGraph>(std::vector{source, destination}, std::pair{1, 1},
                                      std::make_shared<CStrategyMNSIM>());
    ASSERT_EQ(boost::num_edges(cg->get_graph()), 2);
    int volume = 0, controls = 0;
    for (auto edge : boost::make_iterator_range(boost::edges(cg->get_graph()))) {
        volume += cg->get_graph()[edge].datavolume;
        controls += cg->get_graph()[edge].datavolume == 0;
    }
    EXPECT_EQ(volume, 1);
    EXPECT_EQ(controls, 1);
    TGraph tg(cg, 1, std::make_shared<TStrategyMNSIM>());
    EXPECT_EQ(boost::num_edges(tg.get_graph()), 2);
}

TEST(MappingInvariants, ProjectionValidatesFormatsAndRetainsIntegerVolumeLimits) {
    auto source = kernel(0, 1);
    source.depinfo[0].dst_chan = {2, 5};
    EXPECT_THROW(crossbars({source, kernel(1)}), pim::GraphError);
    source.depinfo[0].dst_chan = {0, 1};
    EXPECT_THROW(crossbars({source, kernel(1)}), pim::GraphError);
    source.depinfo[0].dst_chan = {0, 0};
    source.depinfo[0].elements = -2;
    EXPECT_THROW(crossbars({source, kernel(1)}), pim::GraphError);
    source.depinfo[0].elements = std::numeric_limits<int64_t>::max();
    EXPECT_THROW(crossbars({source, kernel(1)}), std::overflow_error);
}

TEST(MappingInvariants, IsolatedCrossbarsAreNotLostByClustering) {
    auto cg = crossbars({kernel(0)});
    TGraph tg(cg, 1, std::make_shared<TStrategyPIMAPPING>());
    ASSERT_EQ(boost::num_vertices(tg.get_graph()), 1);
    EXPECT_EQ(tg.get_graph()[0].cnode_id, (std::vector<size_t>{0}));
}

TEST(MappingInvariants, ParallelEdgesKeepTypeAndDisjointChannels) {
    auto cg = crossbars({kernel(0, 1), kernel(1)});
    auto& graph = cg->get_graph();
    boost::remove_edge(0, 1, graph);
    boost::add_edge(0, 1, CEdge{DepType::Prop, {1, 1}, 4}, graph);
    boost::add_edge(0, 1, CEdge{DepType::Prop, {3, 3}, 4}, graph);
    boost::add_edge(0, 1, CEdge{DepType::Prop, {3, 4}, 8}, graph);
    boost::add_edge(0, 1, CEdge{DepType::Accum, {1, 1}, 4}, graph);
    TGraph tg(cg, 1, std::make_shared<TStrategyMNSIM>());
    ASSERT_EQ(boost::num_edges(tg.get_graph()), 1);
    auto edge = *boost::edges(tg.get_graph()).first;
    EXPECT_EQ(tg.get_graph()[edge].propvolume, 12);
    EXPECT_EQ(tg.get_graph()[edge].accvolume, 4);
    EXPECT_EQ(tg.get_graph()[edge].t_type, DepType::Mixed);
    EXPECT_EQ(tg.get_graph()[edge].layer_map.at(0), 16);
}

TEST(MappingInvariants, PhysicalCapacityIsNotSilentlyExpanded) {
    auto cg = crossbars({kernel(0, 1), kernel(1)});
    auto tg = std::make_shared<TGraph>(cg, 1, std::make_shared<TStrategyMNSIM>());
    EXPECT_THROW(HGraph(tg, cg, {1, 1}, std::make_shared<HStrategyMNSIM>()), pim::MappingError);
    Mapper mapper(1, 1);
    EXPECT_THROW(mapper.zigzag_mapping(2), pim::MappingError);
    EXPECT_EQ(mapper.get_core(0), (std::pair{-1, -1}));
}

TEST(MappingInvariants, SpatemMapsIsolatedTiles) {
    auto cg = crossbars({kernel(0)});
    auto tg = std::make_shared<TGraph>(cg, 1, std::make_shared<TStrategyMNSIM>());
    HGraph hg(tg, cg, {1, 1}, std::make_shared<HStrategySPATEM>());
    EXPECT_EQ(hg.get_hnode(0), (std::pair{0, 0}));
}

TEST(MappingInvariants, NoCommunicationStillHasComputeSegmentsAndExportIsIdempotent) {
    Analyzer analyzer({kernel(0)}, {{8, 8}, 1, {1, 1}, 1}, {false, false});
    analyzer.generate_analysis_result();
    analyzer.generate_comm_info();
    auto result = analyzer.get_analysis_result();
    ASSERT_EQ(result.deploy_info.size(), 1);
    ASSERT_EQ(result.comm_segs.size(), 1);
    EXPECT_EQ(result.comm_segs[0].layers, (std::vector<int>{0}));
    EXPECT_EQ(result.comm_segs[0].datas, (DataMatrix{{0}}));
    EXPECT_EQ(analyzer.get_comm_info().path_num, 0);
    analyzer.generate_analysis_result();
    EXPECT_EQ(analyzer.get_analysis_result().deploy_info.size(), 1);
    EXPECT_EQ(analyzer.get_analysis_result().comm_segs.size(), 1);
}

TEST(MappingInvariants, EmptyInputIsAnEmptyWorkload) {
    Analyzer analyzer({}, {{8, 8}, 1, {1, 1}, 1}, {true, true});
    analyzer.generate_analysis_result();
    analyzer.generate_comm_info();
    EXPECT_TRUE(analyzer.get_analysis_result().deploy_info.empty());
    EXPECT_TRUE(analyzer.get_analysis_result().comm_segs.empty());
    EXPECT_EQ(analyzer.get_comm_info().path_num, 0);
    EXPECT_EQ(analyzer.get_comm_info().total_congestion, 0);
}

TEST(MappingInvariants, DefaultOptimizationDoesNotInventWeightReplicas) {
    Analyzer analyzer({kernel(0)}, {{8, 8}, 1, {1, 1}, 1}, {true, true});
    analyzer.generate_analysis_result();
    ASSERT_EQ(analyzer.get_analysis_result().deploy_info.size(), 1);
    EXPECT_EQ(analyzer.get_analysis_result().deploy_info[0].cnode.size(), 1);
}

TEST(MappingInvariants, PartialSumsUseOutputPositionsAndPartitionCountMatchesConstruction) {
    NNkernel k{0, {1, 3}, {10, 2}, {{-1, {0, 0}}}, {8, 8}, {4, 4}};
    CGraph cg({k}, {10, 8}, std::make_shared<CStrategyMNSIM>());
    EXPECT_EQ(boost::num_vertices(cg.get_graph()), 4);
    EXPECT_EQ(compute_node_num({10, 8}, k.wsize, k.channel), 4);
    ASSERT_EQ(boost::num_edges(cg.get_graph()), 3);
    for (auto edge : boost::make_iterator_range(boost::edges(cg.get_graph()))) {
        EXPECT_EQ(cg.get_graph()[edge].datavolume, 32);
    }
}

TEST(MappingInvariants, TileSubsetSkipsOversizedKernelWithoutChangingCandidateIdentity) {
    auto too_large = kernel(0, -1, 64);
    CGraph cg({too_large, kernel(1)}, {8, 8}, 1, std::make_shared<CStrategyTILE2_0>());
    ASSERT_EQ(boost::num_vertices(cg.get_graph()), 1);
    EXPECT_EQ(cg.get_graph()[0].layer, 1);
    EXPECT_THROW(CGraph({too_large}, {8, 8}, 1, std::make_shared<CStrategyTILE2_0>()), pim::MappingError);
}

TEST(RoutingInvariants, EmptyLocalAndZeroVolumeAreDefined) {
    Scheduler scheduler({1, 1});
    scheduler.set_path_set({});
    EXPECT_EQ(scheduler.schedule(), 0);
    auto path = std::make_shared<Path>(Path{1, {0, 0}, {0, 0}, {}, 0});
    scheduler.set_path_set({path});
    EXPECT_EQ(scheduler.schedule(), 0);
    EXPECT_EQ(path->via, (std::vector<std::pair<int, int>>{{0, 0}}));
    Scheduler line({1, 3});
    auto zero = std::make_shared<Path>(Path{2, {0, 0}, {0, 2}, {}, 0});
    line.set_path_set({zero});
    EXPECT_EQ(line.schedule(), 0);
    EXPECT_EQ(zero->via, xy_init(zero->src, zero->dst));
}

TEST(RoutingInvariants, InvalidPathsFailBeforeRouting) {
    Scheduler scheduler({2, 2});
    EXPECT_THROW(scheduler.set_path_set({nullptr}), pim::SchedulingError);
    auto path = std::make_shared<Path>(Path{0, {-1, 0}, {1, 1}, {}, 3});
    EXPECT_THROW(scheduler.set_path_set({path}), pim::SchedulingError);
    path->src = {0, 0};
    path->datavolume = -1;
    EXPECT_THROW(scheduler.set_path_set({path}), pim::SchedulingError);
}

TEST(RoutingInvariants, RoutesAreShortestDeterministicAndRespectCongestionFormula) {
    Scheduler scheduler({2, 2});
    auto first = std::make_shared<Path>(Path{0, {0, 0}, {1, 1}, {}, 3});
    auto second = std::make_shared<Path>(Path{1, {0, 0}, {1, 1}, {}, 5});
    scheduler.set_path_set({first, second});
    auto congestion = scheduler.schedule();
    auto before = std::pair{first->via, second->via};
    EXPECT_EQ(first->via.size(), 3);
    EXPECT_EQ(second->via.size(), 3);
    EXPECT_EQ(scheduler.schedule(), congestion);
    EXPECT_EQ((std::pair{first->via, second->via}), before);
    CSum sum;
    sum.insert(3);
    EXPECT_EQ(sum.getCSum(), 1);
    sum.insert(5);
    EXPECT_EQ(sum.getCSum(), 6);
    sum.insert(8);
    EXPECT_EQ(sum.getCSum(), 13);
}

TEST(RoutingInvariants, LargeMeshDoesNotOverflowShortestPathCounts) {
    Scheduler scheduler({40, 40});
    auto path = std::make_shared<Path>(Path{0, {0, 0}, {39, 39}, {}, 7});
    scheduler.set_path_set({path});
    EXPECT_EQ(scheduler.schedule(), 78);
    ASSERT_EQ(path->via.size(), 79);
    for (size_t i = 1; i < path->via.size(); ++i) {
        EXPECT_EQ(manhattan_distance(path->via[i - 1], path->via[i]), 1);
        EXPECT_EQ(manhattan_distance(path->via[i], path->dst), 78 - i);
    }
}

TEST(RoutingInvariants, CongestionFormulaIsIndependentOfInsertionOrder) {
    std::vector<int> values{3, 5, 8};
    do {
        CSum sum;
        std::vector<int> prefix;
        for (int value : values) {
            prefix.push_back(value);
            std::sort(prefix.begin(), prefix.end());
            long long expected = prefix.size() < 2 ? 1 :
                std::accumulate(prefix.begin(), prefix.end() - 1, 0LL) + prefix[prefix.size() - 2];
            EXPECT_EQ(sum.getCSum(value), expected);
            sum.insert(value);
            EXPECT_EQ(sum.getCSum(), expected);
        }
    } while (std::next_permutation(values.begin(), values.end()));
    CSum repeated;
    repeated.insert(5);
    repeated.insert(5);
    repeated.insert(5);
    EXPECT_EQ(repeated.getCSum(), 15);
    EXPECT_THROW(repeated.insert(-1), pim::SchedulingError);
}

TEST(MappingInvariants, WorkloadExportPreservesSplitAccumulationAndPropagation) {
    namespace json = boost::json;
    auto input = std::vector<NNkernel>{kernel(0, 1), kernel(1)};
    auto timing = R"({"layer_cycles":{"0":2,"1":3},"element_bits":8,"link_bytes_per_cycle":8,"hop_latency_cycles":1,"network_capacity":2,"clock_period_ns":1.0})";
    auto exported = pim::export_workload(input, {{2, 2}, 2, {2, 2}, 1}, {false, false}, timing);
    auto workload = json::parse(exported).as_object();
    ASSERT_EQ(workload.at("tasks").as_array().size(), 8);
    ASSERT_EQ(workload.at("dependencies").as_array().size(), 8);
    size_t accumulation = 0, propagation = 0, local = 0;
    uint64_t elements = 0;
    for (const auto& edge : workload.at("dependencies").as_array()) {
        accumulation += edge.at("kind").as_string() == "accumulation";
        propagation += edge.at("kind").as_string() == "propagation";
        local += edge.at("route").as_array().size() == 1;
        EXPECT_EQ(edge.at("elements").to_number<int>(), 8);
        elements += edge.at("elements").to_number<uint64_t>();
    }
    EXPECT_EQ(accumulation, 4);
    EXPECT_EQ(propagation, 4);
    EXPECT_EQ(local, 4);
    EXPECT_EQ(elements, 64);
    auto result = json::parse(pim::simulate_workload(exported, 1000)).as_object();
    EXPECT_TRUE(result.at("completed").as_bool());
    EXPECT_EQ(result.at("completed_tasks").to_number<int>(), 8);
    EXPECT_EQ(result.at("delivered_dependencies").to_number<int>(), 8);
    EXPECT_EQ(result.at("sent_bytes").to_number<int>(), 64);
    EXPECT_EQ(result.at("received_bytes").to_number<int>(), 64);
}

TEST(MappingInvariants, LargeChannelIntersectionUsesWideIntermediateArithmetic) {
    auto source = kernel(0, 1, 1000), destination = kernel(1, -1, 1000);
    source.ifmap_size = source.ofmap_size = {100, 100};
    destination.ifmap_size = destination.ofmap_size = {100, 100};
    CGraph graph({source, destination}, {1000, 1000}, std::make_shared<CStrategyMNSIM>());
    ASSERT_EQ(boost::num_edges(graph.get_graph()), 1);
    auto edge = *boost::edges(graph.get_graph()).first;
    EXPECT_EQ(graph.get_graph()[edge].datavolume, 10000000);
}

TEST(MappingInvariants, AggregatedVolumesRejectOverflowRatherThanWrapping) {
    auto cg = crossbars({kernel(0, 1), kernel(1)});
    TGraph tg(cg, 1, std::make_shared<TStrategyMNSIM>());
    tg.update_tedges(0, 1, CEdge{DepType::Prop, {1, 1}, 1200000000}, 0);
    EXPECT_THROW(tg.update_tedges(0, 1, CEdge{DepType::Prop, {1, 1}, 1200000000}, 0), std::overflow_error);

    auto big = [](int layer, int next) {
        return NNkernel{layer, {1, 1}, {1, 1}, {{next, {1, 1}}}, {40000, 30000}, {40000, 30000}};
    };
    // Shared physical link exceeds int even though each flow fits.
    EXPECT_THROW(Analyzer({big(0, 2), big(1, 2), big(2, -1)},
                          (HWInfo{{1, 1}, 1, {1, 3}, 1}), (OptInfo{false, false})), std::overflow_error);
    // Distinct physical links each fit; the exported global sum does not.
    Analyzer analyzer({big(0, 1), big(1, 2), big(2, -1)}, {{1, 1}, 1, {1, 3}, 1}, {false, false});
    EXPECT_THROW(analyzer.generate_comm_info(), std::overflow_error);
}

TEST(MappingInvariants, AutomaticMeshIsExplicitAndDoesNotExpandFixedHardware) {
    Analyzer analyzer({kernel(0, 1), kernel(1)}, {{8, 8}, 1, {0, 0}, 1}, {false, false});
    EXPECT_EQ(analyzer.hgraph().get_shape(), (std::pair{2, 2}));
    EXPECT_THROW(Analyzer({kernel(0, 1), kernel(1)}, (HWInfo{{8, 8}, 1, {1, 1}, 1}),
                          (OptInfo{false, false})), pim::MappingError);
    EXPECT_THROW(Analyzer({kernel(0)}, (HWInfo{{8, 8}, 1, {0, 1}, 1}),
                          (OptInfo{false, false})), pim::ConfigError);
    EXPECT_THROW(Analyzer({kernel(0)}, (HWInfo{{8, 8}, 1, {0, 0}, 1}),
                          (OptInfo{false, false}), true), pim::ConfigError);
}
