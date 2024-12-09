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
vector<vector<pair<int, int>>> partitionNodes(int M, int N, int K) {
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
 * @brief Utility function to print the boxes and their contained nodes.
 *
 * @param boxes The vector of boxes to be printed.
 */
void printBoxes(const vector<vector<pair<int, int>>> &boxes){
    for(int i = 0; i < boxes.size(); ++i){
        cout << "Box " << i+1 << " contains nodes: ";
        for(auto &node : boxes[i]){
            cout << "(" << node.first << ", " << node.second << ") ";
        }
        cout << endl;
    }
}

/**
 * @brief Utility function to generate node indices for a given column.
 *
 * @param M Number of rows.
 * @param n Column index.
 * @return A vector of (m, n) pairs representing nodes in the column.
 */
vector<pair<int, int>> generateColumn(int M, int n){
    vector<pair<int, int>> column;
    for(int m = 0; m < M; ++m){
        column.emplace_back(make_pair(m, n));
    }
    return column;
}

int main(){
    // Example 1
    cout << "=== Example 1 ===" << endl;
    int M1 = 10; // Number of rows
    int N1 = 3;  // Number of columns
    int K1 = 4;  // Maximum nodes per box
    
    vector<vector<pair<int, int>>> boxes1 = partitionNodes(M1, N1, K1);
    printBoxes(boxes1);
    
    cout << "\n=== Example 2 ===" << endl;
    // Example 2
    int M2 = 3;
    int N2 = 5;
    int K2 = 4;
    
    vector<vector<pair<int, int>>> boxes2 = partitionNodes(M2, N2, K2);
    printBoxes(boxes2);
    
    cout << "\n=== Example 3 ===" << endl;
    // Example 3
    int M3 = 5;
    int N3 = 4;
    int K3 = 6;
    
    vector<vector<pair<int, int>>> boxes3 = partitionNodes(M3, N3, K3);
    printBoxes(boxes3);
    
    return 0;
}
