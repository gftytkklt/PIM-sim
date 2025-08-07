#include "util.h"

/**
 * @brief Partitions an MxN grid of nodes into boxes, each containing up to K nodes,
 *        following specific packing rules.
 *
 * @param M Number of rows in the grid.
 * @param N Number of columns in the grid.
 * @param K Maximum number of nodes each box can contain.
 * @return A vector of boxes, where each box is a vector of (m, n) node indices.
 */
std::vector<std::vector<std::pair<int, int>>> uniformsplit(int M, int N, int K) {
    // Vector to store all boxes
    std::vector<std::vector<std::pair<int, int>>> boxes;
    
    // Current box being filled
    std::vector<std::pair<int, int>> currentBox;
    
    // List of residual columns to process after initial processing
    std::vector<std::vector<std::pair<int, int>>> residualColumns;
    
    // Function to process a column and handle residuals
    auto processColumn = [&](const std::vector<std::pair<int, int>>& columnNodes) {
        int columnSize = columnNodes.size();
        
        if(columnSize <= K){
            // Try to add entire column to current box
            if(currentBox.size() + columnSize <= K){
                currentBox.insert(currentBox.end(), columnNodes.begin(), columnNodes.end());
            }
            else{
                // Push current box to boxes
                if(!currentBox.empty()){
                    boxes.emplace_back(currentBox);
                    currentBox.clear();
                }
                // Start a new box with the entire column
                currentBox.insert(currentBox.end(), columnNodes.begin(), columnNodes.end());
            }
        }
        else{
            // Column size > K, need to split into full K-sized chunks
            int fullChunks = columnSize / K;
            int residual = columnSize % K;
            
            // Add full K-sized chunks as separate boxes
            for(int i = 0; i < fullChunks; ++i){
                // If current box has data, push it before starting a new full box
                if(!currentBox.empty()){
                    boxes.emplace_back(currentBox);
                    currentBox.clear();
                }
                // Create a new box for the K-sized chunk
                std::vector<std::pair<int, int>> fullBox(columnNodes.begin() + i*K, columnNodes.begin() + (i+1)*K);
                boxes.emplace_back(fullBox);
            }
            
            // Handle residual nodes
            if(residual > 0){
                std::vector<std::pair<int, int>> residualChunk(columnNodes.begin() + fullChunks*K, columnNodes.end());
                residualColumns.emplace_back(residualChunk);
            }
        }
    };
    
    // Iterate over each column
    for(int n = 0; n < N; ++n){
        // Create the current column as a list of (m, n) pairs
        std::vector<std::pair<int, int>> currentColumn;
        for(int m = 0; m < M; ++m){
            currentColumn.emplace_back(std::make_pair(m, n));
        }
        // Process the current column
        processColumn(currentColumn);
    }
    
    // Now process all residual columns
    for(auto &residual : residualColumns){
        processColumn(residual);
    }
    
    // After processing all columns, push the last box if it's not empty
    if(!currentBox.empty()){
        boxes.emplace_back(currentBox);
    }
    
    return boxes;
}

/**
 * @brief For CEdge merge, only unique elements are counted.
 * 
 * @param pair1 existing interval
 * @param pair2 new interval
 * @return int unique elements num in the new interval
 */
int UniqueElements(const std::pair<int, int>& pair1, const std::pair<int, int>& pair2) {
    int start1 = pair1.first, end1 = pair1.second;
    int start2 = pair2.first, end2 = pair2.second;

    // total length of the second interval
    int totalLength2 = end2 - start2 + 1;

    // no overlap
    if (end2 < start1 || start2 > end1) {
        return totalLength2;  // return the total length of the second interval
    }

    // intersection
    int overlapStart = std::max(start1, start2);
    int overlapEnd = std::min(end1, end2);

    // intersection length
    int overlapLength = overlapEnd - overlapStart + 1;

    // unique length
    return totalLength2 - overlapLength;
}

int manhattan_distance(int x1, int y1, int x2, int y2) {
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

int manhattan_distance(std::pair<int, int> p1, std::pair<int, int> p2) {
    return manhattan_distance(p1.first, p1.second, p2.first, p2.second);
}

/**
 * @brief generate path from src to dst by xy-routing
 * 
 * @param src 
 * @param dst 
 * @return std::vector<std::pair<int, int>> 
 */
std::vector<std::pair<int, int>> XYinit(std::pair<int, int> src, std::pair<int, int> dst)
{
    int x1 = src.first;
    int y1 = src.second;
    int x2 = dst.first;
    int y2 = dst.second;

    std::vector<std::pair<int, int>> path;
    path.push_back({x1, y1});

    // X
    while (x1 != x2) 
    {
        if (x1 < x2)
            x1++;
        else
            x1--;
        path.push_back({x1, y1});
    }

    // Y
    while (y1 != y2) 
    {
        if (y1 < y2)
            y1++;
        else
            y1--;
        path.push_back({x1, y1});
    }

    return path;
}

/**
 * @brief compute median of a vector
 * 
 * @param vec input vector
 * @return int median value
 */
int compute_median(std::vector<int>& vec) {
    std::sort(vec.begin(), vec.end());
    int n = vec.size();
    if (n % 2 == 0) {
        return (vec[n / 2 - 1] + vec[n / 2]) / 2;
    } else {
        return vec[n / 2];
    }
}

/**
 * @brief fast compute median of a vector
 * 
 * @param vec input vector
 * @return int median value
 */
int fast_compute_median(std::vector<int>& vec) {
    int n = vec.size();
    int pos = 0;
    if (n % 2 == 0) {
        pos = n / 2 - 1;
    } else {
        pos = n / 2;
    }
    std::nth_element(vec.begin(), vec.begin() + pos, vec.end());
    return vec[pos];
}

/**
 * @brief fast compute median of a vector of pairs
 * 
 * @param vec input vector pairs
 * @return std::pair<int, int> input pair with median value
 */
std::pair<int, int> fast_compute_median(std::vector<std::pair<int, int>>& vec) {
    int n = vec.size();
    int pos = 0;
    if (n % 2 == 0) {
        pos = n / 2 - 1;
    } else {
        pos = n / 2;
    }
    std::nth_element(vec.begin(), vec.begin() + pos, vec.end(), pair_first_comparator());
    int first = vec[pos].first;
    std::nth_element(vec.begin(), vec.begin() + pos, vec.end(), pair_second_comparator());
    int second = vec[pos].second;
    return std::make_pair(first, second);
}
/**
 * @brief Get the median point object
 * 
 * @param vec input vector of pairs
 * @return std::pair<int, int> vec point closest to the median
 */
std::pair<int, int> get_median_point(std::vector<std::pair<int, int>>& vec) {
    auto mid_pt = fast_compute_median(vec);
    // std::sort(vec.begin(), vec.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
    //     return manhattan_distance(a, mid_pt) < manhattan_distance(b, mid_pt);
    // });
    // return vec[0];
    return *std::min_element(vec.begin(), vec.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return manhattan_distance(a, mid_pt) < manhattan_distance(b, mid_pt);
    });
}
 int Combination(int n, int k) {
    if (k == 0 || k == n) {
        return 1;
    }
    return Combination(n - 1, k - 1) + Combination(n - 1, k);
 }

 int shortest_path_num(std::pair<int, int> src, std::pair<int, int> dst) {
    auto n = manhattan_distance(src, dst);
    auto k = std::min(std::abs(src.first - dst.first), std::abs(src.second - dst.second));
    return Combination(n, k);
 }

 std::vector<int> dup_to_dup(int M, int N, int dup_id) {
    // M: original dup num, N: target dup num
    // dup_id: current dup id in [0, M-1]
    std::vector<int> result;
    if (M == N) {
        result.push_back(dup_id);
        // return result;
    }
    else {
        for (int i = dup_id * N / M; i <= (dup_id + 1) * N / M; i++) {
            // result.push_back(i);
            if (i < N) {
                result.push_back(i);
            } // i must be in [0, N-1]
        }
    }
    return result;
}

int compute_node_num(std::pair<int, int> xbar_size, std::pair<int, int> window_shape, std::pair<int, int> channel_shape) {
    auto [WL, BL] = xbar_size;
    auto [w, h] = window_shape;
    auto [ci, co] = channel_shape;
    auto in_split = std::max(1, w * h * ci / WL);
    auto out_split = std::max(1, co / BL);
    return in_split * out_split;
}

std::vector<size_t> neighbor_ranking_sort(const std::unordered_map<size_t, std::unordered_map<size_t, int>>& conn_intensity_map) {
    std::unordered_map<size_t, double> intensity_scores;
    for (const auto& [node, neighbors] : conn_intensity_map) {
        double score = 0.0;
        for (const auto& [neighbor, intensity] : neighbors) {
            if (conn_intensity_map.find(neighbor) != conn_intensity_map.end()) {
                for (const auto& [co_neighbor, co_intensity] : conn_intensity_map.at(neighbor)) {
                    if (co_neighbor == node || co_neighbor == neighbor) {
                        continue;
                    }
                    if (conn_intensity_map.at(node).find(co_neighbor) != conn_intensity_map.at(node).end()) {
                        double min_intensity = std::min(
                            conn_intensity_map.at(node).at(co_neighbor), 
                            conn_intensity_map.at(neighbor).at(co_neighbor));
                        score += min_intensity;
                    }
                }
            }
        }
        intensity_scores[node] = score;
    }
    std::vector<size_t> sorted_nodes;
    for (const auto& [node, score] : intensity_scores) {
        sorted_nodes.push_back(node);
    }
    std::sort(sorted_nodes.begin(), sorted_nodes.end(), [&](size_t a, size_t b) {
        return intensity_scores[a] > intensity_scores[b];
    });
    return sorted_nodes;
}