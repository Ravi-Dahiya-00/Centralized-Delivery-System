<div align="center">
  <h1>🚀 Centralized Delivery System (CDS)</h1>
  <p><b>Next-Gen Food Delivery Route Optimization & Batching Simulation</b></p>
  
  <p>
    <a href="#the-problem">The Problem</a> •
    <a href="#the-solution">The Solution</a> •
    <a href="#features">Features</a> •
    <a href="#algorithms-used">Algorithms</a> •
    <a href="#getting-started">Getting Started</a> •
    <a href="#contributing">Contributing</a>
  </p>
</div>

---

## 🚨 The Problem: Why Food Delivery is Broken
Currently, gig-economy food delivery operates in fragmented silos (e.g., Zomato, Swiggy, UberEats). This creates massive inefficiencies:
- **Redundant Routes:** Five riders from different platforms might deliver to the same neighborhood simultaneously, burning 5x the fuel.
- **Rider Burnout:** Single-order dispatches lead to low earnings per kilometer.
- **High Carbon Footprint:** Unoptimized routing directly contributes to urban traffic congestion and emissions.
- **High Operational Costs:** Platforms bleed money subsidizing inefficient single deliveries during rush hours.

## 💡 The Solution: Centralized Batching
What if all orders, regardless of the platform, were pooled into a **centralized network**? 

The **Centralized Delivery System (CDS)** is a high-performance simulation engine that proves the mathematical efficiency of cross-platform batching. By intelligently grouping orders heading in similar directions and assigning them to a shared fleet, CDS minimizes total travel distance, drastically reduces fuel consumption, and significantly boosts rider earnings per hour.

---

## ✨ Features

- **⚡ High-Performance C++ Backend**: Handles real-time graph routing (Dijkstra), dynamic order generation, and state management without breaking a sweat.
- **🧠 Smart Batching Algorithm**: Solves mini-TSP (Traveling Salesperson Problem) to calculate optimized multi-stop routes ensuring pickups always happen before deliveries.
- **🗺️ Live React Dashboard**: Watch the algorithm work in real-time. A visual map animates riders as they navigate the city graph.
- **📈 Rush-Hour Dynamics**: Order volume dynamically scales to replicate realistic peak hours (lunch/dinner rushes).
- **🚀 Turbo Mode (Stress Testing)**: Instantly fast-forward up to 24 hours of traffic to stress-test the batching algorithm and generate large-scale analytics in seconds.
- **📊 Real-Time Analytics**: Tracks the metrics that matter: fuel saved, distance saved, earnings increases, and system-wide batch efficiency.

---

## 🧮 Algorithms Used

This project is a masterclass in applied Data Structures and Algorithms (DSA):

| Concept | Implementation | Purpose |
|---|---|---|
| **Dijkstra's Algorithm** | `std::priority_queue` (Min-Heap) | Finding the absolute shortest path between any two intersections. |
| **All-Pairs Shortest Path** | Distance Matrix | Precomputing paths so routing decisions happen in `O(1)` time. |
| **Mini-TSP (Brute-force)** | Permutations | Calculating the optimal multi-stop order for a batched delivery. |
| **Greedy Bipartite Matching** | Heuristics | Pairing compatible pending orders to the nearest available rider. |
| **Graph Representation** | Adjacency List | Modeling the city grid, roads, shortcuts, and node weights. |

---

## 🛠️ Getting Started

### Prerequisites
- **Windows**: The backend includes a `build.ps1` script for compiling via MinGW-w64 (`g++` / C++17).
- **Node.js**: Required for running the Vite frontend (v16+).

### 1. Build & Run the Backend
```powershell
cd backend
.\build.ps1
.\delivery_server.exe
```
*The server will start on `http://localhost:8081`*

### 2. Run the Frontend
```powershell
cd frontend
npm install
npm run dev
```
*Open `http://localhost:5173` in your browser.*

---

## 🎮 How to Use the Dashboard

1. **Start the Engine**: Navigate to the **⚙️ Controls** tab and click **▶ Start**.
2. **Watch the Map**: Switch to the **🗺️ Live Map** to watch riders execute complex batch routes.
3. **Speed it Up**: Adjust the simulation speed (up to 10x) to simulate a full day faster.
4. **Turbo Mode**: Click **⚡ Fast-Forward** to simulate hours of deliveries instantly. 
5. **View Analytics**: Check the **📊 Analytics** tab to see the exact percentage of distance and fuel saved compared to standard single-dispatch routing.

---

## 🤝 Contributing (Pull Requests Welcome!)

**This project is 100% open-source, and we are actively looking for contributors!** 

Whether you're a C++ wizard, a React expert, or a DSA enthusiast, we'd love your help. Here are some areas where you can contribute:
- 🚀 **Algorithm Optimization**: Can you write a faster heuristic for the batching matching?
- 🎨 **UI/UX**: Help make the React map and dashboard even more beautiful.
- 🐳 **Dockerization**: Help us containerize the backend and frontend for easier cross-platform deployment.
- 📊 **More Analytics**: Add new metrics like average delivery time, thermal loss, or carbon footprint savings.

### How to Contribute
1. **Fork** the repository.
2. Create your feature branch (`git checkout -b feature/AmazingFeature`).
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`).
4. Push to the branch (`git push origin feature/AmazingFeature`).
5. Open a **Pull Request**! We review PRs quickly.

---

<div align="center">
  <p>Built with ❤️ by a passionate developer bridging the gap between Data Structures and real-world logistics.</p>
</div>
