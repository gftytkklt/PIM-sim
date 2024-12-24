#include "util.h"

using namespace std;

/**
 * @brief Partitions an MxN grid of nodes into boxes, each containing up to K nodes,
 *        following specific packing rules.
 *
 * @param M Number of rows in the grid.
 * @param N Number of columns in the grid.
 * @param K Maximum number of nodes each box can contain.
 * @return A vector of boxes, where each box is a vector of (m, n) node indices.
 */
vector<vector<pair<int, int>>> uniformsplit(int M, int N, int K) {
    // Vector to store all boxes
    vector<vector<pair<int, int>>> boxes;
    
    // Current box being filled
    vector<pair<int, int>> currentBox;
    
    // List of residual columns to process after initial processing
    vector<vector<pair<int, int>>> residualColumns;
    
    // Function to process a column and handle residuals
    auto processColumn = [&](const vector<pair<int, int>>& columnNodes) {
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
                vector<pair<int, int>> fullBox(columnNodes.begin() + i*K, columnNodes.begin() + (i+1)*K);
                boxes.emplace_back(fullBox);
            }
            
            // Handle residual nodes
            if(residual > 0){
                vector<pair<int, int>> residualChunk(columnNodes.begin() + fullChunks*K, columnNodes.end());
                residualColumns.emplace_back(residualChunk);
            }
        }
    };
    
    // Iterate over each column
    for(int n = 0; n < N; ++n){
        // Create the current column as a list of (m, n) pairs
        vector<pair<int, int>> currentColumn;
        for(int m = 0; m < M; ++m){
            currentColumn.emplace_back(make_pair(m, n));
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

int compute_median(std::vector<int>& vec) {
    std::sort(vec.begin(), vec.end());
    int n = vec.size();
    if (n % 2 == 0) {
        return (vec[n / 2 - 1] + vec[n / 2]) / 2;
    } else {
        return vec[n / 2];
    }
}

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