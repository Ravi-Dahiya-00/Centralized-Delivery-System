#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <limits>

// ─── Node Types ──────────────────────────────────────────────
enum class NodeType {
    INTERSECTION,
    ZOMATO,       // Zomato-only restaurant
    SWIGGY,       // Swiggy-only restaurant
    BOTH,         // Restaurant on both platforms
    HOUSE         // Customer location
};

std::string nodeTypeToString(NodeType t);

// ─── Graph Node ───────────────────────────────────────────────
struct Node {
    int    id;
    std::string name;
    NodeType    type;
    double x, y;   // SVG coordinates for map rendering
};

// ─── Graph Edge ───────────────────────────────────────────────
struct Edge {
    int to;
    int weight;   // distance in meters
};

// ─── Graph Class (Adjacency List) ─────────────────────────────
class Graph {
public:
    std::vector<Node>                         nodes;
    std::unordered_map<int, std::vector<Edge>> adj;

    // All-pairs shortest path distances (precomputed at startup)
    std::vector<std::vector<int>>  dist;   // dist[u][v] = shortest distance
    std::vector<std::vector<int>>  next;   // next[u][v] = next hop on shortest path from u to v

    void addNode(const Node& n);
    void addEdge(int from, int to, int weight);

    // Precompute all-pairs shortest paths using Dijkstra from each source
    void precompute();

    // Return the ordered list of nodes on shortest path from src to dst
    std::vector<int> getPath(int src, int dst) const;

    // Distance between two nodes (precomputed)
    int distance(int u, int v) const;

    int nodeCount() const { return (int)nodes.size(); }

    // Load graph from JSON file
    static Graph loadFromFile(const std::string& path);

    static const int INF = std::numeric_limits<int>::max() / 2;
};
