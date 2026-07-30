# 🚀 Centralized Delivery System
### DSA College Project | C++ Backend + React Frontend

A simulated cross-platform food delivery optimization system demonstrating:
- **Dijkstra's Algorithm** for shortest path routing
- **Brute-force mini-TSP** for multi-stop route ordering (pickup-before-delivery constraint)
- **Greedy batch matching** for pairing compatible orders
- **Priority Queue (min-heap)** as the core of Dijkstra
- Live animated city map with rider movement
- Analytics: earnings boost, distance saved, fuel saved (per rider + system total)

---

## 📁 Project Structure
```
Centralized Delivery System/
├── backend/          ← C++ server (DSA algorithms + REST API)
│   ├── main.cpp
│   ├── include/      ← Headers
│   ├── src/          ← Implementations
│   ├── vendor/       ← httplib.h, json.hpp (single-header libs)
│   ├── data/
│   │   └── city_graph.json
│   └── CMakeLists.txt
└── frontend/         ← React + Vite UI
    ├── src/
    │   ├── App.jsx
    │   ├── components/
    │   │   ├── CityMap.jsx          ← Animated SVG city map
    │   │   ├── OrderTable.jsx       ← Order management
    │   │   ├── AnalyticsDashboard.jsx ← Earnings/savings charts
    │   │   └── ControlPanel.jsx     ← Simulation controls + turbo mode
    │   ├── store/useStore.js        ← Zustand state
    │   ├── api/api.js               ← HTTP client
    │   └── hooks/useSimulation.js  ← 500ms polling hook
    └── package.json
```

---

## 🛠️ How to Build & Run

### Step 1: Build C++ Backend

**Requires:** CMake 3.16+, g++ / MSVC with C++17

```powershell
# From project root
cd backend

# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build . --config Release

# Run (from build directory — data/ is copied automatically)
./delivery_server       # Linux/Mac
.\delivery_server.exe   # Windows
```

The server starts on **http://localhost:8080**

---

### Step 2: Run Frontend

```powershell
cd frontend
npm install     # first time only
npm run dev
```

Open **http://localhost:5173** in your browser.

---

## 🎮 How to Use

1. Open the browser → go to **⚙️ Controls** tab
2. Click **▶ Start** to begin live simulation
3. Watch **🗺️ Live Map** — riders animate along routes
4. Check **📋 Orders** — filter by status/platform
5. View **📊 Analytics** — per-rider earnings/distance/fuel savings

### Fast-Forward Mode
- Go to **⚙️ Controls** → Fast-Forward section
- Select hours (1–24), click **⚡ Fast-Forward**
- Instantly processes N hours of orders + deliveries
- Analytics immediately reflects the full period

### Speed Control
- 1× = real time
- 2×, 5×, 10× = accelerated simulation

---

## 🧮 DSA Algorithms Used

| Algorithm | Location | Purpose |
|---|---|---|
| **Dijkstra's (Min-Heap)** | `src/dijkstra.cpp` | Shortest path between any 2 nodes |
| **All-Pairs Shortest Path** | `src/graph.cpp` | Precomputed distance matrix |
| **Brute-force Permutation** | `src/route_optimizer.cpp` | Optimal multi-stop ordering (mini-TSP) |
| **Greedy Matching** | `src/order_manager.cpp` | Batch 2 compatible orders together |
| **Priority Queue** | `src/dijkstra.cpp` | `std::priority_queue<pair<int,int>, vector, greater<>>` |
| **Adjacency List** | `src/graph.cpp` | `unordered_map<int, vector<Edge>>` |
| **Queue** | `src/order_manager.cpp` | Pending order queue |

---

## 📊 Analytics Metrics

### Per Rider:
- **Earnings Increased By** = actual earnings − solo baseline earnings
- **Earnings Increase %** = (increase / solo baseline) × 100
- **Distance Saved** = solo distance − actual distance traveled
- **Avg Distance Saved** = distance saved / orders delivered
- **Fuel Saved (₹)** = distance saved × ₹1.8/km

### System Total:
- All above metrics summed across all riders
- **Batch Rate %** = batched orders / total orders × 100
- **Orders per Hour** = total delivered / sim hours

---

## ⚙️ Configuration (city_graph.json → config section)

```json
{
  "config": {
    "max_orders_per_batch": 2,
    "max_batch_pickup_distance": 300,
    "max_delivery_time_seconds": 2700,
    "extra_distance_threshold_pct": 30,
    "order_base_pay_inr": 35,
    "distance_bonus_per_km_inr": 2.0,
    "fuel_cost_per_km_inr": 1.8,
    "order_interval_seconds": 12,
    "sim_tick_ms": 800
  }
}
```

---

## 🏙️ City Map

- **38 nodes**: 12 intersections, 5 Zomato restaurants, 5 Swiggy restaurants, 4 both-platform restaurants, 12 customer houses
- **~65 edges**: Grid roads + diagonal shortcuts with meter-based weights
- **4 riders**: Centralized pool (not platform-specific)
