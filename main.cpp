#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <fstream>
#include "analyzer.h"

void printNNkernel(const NNkernel& kernel, std::ofstream& outFile) {
    outFile << "{" << kernel.layer << ", ";
    outFile << "{" << kernel.wsize.first << "," << kernel.wsize.second << "}, ";
    outFile << "{" << kernel.channel.first << "," << kernel.channel.second << "}, ";
    outFile << "std::vector<Depinfo>{ ";
    for (size_t i = 0; i < kernel.depinfo.size(); ++i) {
        const Depinfo& info = kernel.depinfo[i];
        outFile << "{ " << info.dep_layer << ", ";
        outFile << "std::make_pair(" << info.dep_chan.first << "," << info.dep_chan.second << ") ";
        outFile << "}";
        if (i < kernel.depinfo.size() - 1) {
            outFile << ", ";
        }
    }
    outFile << "}, ";
    outFile << "{" << kernel.ifmap_size.first << ", " << kernel.ifmap_size.second << "}, ";
    outFile << "{" << kernel.ofmap_size.first << ", " << kernel.ofmap_size.second << "}}," << std::endl;
}

void check_data(const std::vector<NNkernel>& kernels) {
    // 输出data
    // for (auto& d : kernels) {
    //     std::cout << "Layer: " << d.layer << std::endl;
    //     std::cout << "Wsize: (" << d.wsize.first << ", " << d.wsize.second << ")" << std::endl;
    //     std::cout << "Channel: (" << d.channel.first << ", " << d.channel.second << ")" << std::endl;
    //     std::cout << "  - Ifmap Size: (" << d.ifmap_size.first << ", " << d.ifmap_size.second << ")" << std::endl;
    //     std::cout << "  - Ofmap Size: (" << d.ofmap_size.first << ", " << d.ofmap_size.second << ")" << std::endl;
    //     std::cout << "Depinfo: " << std::endl;
    //     for (auto& di : d.depinfo) {
    //         std::cout << "  - Dep Layer: " << di.dep_layer << ", Dep Channel: (" << di.dep_chan.first << ", " << di.dep_chan.second << ")" << std::endl;
    //     }
    //     std::cout << std::endl;  // 每个NNkernel结构体的数据间空一行，方便区分
    // }

    //检查算子顺序是否违背数据依赖关系
    for (const auto& kernel : kernels) {
        for (const auto& dep : kernel.depinfo) {
            if (dep.dep_layer < kernel.layer & dep.dep_layer >= 0 ){
                std::cout << "Error: Layer " << dep.dep_layer << " depends on layer " << kernel.layer << ", but " << kernel.layer << " comes after " << dep.dep_layer << std::endl;
            }
        }
    }
    // 生成所有kernels的data，方便查看
    std::ofstream outFile("kernels.txt");  // 文件名可以根据需求修改
    if (outFile.is_open()) {
        for (const auto& kernel : kernels) {
            printNNkernel(kernel, outFile);
        }
        outFile.close();
        std::cout << "kernel信息已写入到kernels.txt供查看" << std::endl;
    } else {
        std::cerr << "无法打开文件进行写入。" << std::endl;
    }
}

void test6(const std::vector<NNkernel>& kernels){
    std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256));
    decltype(cg->get_graph()) graph = cg->get_graph();
    TGraph tg = TGraph(cg, 3);
    tg = TGraph(cg, 2);
    auto hg = HGraph(std::make_shared<TGraph>(tg), cg);
    auto dg = DGraph(std::make_shared<HGraph>(hg), std::make_shared<TGraph>(tg), cg);
}
void test7(const std::vector<NNkernel>& kernels){
    HWInfo info = {{1152, 256}, 2, {3, 3}, 1};
    OptInfo opt;
    // test analyzer
    // OptAll
    std::cout << "OptAll" << std::endl;
    opt = {true, true};
    Analyzer analyzer1 = Analyzer(kernels, info, opt);
    // OptSchedule
    std::cout << "OptSchedule" << std::endl;
    opt = {false, true};
    Analyzer analyzer2= Analyzer(kernels, info, opt);
    // OptMapping
    std::cout << "OptMapping" << std::endl;
    opt = {true, false};
    Analyzer analyzer3 = Analyzer(kernels, info, opt);
    // OptNone
    std::cout << "OptNone" << std::endl;
    opt = {false, false};
    Analyzer analyzer4 = Analyzer(kernels, info, opt);
}

int test(const std::vector<NNkernel>& kernels) {
    check_data(kernels);
    //run test
    test6(kernels);
    test7(kernels);

    return 114514;
}

auto analyze(const std::vector<NNkernel>& kernels, const HWInfo& info = {{1152, 256}, 2, {3, 3}, 1}, const OptInfo& opt = {true, true}) {
    Analyzer analyzer1 = Analyzer(kernels, info, opt);
    analyzer1.generate_analysis_result();
    analyzer1.generate_comm_info();
    analyzer1.print_result();
    return std::make_pair<AnalysisResult, CommInfo>(analyzer1.get_analysis_result(), analyzer1.get_comm_info());
}

int main() {
    std::vector<NNkernel> kernels = { 
    {0, {3,3}, {256,384}, std::vector<Depinfo>{{1,std::make_pair(1,384)}},{8, 8},{4, 4}}, 
    {1, {3,3}, {384,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},{4, 4},{2, 2}},
    {2, {3,3}, {384,256}, std::vector<Depinfo>{{-1,std::make_pair(0,0)}},{2, 2},{1, 1}} 
    };
    return test(kernels);
}

namespace py = pybind11;
PYBIND11_MODULE(libmain, m) {
    py::class_<Depinfo>(m, "Depinfo")
       .def(py::init<>())
       .def_readwrite("dep_layer", &Depinfo::dep_layer)
       .def_readwrite("dep_chan", &Depinfo::dep_chan);

    py::class_<NNkernel>(m, "NNkernel")
       .def(py::init<>())
       .def_readwrite("layer", &NNkernel::layer)
       .def_readwrite("wsize", &NNkernel::wsize)
       .def_readwrite("channel", &NNkernel::channel)
       .def_readwrite("depinfo", &NNkernel::depinfo)
       .def_readwrite("ifmap_size", &NNkernel::ifmap_size)
       .def_readwrite("ofmap_size", &NNkernel::ofmap_size);
    
    py::class_<HWInfo>(m, "HWInfo")
        .def(py::init<>())
        .def_readwrite("xbar_size", &HWInfo::xbar_size)
        .def_readwrite("xbar_num", &HWInfo::xbar_num)
        .def_readwrite("tile_size", &HWInfo::tile_size)
        .def_readwrite("pipeline_depth", &HWInfo::pipeline_depth);

    py::class_<OptInfo>(m, "OptInfo")
        .def(py::init<>())
        .def_readwrite("mapping_opt", &OptInfo::mapping_opt)
        .def_readwrite("sched_opt", &OptInfo::sched_opt);

    py::class_<CommInfo>(m, "CommInfo")
        .def(py::init<>())
        .def_readwrite("path_num", &CommInfo::path_num)
        .def_readwrite("datavolume", &CommInfo::datavolume)
        .def_readwrite("total_hops", &CommInfo::total_hops)
        .def_readwrite("total_congestion", &CommInfo::total_congestion);

    py::class_<AnalysisResult>(m, "AnalysisResult")
        .def(py::init<>())
        .def_readwrite("deploy_info", &AnalysisResult::deploy_info)
        .def_readwrite("comm_segs", &AnalysisResult::comm_segs);

    py::class_<DeployInfo>(m, "DeployInfo")
        .def(py::init<>())
        .def_readwrite("layer", &DeployInfo::layer)
        .def_readwrite("tile_id", &DeployInfo::tile_id)
        .def_readwrite("child_tile", &DeployInfo::child_tile)
        .def_readwrite("paths", &DeployInfo::paths)
        .def_readwrite("cnode", &DeployInfo::cnode);
    
    py::class_<Path>(m, "Path")
        .def(py::init<>())
        .def_readwrite("src", &Path::src)
        .def_readwrite("dst", &Path::dst)
        .def_readwrite("via", &Path::via)
        .def_readwrite("datavolume", &Path::datavolume);

    py::class_<CNode>(m, "CNode")
        .def(py::init<>())
        .def_readwrite("layer", &CNode::layer)
        .def_readwrite("ifmap_size", &CNode::ifmap_size)
        .def_readwrite("ofmap_size", &CNode::ofmap_size)
        .def_readwrite("id_cin", &CNode::id_cin)
        .def_readwrite("id_cout", &CNode::id_cout);

    py::class_<CommSeg>(m, "CommSeg")
        .def(py::init<>())
        .def_readwrite("layers", &CommSeg::layers)
        .def_readwrite("datas", &CommSeg::datas);

    m.def("test", &test, "Process data and return a result");

    m.def("analyze", &analyze, 
        py::arg("kernels"),
        py::arg("info") = HWInfo{{1152, 256}, 2, {3, 3}, 1},
        py::arg("opt") = OptInfo{true, true},
        "Analyze data and return a result");
}
