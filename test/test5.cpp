#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <queue>
#include <algorithm>
#include <limits>
#include <cmath>

// 自定义哈希函数用于pair<int, int>
struct pair_hash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// 节点组结构
struct NodeGroup {
    int group_id;
    std::vector<int> nodes; // 节点编号
    std::vector<int> parent_groups; // 父节点组编号
};

// Mapper 类，管理节点与核心的映射
class Mapper {
public:
    // 核心阵列，-1表示未被占用
    std::vector<std::vector<int>> core_array;
    int rows, cols;

    // 双向映射
    std::unordered_map<int, std::pair<int, int>> node_to_core;
    std::unordered_map<std::pair<int, int>, int, pair_hash> core_to_node;

    Mapper(int r, int c) : rows(r), cols(c), core_array(r, std::vector<int>(c, -1)) {}

    // 检查位置是否在范围内
    bool is_valid(int x, int y) const {
        return x >= 0 && x < rows && y >= 0 && y < cols;
    }

    // 检查核心是否空闲
    bool is_free(int x, int y) const {
        return is_valid(x, y) && core_array[x][y] == -1;
    }

    // 映射节点到核心
    bool map_node_to_core(int node, int x, int y) {
        if (node_to_core.find(node) != node_to_core.end()) {
            std::cout << "节点 " << node << " 已映射到核心 (" << node_to_core[node].first << ", " << node_to_core[node].second << ")\n";
            return false;
        }
        if (!is_free(x, y)) {
            std::cout << "核心 (" << x << ", " << y << ") 已被占用\n";
            return false;
        }
        node_to_core[node] = {x, y};
        core_to_node[{x, y}] = node;
        core_array[x][y] = node;
        return true;
    }

    // 解除节点映射
    bool unmap_node(int node) {
        auto it = node_to_core.find(node);
        if (it == node_to_core.end()) {
            std::cout << "节点 " << node << " 未映射\n";
            return false;
        }
        int x = it->second.first;
        int y = it->second.second;
        node_to_core.erase(it);
        core_to_node.erase({x, y});
        core_array[x][y] = -1;
        return true;
    }

    // 打印当前映射
    void print_mappings() const {
        std::cout << "当前节点到核心的映射:\n";
        for (const auto& pair : node_to_core) {
            std::cout << "节点 " << pair.first << " -> (" << pair.second.first << ", " << pair.second.second << ")\n";
        }
    }

    // 打印核心阵列状态
    void print_core_array() const {
        std::cout << "核心阵列状态:\n";
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                if (core_array[i][j] == -1)
                    std::cout << " - ";
                else
                    std::cout << " " << core_array[i][j] << " ";
            }
            std::cout << "\n";
        }
    }

    // 计算曼哈顿距离
    int manhattan_distance(int x1, int y1, int x2, int y2) const {
        return std::abs(x1 - x2) + std::abs(y1 - y2);
    }

    // 找到与指定位置最近的空闲区域，返回左上角坐标
    bool find_nearest_contiguous_block(int block_size, const std::vector<std::pair<int, int>>& parent_cores, std::pair<int, int>& start_pos) const {
        int min_distance = std::numeric_limits<int>::max();
        bool found = false;
        // 遍历整个核心阵列，寻找符合条件的区域
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                // 检查从 (i,j) 开始的block_size是否可用
                if (j + block_size > cols) continue; // 超出范围
                bool can_place = true;
                for (int k = 0; k < block_size; ++k) {
                    if (!is_free(i, j + k)) {
                        can_place = false;
                        break;
                    }
                }
                if (can_place) {
                    // 计算与父节点组的最小曼哈顿距离
                    int distance = 0;
                    if (!parent_cores.empty()) {
                        int temp_min = std::numeric_limits<int>::max();
                        for (const auto& core : parent_cores) {
                            int d = manhattan_distance(i, j, core.first, core.second);
                            if (d < temp_min) temp_min = d;
                        }
                        distance = temp_min;
                    }
                    // 更新最优位置
                    if (distance < min_distance) {
                        min_distance = distance;
                        start_pos = {i, j};
                        found = true;
                        if (min_distance == 0) { // 最优
                            return true;
                        }
                    }
                }
            }
        }
        return found;
    }

    // 映射一组节点到核心
    bool map_group(const NodeGroup& group, const std::unordered_map<int, NodeGroup>& group_map) {
        // 收集所有父节点组的核心位置
        std::vector<std::pair<int, int>> parent_cores;
        for (const int parent_id : group.parent_groups) {
            auto it = group_map.find(parent_id);
            if (it != group_map.end()) {
                for (const int parent_node : it->second.nodes) {
                    auto it_node = node_to_core.find(parent_node);
                    if (it_node != node_to_core.end()) {
                        parent_cores.push_back(it_node->second);
                    }
                }
            }
        }

        int block_size = group.nodes.size();
        std::pair<int, int> start_pos;

        bool found = false;
        if (!parent_cores.empty()) {
            found = find_nearest_contiguous_block(block_size, parent_cores, start_pos);
        } else {
            // 无父节点，默认从左上角开始
            found = find_nearest_contiguous_block(block_size, {}, start_pos);
        }

        if (!found) {
            std::cout << "无法找到足够的空闲核心块来映射节点组 " << group.group_id << "\n";
            return false;
        }

        // 映射节点到找到的区域
        for (size_t i = 0; i < group.nodes.size(); ++i) {
            int node = group.nodes[i];
            int x = start_pos.first;
            int y = start_pos.second + i;
            if (!map_node_to_core(node, x, y)) {
                std::cout << "映射失败: 节点 " << node << " 到 (" << x << ", " << y << ")\n";
                return false;
            }
        }
        std::cout << "成功映射节点组 " << group.group_id << " 到区域 (" << start_pos.first << ", " << start_pos.second << ") 到 (" << start_pos.first << ", " << start_pos.second + group.nodes.size() - 1 << ")\n";
        return true;
    }
};
int main() {
    // 初始化Mapper，假设核心阵列为5行5列
    Mapper mapper(5, 5);

    // 定义节点组
    std::vector<NodeGroup> node_groups = {
        {1, {1, 2, 3}, {}},        // 第1组，无父节点
        {2, {4, 5}, {1}},          // 第2组，父节点组为第1组
        {3, {6, 7, 8}, {2}},       // 第3组，父节点组为第2组
        {4, {9}, {1, 3}}           // 第4组，父节点组为第1和第3组
    };

    // 创建一个组ID到NodeGroup的映射，方便查找父组
    std::unordered_map<int, NodeGroup> group_map;
    for (const auto& group : node_groups) {
        group_map[group.group_id] = group;
    }

    // 按组ID排序或其他逻辑，以确保父组先被映射
    // 这里假设node_groups按依赖关系排序
    for (const auto& group : node_groups) {
        mapper.map_group(group, group_map);
    }

    // 打印最终映射
    mapper.print_mappings();
    mapper.print_core_array();

    return 0;
}
