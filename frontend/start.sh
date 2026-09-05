#!/usr/bin/env bash
set -e

# ==============================================================================
#  RedR - Hydrodynamic Flood Simulation & Real-time Risk Engine
#  Starts FastAPI Vector Tile Server and the C++ Qt 6 Desktop Map Application
# ==============================================================================

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

# Color Codes
GREEN="\033[0;32m"
BLUE="\033[0;34m"
YELLOW="\033[1;33m"
CYAN="\033[0;36m"
RED="\033[0;31m"
BOLD="\033[1m"
NC="\033[0m"

echo -e "${CYAN}${BOLD}"
echo "=================================================================="
echo "         🌊  RedR - FLOOD SIMULATOR & RISK ENGINE LAUNCHER        "
echo "=================================================================="
echo -e "${NC}"

SERVER_PID=""

# Cleanup function to kill background server on exit
cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "\n${YELLOW}[*] Shutting down FastAPI Vector Tile Server (PID: $SERVER_PID)...${NC}"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        echo -e "${GREEN}[✓] Server stopped gracefully.${NC}"
    fi
}
trap cleanup EXIT INT TERM

# ------------------------------------------------------------------------------
# 1. Setup Python Virtual Environment for FastAPI Tile Server
# ------------------------------------------------------------------------------
echo -e "${BLUE}[1/3] Checking FastAPI Backend Environment...${NC}"
if [ ! -d "server/venv" ]; then
    echo -e "      Creating Python virtual environment in server/venv..."
    python3 -m venv server/venv
fi

if ! server/venv/bin/python -c "import fastapi, aiosqlite, uvicorn" 2>/dev/null; then
    echo -e "      Installing backend dependencies..."
    server/venv/bin/pip install -r server/requirements.txt
fi
echo -e "${GREEN}[✓] Backend environment ready.${NC}"

# ------------------------------------------------------------------------------
# 2. Start FastAPI Tile Server in Background
# ------------------------------------------------------------------------------
echo -e "${BLUE}[2/3] Starting FastAPI Vector Tile Server on http://localhost:8000...${NC}"
server/venv/bin/python -m uvicorn server.main:app --host 0.0.0.0 --port 8000 > server/server.log 2>&1 &
SERVER_PID=$!

# Wait for server health endpoint to be available
MAX_RETRIES=15
COUNTER=0
SERVER_READY=false

while [ $COUNTER -lt $MAX_RETRIES ]; do
    if curl -s http://localhost:8000/health > /dev/null 2>&1; then
        SERVER_READY=true
        break
    fi
    sleep 0.4
    COUNTER=$((COUNTER + 1))
done

if [ "$SERVER_READY" = true ]; then
    echo -e "${GREEN}[✓] FastAPI Vector Tile Server online at http://localhost:8000 (PID: $SERVER_PID)${NC}"
    echo -e "    - Health Check : http://localhost:8000/health"
    echo -e "    - TileJSON 3.0 : http://localhost:8000/tilejson.json"
    echo -e "    - Dark Style   : http://localhost:8000/style.json"
else
    echo -e "${YELLOW}[!] Tile server starting in background (log: server/server.log)${NC}"
fi

# ------------------------------------------------------------------------------
# 3. Build & Launch C++ Qt 6 Desktop Map Client
# ------------------------------------------------------------------------------
echo -e "${BLUE}[3/3] Preparing RedR Desktop Application...${NC}"

if [ ! -f "build/RedR" ] && [ ! -f "build/assam_map" ]; then
    echo -e "      Building application with CMake..."
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j"$(nproc)"
    cd "$PROJECT_DIR"
fi

echo -e "${GREEN}${BOLD}[✓] Launching RedR Desktop Application (Full Dark Theme)...${NC}"
echo -e "${CYAN}    Controls:${NC}"
echo -e "    - Pan            : Left Click + Drag / Arrow keys / W, A, S, D"
echo -e "    - Zoom           : Mouse Wheel / Double Click / + / -"
echo -e "    - Inspect Feature: Click on any dam, road, city, river, or POI"
echo -e "    - Search         : Floating search bar at top-left"
echo -e "    - Measurement    : Click 📏 button"
echo -e "=================================================================="

# Execute Desktop App
if [ -f "./build/RedR" ]; then
    ./build/RedR
else
    ./build/assam_map
fi

