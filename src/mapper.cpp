#include "mapper.h"
bool Mapper::map_node_to_core(size_t node, int x, int y) {
    if (node_to_core.find(node) != node_to_core.end()) {
        // node is already mapped
        return false;
    }
    if (!is_free(x, y)) {
        // core is not free
        return false;
    }
    node_to_core[node] = {x, y};
    core_to_node[{x, y}] = node;
    core_array[x][y] = node;
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

bool Mapper::find_best_contiguous_block(int required_size, const std::vector<std::pair<int, int>>& ref_points, std::pair<int, int>& best_start, int& min_distance) const {
    bool found = false;
    min_distance = std::numeric_limits<int>::max();
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
                        return true;
                    }
                }
            }
        }
    }
    return found;
}

bool Mapper::map_group(const Group& group) {
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

    int required_size = unmapped_nodes.size();
    if (required_size == 0) {
        // all nodes are already mapped, no need to do anything
        return true;
    }

    std::pair<int, int> best_start;
    int min_distance;

    bool found = find_best_contiguous_block(required_size, ref_points, best_start, min_distance);
    if (!found) {
        // if contiguous block cannot be found, return failure
        std::cout << "cannt find enough contiguous free cores to map the group\n";
        return false;
    }

    // map unmapped nodes to the found contiguous block
    for (int k = 0; k < required_size; ++k) {
        size_t node = unmapped_nodes[k];
        int x = best_start.first;
        int y = best_start.second + k;
        bool success = map_node_to_core(node, x, y);
        if (!success) {
            std::cout << "failed to map node " << node << " to (" << x << ", " << y << ")\n";
            // unmap all nodes that have been mapped
            for (int m = 0; m < k; ++m) {
                size_t rem_node = unmapped_nodes[m];
                unmap_node(rem_node);
            }
            return false;
        }
    }

    std::cout << "success to map group to region (" << best_start.first << ", " << best_start.second << ") to ("
                << best_start.first << ", " << best_start.second + required_size - 1 << ")\n";
    return true;
}