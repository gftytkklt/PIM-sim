#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
template <typename GraphType>
void Scheduler<GraphType>::schedule() {
    // print path set num
    std::cout << "Path num: " << path_set->size() << std::endl;
    init_bce();
    // print bce
    for (const auto& [key, val] : bce_map) {
        std::cout << "Edge id: " << key << " BCE: " << val << std::endl;
    }
}

template <typename GraphType>
void Scheduler<GraphType>::init_bce() {
    // traverse all paths
    // calculate edge bce only, so brandes algorithm can be simplified
    for (const auto& path : *path_set) {
        // print path info
        std::cout << "Path: " << path.id << " from " << path.src.first << "," << path.src.second << " to " << path.dst.first << "," << path.dst.second << std::endl;
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
                auto sigma_wt = shortest_path_num(w_tile, path.dst);
                double delta = static_cast<double>(sigma[v]) * sigma_wt / sigma[dst] * data_volume;
                // do not div by 2 because prev property do not commute
                bce_map[edge_map[UnorderedPair{v, w}]] += delta;
            }
        }
    }
}
#endif