#!/usr/bin/env pwsh
# ─────────────────────────────────────────────────────────────────
# build.ps1 — Build the Centralized Delivery System C++ backend
# Usage: .\build.ps1
# ─────────────────────────────────────────────────────────────────

$GPP = "C:\Users\HP\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"

# Fallback: try system g++
if (-not (Test-Path $GPP)) {
    $cmd = Get-Command g++ -ErrorAction SilentlyContinue
    if ($cmd) { $GPP = $cmd.Source }
}
if (-not $GPP) {
    Write-Error "g++ not found. Install MinGW-w64 or WinLibs."
    exit 1
}

Write-Host "[Build] Using g++: $GPP"
& $GPP --version | Select-Object -First 1

$SRC = @(
    "main.cpp",
    "src/graph.cpp",
    "src/dijkstra.cpp",
    "src/route_optimizer.cpp",
    "src/order.cpp",
    "src/order_manager.cpp",
    "src/rider.cpp",
    "src/rider_manager.cpp",
    "src/simulator.cpp",
    "src/api_server.cpp"
)

$FLAGS = @(
    "-std=c++17",
    "-O2",
    "-Wall",
    "-static",
    "-Iinclude",
    "-Ivendor",
    "-o", "delivery_server.exe",
    "-lws2_32"   # Windows socket library (needed by httplib)
)

$CMD = @($GPP) + $SRC + $FLAGS

Write-Host "`n[Build] Compiling..."
Write-Host "[Build] Command: $($CMD -join ' ')"
Write-Host ""

$result = & $CMD[0] $CMD[1..($CMD.Length-1)] 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "`n[Build] ✅ Build successful! → delivery_server.exe" -ForegroundColor Green
    Write-Host "[Build] Copy data/ folder next to the exe if not present."
    Write-Host "[Build] Run: .\delivery_server.exe"
} else {
    Write-Host "`n[Build] ❌ Build failed:" -ForegroundColor Red
    $result | ForEach-Object { Write-Host $_ }
    exit 1
}
