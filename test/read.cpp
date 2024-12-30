#include "graph.h"
#include "fstream"

// read from file
std::vector<NNkernel> read_from_json() {
    // 路径需要修改
    std::ifstream in("~/opencode/kernels.json");
    if (!in.is_open()) {
        std::cerr << "无法打开文件！检查路径下文件是否存在" << std::endl;
        return {};
    }

    nlohmann::json json_kernels;
    in >> json_kernels;

    // 反序列化为 std::vector<NNkernel>
    std::vector<NNkernel> kernels = json_kernels.get<std::vector<NNkernel>>();
    return kernels;
}