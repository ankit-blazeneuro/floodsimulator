# 🗺️ MapLibre Dark Explorer - High Performance Vector Map (FastAPI + Qt 6 QML)

A high-performance, full-stack mapping platform featuring:
1. **FastAPI Vector Tile Server**: Asynchronous MBTiles reader built on `aiosqlite` with TileJSON 3.0, XYZ-to-TMS coordinate projection, gzip encoding, and dark style generation.
2. **C++ Qt 6 & Qt Quick (QML) Desktop Client**: Modern desktop UI inspired by the minimalist dark aesthetic of **`shadcn/ui`** (zinc/slate color tokens, `#18181B` translucent surfaces, `#27272A` borders, `#FAFAFA` typography, custom toggles, and buttons).
3. **MapLibre Native for Qt Engine**: Hardware-accelerated GPU vector tile rendering with pitch, bearing, and zoom controls.

---

## 🏛️ System Architecture

```
SIH/
├── CMakeLists.txt              # Qt 6 & QML build configuration
├── README.md                   # Setup and execution guide
├── data/
│   ├── assam-latest.osm.pbf    # OpenStreetMap raw PBF dataset
│   ├── assam_map.bin           # Native high-speed binary cache
│   └── map.mbtiles             # Vector tile database (Generated via Planetiler)
├── server/
│   ├── main.py                 # FastAPI vector tile server with TMS conversion
│   ├── requirements.txt        # Server dependencies (FastAPI, aiosqlite, uvicorn)
│   └── venv/                   # Python virtual environment
├── qml/
│   ├── Main.qml                # Root application window with dark MapView & floating HUD
│   ├── ShadcnButton.qml        # Dark modern button component (variants, hover/press animations)
│   ├── ShadcnSwitch.qml        # Dark toggle switch with smooth spring animations
│   └── qml.qrc                 # Qt resource bundle
└── src/
    ├── main.cpp                # QGuiApplication entry point & QML engine loader
    ├── core/                   # Core spatial index, PBF parser, and binary cache
    ├── renderer/               # Vector rendering engine & dark theme palettes
    └── ui/                     # Native Qt desktop widgets & controls
```

---

## 🚀 Part 1: Generating `map.mbtiles` with Planetiler

To convert OpenStreetMap `.osm.pbf` files into vector tile `.mbtiles` at high speed:

### Option A: Using Docker (Recommended)
```bash
cd /home/ankit/Desktop/SIH
docker run -e JAVA_TOOL_OPTIONS="-Xmx4g" \
  -v "$(pwd)/data":/data \
  ghcr.io/onthegomap/planetiler:latest \
  --osm-path=/data/assam-latest.osm.pbf \
  --output=/data/map.mbtiles
```

### Option B: Using Standalone Java JAR
```bash
wget https://github.com/onthegomap/planetiler/releases/latest/download/planetiler.jar
java -Xmx4g -jar planetiler.jar \
  --osm-path=data/assam-latest.osm.pbf \
  --output=data/map.mbtiles
```

---

## ⚡ Part 2: Running the FastAPI Vector Tile Server

The backend reads from `data/map.mbtiles` and serves vector tiles at `http://localhost:8000`.

### 1. Setup Virtual Environment
```bash
cd /home/ankit/Desktop/SIH
python3 -m venv server/venv
source server/venv/bin/activate
pip install -r server/requirements.txt
```

### 2. Start the Server
```bash
source server/venv/bin/activate
python -m uvicorn server.main:app --host 0.0.0.0 --port 8000 --reload
```

### 3. API Endpoints
- **Health Check & Metadata**: `GET http://localhost:8000/health`
- **TileJSON 3.0 Specification**: `GET http://localhost:8000/tilejson.json`
- **Vector Tile (Protobuf)**: `GET http://localhost:8000/tiles/{z}/{x}/{y}.pbf`
  - Automatically converts Slippy Map XYZ $y$ coordinate to TMS $y$ coordinate ($y_{TMS} = 2^z - 1 - y$).
  - Serves with `Content-Type: application/x-protobuf` and `Content-Encoding: gzip`.
- **Dark Theme Vector Style**: `GET http://localhost:8000/style.json`

---

## 🖥️ Part 3: Compiling & Running the C++ Qt 6 Desktop Client

### 1. Install Qt 6 and MapLibre Dependencies (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qml6-module-qtquick \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts
```

### 2. Fetch & Install MapLibre Native for Qt (`QMapLibre`)
```bash
# Clone and build MapLibre Native for Qt
git clone --recurse-submodules https://github.com/maplibre/maplibre-native-qt.git
cd maplibre-native-qt
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja
sudo ninja install
```

### 3. Build the Application
```bash
cd /home/ankit/Desktop/SIH
mkdir -p build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### 4. Launch the Client
Ensure the FastAPI server is running on port `8000`, then launch:
```bash
./build/maplibre_dark_explorer
```

---

## 🎨 shadcn/ui Dark Aesthetic Features

- **Palette**: Strict Dark Mode using Tailwind / shadcn `zinc` tokens (`#09090B` window, `#18181B` translucent card surface, `#27272A` borders, `#FAFAFA` primary text, `#A1A1AA` secondary text).
- **Floating HUD**: Top-right floating control panel displaying real-time coordinate telemetry, layer toggles (`ShadcnSwitch`), camera presets (`ShadcnButton`), and server health badge (`#10B981` LIVE).
- **Full Interactivity**: Smooth pan, continuous cursor-anchored zoom, 3D tilt, and orientation reset.
