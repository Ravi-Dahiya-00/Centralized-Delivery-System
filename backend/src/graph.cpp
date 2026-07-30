#include "../include/graph.h"
#include "../include/dijkstra.h"
#include "../vendor/json.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>

const int Graph::INF;

using json = nlohmann::json;

// ─── NodeType helpers ─────────────────────────────────────────
std::string nodeTypeToString(NodeType t) {
    switch (t) {
        case NodeType::INTERSECTION: return "INTERSECTION";
        case NodeType::ZOMATO:       return "ZOMATO";
        case NodeType::SWIGGY:       return "SWIGGY";
        case NodeType::BOTH:         return "BOTH";
        case NodeType::HOUSE:        return "HOUSE";
        default:                     return "UNKNOWN";
    }
}

static NodeType nodeTypeFromString(const std::string& s) {
    if (s == "ZOMATO")       return NodeType::ZOMATO;
    if (s == "SWIGGY")       return NodeType::SWIGGY;
    if (s == "BOTH")         return NodeType::BOTH;
    if (s == "HOUSE")        return NodeType::HOUSE;
    return NodeType::INTERSECTION;
}

// ─── Graph: add node/edge ─────────────────────────────────────
void Graph::addNode(const Node& n) {
    nodes.push_back(n);
    if (adj.find(n.id) == adj.end())
        adj[n.id] = {};
}

void Graph::addEdge(int from, int to, int weight) {
    adj[from].push_back({to,   weight});
    adj[to  ].push_back({from, weight});
}

// ─── Precompute all-pairs shortest paths ─────────────────────
// Runs Dijkstra from every node → fills dist[][] and next[][]
void Graph::precompute() {
    int n = nodeCount();
    dist.assign(n, std::vector<int>(n, INF));
    next.assign(n, std::vector<int>(n, -1));

    for (int src = 0; src < n; ++src) {
        DijkstraResult res = dijkstra(*this, src);
        for (int dst = 0; dst < n; ++dst) {
            dist[src][dst] = res.dist[dst];
        }
        // Reconstruct next-hop table from parent array
        for (int dst = 0; dst < n; ++dst) {
            if (dst == src || res.dist[dst] == INF) continue;
            // Walk backwards from dst to find the hop right after src
            int cur = dst;
            while (res.parent[cur] != src && res.parent[cur] != -1)
                cur = res.parent[cur];
            next[src][dst] = cur;
        }
    }
    std::cout << "[Graph] Precomputed all-pairs shortest paths for "
              << n << " nodes.\n";
}

// ─── Reconstruct full path from src → dst ────────────────────
std::vector<int> Graph::getPath(int src, int dst) const {
    if (src == dst) return {src};
    if (next[src][dst] == -1) return {};   // unreachable

    std::vector<int> path = {src};
    int cur = src;
    while (cur != dst) {
        cur = next[cur][dst];
        path.push_back(cur);
        if ((int)path.size() > (int)nodes.size() + 1) {
            // Cycle guard
            return {};
        }
    }
    return path;
}

int Graph::distance(int u, int v) const {
    if (u < 0 || v < 0 || u >= (int)dist.size() || v >= (int)dist.size())
        return INF;
    return dist[u][v];
}

// ─── Load graph from JSON file ────────────────────────────────
Graph Graph::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open city_graph.json: " + path);

    json j;
    f >> j;

    Graph g;

    for (const auto& jn : j["nodes"]) {
        Node n;
        n.id   = jn["id"].get<int>();
        n.name = jn["name"].get<std::string>();
        n.type = nodeTypeFromString(jn["type"].get<std::string>());
        n.x    = jn["x"].get<double>();
        n.y    = jn["y"].get<double>();
        g.addNode(n);
    }

    for (const auto& je : j["edges"]) {
        int from   = je["from"].get<int>();
        int to     = je["to"].get<int>();
        int weight = je["weight"].get<int>();
        g.addEdge(from, to, weight);
    }

    g.precompute();
    return g;
}
