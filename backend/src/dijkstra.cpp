#include "../include/dijkstra.h"
#include <queue>
#include <vector>
#include <algorithm>  // std::reverse
#include <limits>

/*
 * Dijkstra's Algorithm — Implementation
 * ──────────────────────────────────────
 * Data structure: std::priority_queue with a min-heap comparator (greater<>)
 *
 * Each element in the heap is a pair<int, int>: {distance, node_id}
 * We use the distance as the priority key (min-distance first).
 *
 * Algorithm:
 *  1. Initialize dist[src] = 0, all others = INF
 *  2. Push {0, src} into the min-heap
 *  3. While heap is not empty:
 *     a. Pop the node u with minimum distance
 *     b. If dist[u] < popped distance, skip (stale entry)
 *     c. For each neighbor v of u:
 *        - If dist[u] + edge_weight < dist[v]:
 *             Update dist[v], set parent[v] = u
 *             Push {dist[v], v} into heap
 *
 * Time Complexity : O((V + E) log V)
 * Space Complexity: O(V)
 */

DijkstraResult dijkstra(const Graph& g, int src) {
    int n = g.nodeCount();

    DijkstraResult result;
    result.dist.assign(n, Graph::INF);
    result.parent.assign(n, -1);

    result.dist[src] = 0;

    // Min-heap: {distance, node_id}
    using PII = std::pair<int, int>;
    std::priority_queue<PII, std::vector<PII>, std::greater<PII>> minHeap;
    minHeap.push({0, src});

    while (!minHeap.empty()) {
        int d = minHeap.top().first;
        int u = minHeap.top().second;
        minHeap.pop();

        // Skip stale heap entries (lazy deletion)
        if (d > result.dist[u]) continue;

        // Relax all outgoing edges from u
        auto it = g.adj.find(u);
        if (it == g.adj.end()) continue;

        for (const Edge& edge : it->second) {
            int v = edge.to;
            int w = edge.weight;

            if (result.dist[u] + w < result.dist[v]) {
                result.dist[v]   = result.dist[u] + w;
                result.parent[v] = u;
                minHeap.push({result.dist[v], v});
            }
        }
    }

    return result;
}

// ─── Reconstruct path from parent array ──────────────────────
std::vector<int> reconstructPath(const std::vector<int>& parent, int src, int dst) {
    std::vector<int> path;
    if (parent[dst] == -1 && dst != src) return path;  // unreachable

    for (int v = dst; v != -1; v = parent[v])
        path.push_back(v);

    std::reverse(path.begin(), path.end());
    return path;
}
