# 🌊 RedR — Unified Hydrodynamic Flood Simulator & Real-time AI Risk Engine

An end-to-end disaster management and dam safety surveillance platform developed for the **Smart India Hackathon (SIH)**.

RedR merges a high-performance **C++ Qt 6 / QML Hardware-Accelerated Desktop GIS Client** with a continuous **Automated Meteorological Surveillance & ML Dam Breach Prediction Engine** powered by FastAPI, XGBoost/LightGBM, and Modal Serverless GPU cloud computing.

---

## 🏛️ System Architecture

```
SIH_merge / RedR/
├── start.sh                      # Unified one-command launcher (FastAPI + Qt 6)
├── .gitignore                    # Project-wide ignore rules
├── backend/                      # HydroGuard-AI Risk Engine & FastAPI Bridge
│   ├── src/
│   │   ├── api.py                # FastAPI REST endpoints & Modal bridge
│   │   ├── predictor.py          # ML inference engine with domain physics guardrails
│   │   ├── feature_pipeline.py   # Rational Method inflow, ORI, DBI feature extraction
│   │   └── data_generator.py     # Physics-informed synthetic breach dataset generator
│   ├── modal_training/
│   │   └── train_gpu.py          # Modal serverless GPU training & deployment
│   ├── dam_dataset/
│   │   └── dam.geojson           # National database of 6,600+ Indian dams
│   ├── models/
│   │   ├── active_surveillance.json  # Cached nationwide weather surveillance states
│   │   └── training_metrics.json     # Model calibration metrics (Brier score & AUC)
│   ├── tests/
│   │   └── test_real_world_suite.py  # 16-scenario verification test suite
│   ├── surveillance_daemon.py    # Background Open-Meteo cluster weather poller
│   └── requirements.txt          # Python dependencies (FastAPI, GeoPandas, Modal, etc.)
└── frontend/                     # RedR C++ Qt 6 & QML Desktop Application
    ├── CMakeLists.txt            # Qt 6 build system
    ├── src/
    │   ├── main.cpp              # Application entry point
    │   ├── core/                 # DamManager, OSM parser, Tile cache, Weather managers
    │   ├── renderer/             # GPU vector tile renderer, MapStyle, Label placement
    │   └── ui/                   # MonitorWidget (Unified Surveillance & ML),
    │                             # AnalyticsWidget, OnlineTileWidget, HelicopterTracker
    ├── qml/                      # Hardware-accelerated Qt Quick UI components
    └── screenshots/              # System architecture and UI previews
```

---

## ⚡ Key Capabilities

### 1. Unified Surveillance & ML Prediction HUD
* **One-Click Execution**: A single unified button (`⚡ Run Full Surveillance & ML Prediction`) initiates nationwide Open-Meteo NWP radar scans across all Indian dams while simultaneously running deep physics-informed ML breach inference for the selected reservoir.
* **Physics Domain Guardrails**: Evaluates engineered vs. non-engineered dams against CWC & IS 11223 standards, computing the Overtopping Risk Index (ORI), Dam Breach Index (DBI), Rational Method Inflow Surge ($Q_{in}$), and crest displacement.
* **SHAP Explainability**: Instant breakdown of top breach drivers (extreme rainfall, reduced freeboard, structural deformation).

### 2. High-Performance C++ Qt 6 GIS Client
* Minimalist dark theme inspired by `shadcn/ui` with zinc/slate color tokens (`#18181B` surfaces, `#27272A` borders).
* Dynamic 60-minute downstream flood wave inundation simulation.
* Real-time Helicopter ADS-B/OpenSky search-and-rescue telemetry tracking.
* High-speed vector tile rendering with 360° rotation, geodesic ruler, and spatial frustum culling.

### 3. Serverless Cloud & Edge Scalability
* Direct fallback to **Modal Serverless GPU cloud** (`predict_breach_web` & `batch_predict_breach_web`) whenever local daemon endpoints are unreachable.
* Offline caching for high-speed operation during crisis communication blackouts.

---

## 🚀 Quick Start

### Prerequisites
* **Linux** (Ubuntu 22.04+ recommended)
* **C++17 Compiler** (`g++` or `clang`)
* **CMake** $\ge$ 3.16 & **Qt 6** (Core, Gui, Widgets, Quick, Network)
* **Python** 3.10+

### One-Command Launch
To start both the FastAPI Risk Gateway and the Qt 6 Desktop client:
```bash
./start.sh
```

---

## 🧪 Testing

Run the full real-world disaster suite (16 comprehensive test cases including Bhale Dam, Rishi Ganga, Machchhu-II, and Kaddam Dam):
```bash
cd backend
venv/bin/python -m pytest tests/test_real_world_suite.py -v
```
