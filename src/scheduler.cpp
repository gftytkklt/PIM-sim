#include "scheduler.h"

Scheduler::Scheduler(std::pair<int, int> tile_size) 
    : tile_size(tile_size) {
    // init graph
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            boost::add_vertex(SNode{i, j}, graph);
        }
    }
    // 2D mesh connection & init edge_map
    size_t id = 0;
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            size_t src, dst;
            if (i > 0) {
                src = i * tile_size.second + j;
                dst = (i - 1) * tile_size.second + j;
                boost::add_edge(src, dst, graph);
                edge_map[UnorderedPair{src, dst}] = id++;
            }
            if (j > 0) {
                src = i * tile_size.second + j;
                dst = i * tile_size.second + j - 1;
                boost::add_edge(src, dst, graph);
                edge_map[UnorderedPair{src, dst}] = id++;
            }
        }
    }
}

// std::vector<Path> Scheduler::schedule() {
SchedInfo Scheduler::schedule() {
    // print path set num
    // std::cout << "Path num: " << path_set->size() << std::endl;
    init_bce();
    // print bce
    // for (const auto& [key, val] : bce_map) {
    //     std::cout << "Edge id: " << key << " BCE: " << val << std::endl;
    // }
    // print path before
    // std::cout << "sch Path before:" << std::endl;
    // for (const auto& path : *path_set) {
    //     for (const auto& via : path.via) {
    //         std::cout << via.first << "," << via.second << " ";
    //     }
    //     std::cout << std::endl;
    // }
    congestion_aware_routing();
    long long total_congestion = 0;
    for (const auto& [key, val] : congestion_map) {
        total_congestion += val.getCSum();
    }
    // std::cout << "sch Path after:" << std::endl;
    // for (const auto& path : *path_set) {
    //     for (const auto& via : path.via) {
    //         std::cout << via.first << "," << via.second << " ";
    //     }
    //     std::cout << std::endl;
    // }
    // return *path_set;
    // return SchedInfo{total_congestion, *path_set};
    return SchedInfo{total_congestion, path_set};
}

// SchedInfo Scheduler::xy_routing() {
//     // don't care bce
//     congestion_map.clear();
//     // add congestion volume only
//     for (const auto& path : *path_set) {
//         // use default xy-routing
//         for (int i = 0; i < path.via.size() - 1; i++) {
//             auto src = xy_to_id(path.via[i]);
//             auto dst = xy_to_id(path.via[i + 1]);
//             auto edge_id = edge_map[UnorderedPair{src, dst}];
//             congestion_map[edge_id].insert(path.datavolume);
//         }
//     }
//     long long total_congestion = 0;
//     for (const auto& [key, val] : congestion_map) {
//         total_congestion += val.getCSum();
//     }
//     return SchedInfo{total_congestion, *path_set};
// }

// modified version
SchedInfo Scheduler::xy_routing() {
    // don't care bce
    congestion_map.clear();
    // add congestion volume only
    for (const auto& path : path_set) {
        // use default xy-routing
        for (int i = 0; i < path->via.size() - 1; i++) {
            auto src = xy_to_id(path->via[i]);
            auto dst = xy_to_id(path->via[i + 1]);
            auto edge_id = edge_map[UnorderedPair{src, dst}];
            congestion_map[edge_id].insert(path->datavolume);
        }
    }
    long long total_congestion = 0;
    for (const auto& [key, val] : congestion_map) {
        total_congestion += val.getCSum();
    }
    return SchedInfo{total_congestion, path_set};
}

void Scheduler::init_bce() {
    // traverse all paths
    // calculate edge bce only, so brandes algorithm can be simplified
    // clear bce_map first
    bce_map.clear();
    congestion_map.clear();
    // for (const auto& path : *path_set) {
    for (const auto& path_ptr : path_set) {
        auto& path = *path_ptr;
        // print path info
        // std::cout << "Path: " << path.id << " from " << path.src.first << "," << path.src.second << " to " << path.dst.first << "," << path.dst.second << std::endl;
        auto src = xy_to_id(path.src);
        auto dst = xy_to_id(path.dst);
        auto data_volume = path.datavolume;
        // BFS to find shortest path
        std::queue<Vertex> q;
        std::stack<Vertex> s;
        std::vector<int> dist(boost::num_vertices(graph), -1); // distance from src
        std::vector<int> sigma(boost::num_vertices(graph), 0); // shortest path count
        std::vector<std::vector<Edge>> prev(boost::num_vertices(graph)); // previous edge

        dist[src] = 0;
        sigma[src] = 1;
        q.push(src);
        // BFS process
        while(!q.empty()) {
            auto v = q.front();
            q.pop();
            s.push(v);
            // traverse all adjacent nodes
            for (const auto& e : boost::make_iterator_range(out_edges(v, graph))) {
                auto w = boost::target(e, graph);
                // relax
                if(dist[w] < 0) {
                    dist[w] = dist[v] + 1; // get dist(src,w)
                    q.push(w);
                }
                // find shortest path to w via v
                if(dist[w] == dist[v] + 1) {
                    sigma[w] += sigma[v]; // update shortest path count
                    prev[w].push_back(e);
                }
            }
        }
        // traverse nodes in reverse order
        while(!s.empty()) {
            auto w = s.top();
            s.pop();
            // update bce
            for (const auto& e : prev[w]) {
                auto v = boost::source(e, graph); // src node
                // if (v,w) is out of shortest path, skip
                auto w_tile = id_to_xy(w);
                auto m_st = manhattan_distance(path.src, path.dst);
                auto m_sw = manhattan_distance(path.src, w_tile);
                auto m_wt = manhattan_distance(w_tile, path.dst);
                if(m_sw + m_wt != m_st) { // in fact, m_sw + m_wt >= m_st
                    continue;
                }
                // update bce
                // delta = sigma(s,v) * sigma(w,t) / sigma(s,t)
                // while sigma(s,w) is known, sigma (w,t) need BFS from w
                // however, in 2D mesh case, sigma(w,t) can be calculated by manhattan distance
                // std::cout << "Edge: " << v << " -> " << w << " id: " << edge_map[UnorderedPair{v, w}] << std::endl;
                auto sigma_wt = shortest_path_num(w_tile, path.dst);
                double delta = static_cast<double>(sigma[v]) * sigma_wt / sigma[dst] * data_volume;
                // do not div by 2 because prev property do not commute
                auto edge_id = edge_map[UnorderedPair{v, w}];
                bce_map[edge_id] += delta;
            }
        }
    }
    // normalize bce
    // find max_element first
    auto max_iter = std::max_element(bce_map.begin(), bce_map.end(), 
        [](const auto& p1, const auto& p2) {
            return p1.second < p2.second;
        });
    auto max_bce = max_iter->second;
    // then normalize
    for (auto& [key, val] : bce_map) {
        val /= max_bce;
        // init congestion volume by 1
        congestion_map[key] = CSum{};
    }
}

void Scheduler::congestion_aware_routing() {
    // sort path by manhattan distance and data volume
    // path with min manhattan distance and max data volume is scheduled first
    // std::sort(path_set->begin(), path_set->end(), 
    // std::sort(path_set->begin(), path_set->end(),
    std::sort(path_set.begin(), path_set.end(),
        [](const auto& p1, const auto& p2) {
            // auto m1 = manhattan_distance(p1.src, p1.dst);
            // auto m2 = manhattan_distance(p2.src, p2.dst);
            auto m1 = manhattan_distance(p1->src, p1->dst);
            auto m2 = manhattan_distance(p2->src, p2->dst);
            if(m1 != m2) {
                return m1 < m2;
            }
            // return p1.datavolume > p2.datavolume;
            return p1->datavolume > p2->datavolume;
        });
    
    // schdeule path by path
    // for (auto& path : *path_set) {
    for (auto& path : path_set) {
        // print path src and dst
        // std::cout << path.src.first << "," << path.src.second << " -> " << path.dst.first << "," << path.dst.second << std::endl;
        // auto value = path.datavolume;
        auto value = path->datavolume;
        // auto src = xy_to_id(path.src);
        auto src = xy_to_id(path->src);
        // auto dst = xy_to_id(path.dst);
        auto dst = xy_to_id(path->dst);
        // auto x_range = std::make_pair(std::min(path.src.first, path.dst.first), std::max(path.src.first, path.dst.first));
        auto x_range = std::make_pair(std::min(path->src.first, path->dst.first), std::max(path->src.first, path->dst.first));
        // auto y_range = std::make_pair(std::min(path.src.second, path.dst.second), std::max(path.src.second, path.dst.second));
        auto y_range = std::make_pair(std::min(path->src.second, path->dst.second), std::max(path->src.second, path->dst.second));
        // weight calculator
        WeightCalculator weight_calculator{graph, edge_map, bce_map, congestion_map, tile_size, x_range, y_range, value};
        // constrained_dijkstra_visitor vis{path.src, path.dst, *this};
        // dijkstra shortest path
        std::vector<Vertex> pred(boost::num_vertices(graph));
        std::vector<double> dist(boost::num_vertices(graph), std::numeric_limits<double>::infinity());
        // auto weight_map = boost::weight_map(weight_calculator);
        auto weight_map = boost::make_function_property_map<Edge>(weight_calculator);
        // current path deployment
        boost::dijkstra_shortest_paths(graph, src, 
        boost::predecessor_map(&pred[0])
        .distance_map(&dist[0])
        .weight_map(weight_map));
        // .visitor(vis));
        std::vector<std::pair<int,int>> path_vec;
        for (auto v = dst; v != src; v = pred[v]) {
            path_vec.push_back(id_to_xy(v));
            auto edge_id = edge_map[UnorderedPair{pred[v], v}];
            congestion_map[edge_id].insert(value);
        }
        // path_vec.push_back(path.src);
        path_vec.push_back(path->src);
        std::reverse(path_vec.begin(), path_vec.end());
        // print path via
        // for (const auto& node : path.via) {
        //     std::cout << node.first << "," << node.second << " ";
        // }
        // std::cout << std::endl;
        // for (const auto& node : path_vec) {
        //     std::cout << node.first << "," << node.second << " ";
        // }
        // std::cout << std::endl;
        // path.via = path_vec;
        path->via = path_vec;
        // std::cout << std::endl;
    }
    // print path_set
    for (const auto& path : path_set) {
        for (const auto& node : path->via) {
            std::cout << node.first << "," << node.second << " ";
        }
        std::cout << std::endl;
    }
}