#include "mapper.h"
bool Mapper::map_node_to_core(size_t node, int x, int y) {
    if (node_to_core.find(node) != node_to_core.end()) {
        // node is already mapped
        std::cout << "node " << node << " is already mapped" << std::endl;
        return false;
    }
    if (!is_free(x, y)) {
        // core is not free
        std::cout << "core (" << x << ", " << y << ") is not free" << std::endl;
        return false;
    }
    node_to_core[node] = {x, y};
    core_to_node[{x, y}] = node;
    core_array[x][y] = node;
    std::cout << "node " << node << " mapped to core (" << x << ", " << y << ")\n";
    return true;
}

bool Mapper::unmap_node(size_t node) {
    auto it = node_to_core.find(node);
    if (it == node_to_core.end()) {
        // unmapped node
        return false;
    }
    auto [x,y] = it->second;
    node_to_core.erase(it);
    core_to_node.erase({x, y});
    core_array[x][y] = -1;
    return true;
}

void Mapper::print_mappings() const {
    std::cout << "node to core mappping:\n";
    for (const auto& pair : node_to_core) {
        std::cout << "node " << pair.first << " -> (" << pair.second.first << ", " << pair.second.second << ")\n";
    }
}

void Mapper::print_core_array() const {
    std::cout << "core status:\n";
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

/**
 * @brief naive contiguous row search
 * 
 * @param required_size tiles needed for mapping
 * @param ref_points nodes already mapped
 * @return std::pair<bool, std::vector<std::pair<int,int>>> 
 */
std::pair<bool, std::vector<std::pair<int,int>>> Mapper::find_best_contiguous_block(int required_size, const std::vector<std::pair<int, int>>& ref_points) const {
    bool found = false;
    std::vector<std::pair<int,int>> map_set{};
    auto min_distance = std::numeric_limits<int>::max();
    std::pair<int, int> best_start{};
    // traverse all rows
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j <= cols - required_size; ++j) {
            bool can_place = true;
            // check if required_size contiguous cores starting from (i,j) are free
            for (int k = 0; k < required_size; ++k) {
                if (core_array[i][j + k] != -1) {
                    can_place = false;
                    break;
                }
            }
            if (can_place) {
                // calculate the minimum manhattan distance to ref points
                int distance = std::numeric_limits<int>::max();
                for (const auto& ref : ref_points) {
                    // calculate the distance between all positions in this block and ref
                    for (int k = 0; k < required_size; ++k) {
                        int dist = manhattan_distance(i, j + k, ref.first, ref.second);
                        if (dist < distance) {
                            distance = dist;
                        }
                    }
                }
                // if no ref points, distance is 0
                if (ref_points.empty()) {
                    distance = 0;
                }
                // update best position
                if (distance < min_distance) {
                    min_distance = distance;
                    best_start = {i, j};
                    found = true;
                    // return if the optimal distance is 0
                    if (distance == 0) {
                        for (int k = 0; k < required_size; ++k) {
                            map_set.push_back({i, j + k});
                        }
                        // return true;
                        return {true, map_set};
                    }
                }
            }
        }
    }
    if (found) {
        // fill map_set
        for (int k = 0; k < required_size; ++k) {
            map_set.push_back({best_start.first, best_start.second + k});
        }
    }
    return {found, map_set};
    // return found;
}

bool Mapper::map_group(const Group& group, const Group& dep_set) {
    // identify mapped and unmapped nodes
    std::vector<size_t> mapped_nodes;
    std::vector<size_t> unmapped_nodes;
    std::vector<std::pair<int, int>> ref_points;
    for (const auto& node : group) {
        if (node_to_core.find(node) != node_to_core.end()) {
            mapped_nodes.push_back(node);
            ref_points.push_back(node_to_core.at(node));
        } else {
            unmapped_nodes.push_back(node);
        }
    }

    // push mapped dep set nodes to ref points
    for (const auto& node : dep_set) {
        if (node_to_core.find(node) != node_to_core.end()) {
            ref_points.push_back(node_to_core.at(node));
        }
    }

    int required_size = unmapped_nodes.size();
    if (required_size == 0) {
        // all nodes are already mapped, no need to do anything
        return true;
    }

    std::pair<int, int> best_start;
    int min_distance;

    auto [found, map_set] = find_best_contiguous_block(required_size, ref_points);
    if (!found) {
        // if contiguous block cannot be found, return failure
        std::cout << "cannt find enough contiguous free cores to map the group" << std::endl;
        return false;
    }
    // std::cout << "node set: ";
    // for (const auto& node : unmapped_nodes) {
    //     std::cout << node << " ";
    // }
    // std::cout << std::endl;
    // std::cout << "map set: ";
    // for (const auto& [x, y] : map_set) {
    //     std::cout << "(" << x << ", " << y << ") ";
    // }
    std::cout << std::endl;
    // map unmapped nodes to the found contiguous block
    for (int k = 0; k < required_size; ++k) {
        size_t node = unmapped_nodes[k];
        // std::cout << "mapping node " << node << " to (" << map_set[k].first << ", " << map_set[k].second << ")\n";
        bool success = map_node_to_core(node, map_set[k].first, map_set[k].second);
        if (!success) {
            std::cout << "failed to map node " << node << " to (" << map_set[k].first << ", " << map_set[k].second << ")\n";
            // unmap all nodes that have been mapped
            for (int m = 0; m < k; ++m) {
                size_t rem_node = unmapped_nodes[m];
                unmap_node(rem_node);
            }
            return false;
        }
    }
    return true;
}

std::pair<int, int> Mapper::get_core(size_t node) const {
    auto it = node_to_core.find(node);
    if (it == node_to_core.end()) {
        return {-1, -1};
    }
    return it->second;
}