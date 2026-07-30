#pragma once
#include "graph.h"
#include <vector>
#include <utility>

/*
 * Dijkstra's Single-Source Shortest Path
 * ────────────────────────────────────────
 * Data Structure : Min-Heap (priority_queue with greater<>)
 * Time Complexity: O((V + E) log V)
 * Space Complexity: O(V)
 *
 * Returns dist[] and parent[] arrays from a given source node.
 * The Graph::precompute() method calls this for every node.
 */

struct DijkstraResult {
    std::vector<int> dist;    // dist[v] = shortest distance from src to v
    std::vector<int> parent;  // parent[v] = previous node on shortest path
};

// Run Dijkstra from source node `src` on the given graph
DijkstraResult dijkstra(const Graph& g, int src);

// Reconstruct the path from src to dst using the parent array
std::vector<int> reconstructPath(const std::vector<int>& parent, int src, int dst);
