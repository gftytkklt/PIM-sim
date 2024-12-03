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
std::ostream& operator<<(std::ostream &os, const CNode &cnode) {
    os << "CNode: " << cnode.id << " " << cnode.name;
    return os;
}

int main() {
    // Define a graph type with custom vertex and edge properties
    using namespace boost;
    using NodeProperty = property<vertex_name_t, CNode>;
    using EdgeProperty = property<edge_name_t, std::string>;

    typedef adjacency_list<vecS, vecS, undirectedS,CNode,CEdge> Graph;
    // Create a graph object
    Graph g;
    auto v1 = add_vertex(g);
    auto v2 = add_vertex(g);
    auto v3 = add_vertex(g);
    auto v4 = add_vertex(CNode{4, "Node 4"}, g);
    // auto v5 = add_vertex({"dd", "Node 5"}, g);
    auto sth = g[v4];
    std::cout << "Node 4 name: " << sth << std::endl;
    // put(vertex_all, g, v3, CNode{3, "Node 3"});
    // auto sth = get(vertex_name, g, v4);
    // auto sth1 = get(vertex_all, g, v4);
    // std::cout << "Node 3 name: " << sth << std::endl;
    // put(vertex_name, g, v1, "Node 1");
    // put(vertex_name, g, v2, "Node 2");
    // auto sth = get(vertex_all, g, v1);
    // std::cout << "Node 1 name: " << sth << std::endl;


    return 0;
}