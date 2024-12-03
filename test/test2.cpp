#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_traits.hpp>
#include <iostream>
#include <vector>
#include <memory>
struct CNode {
    int id;
    std::string name;
};

struct CEdge {
    double weight;
};

int main() {
    // Define a graph type with custom vertex and edge properties
    using namespace boost;
    typedef adjacency_list<vecS, vecS, undirectedS,property<vertex_name_t, std::string>,property<edge_name_t, std::string>> Graph;
    // Create a graph object
    Graph g;
    auto v1 = add_vertex(g);
    auto v2 = add_vertex(g);
    put(vertex_name, g, v1, "Node 1");
    put(vertex_name, g, v2, "Node 2");
    auto sth = get(vertex_name, g, v1);
    std::cout << "Node 1 name: " << sth << std::endl;

    return 0;
}