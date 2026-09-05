# 🌊 HydroGuard-AI

**Automated Real-Time Meteorological Surveillance & Dam Failure Probability Prediction System**

> Smart India Hackathon (SIH) — Disaster Management & Early Warning

---

## 🎯 Overview

HydroGuard-AI is an end-to-end system that continuously monitors live weather data, detects localized cloudbursts, identifies at-risk dams (both engineered and natural landslide barriers), and predicts **dam breach probability** using a calibrated XGBoost + LightGBM ensemble trained on physics-informed synthetic data.

### Key Features
- 🌧️ **Real-Time Weather Surveillance** — Polls Open-Meteo API every 15-30 mins to detect cloudbursts (≥20 mm/hr)
- 🗺️ **Spatial Catchment Analysis** — GeoPandas-powered catchment intersection to identify at-risk dams
- 🧮 **Physics-Informed Features** — Rational Method inflow, Overtopping Risk Index (ORI), Stefanelli DBI
- 🤖 **ML Breach Prediction** — Calibrated ensemble producing P(breach) ∈ [0,1] with SHAP explainability
- 📊 **Interactive Dashboard** — Streamlit + Folium map with real-time risk visualization
- 📤 **Automated Exports** — KML/SHP files for NDRF/HADR dispatch

---

## 🚀 Quick Start

### 1. Install Dependencies
```bash
python -m venv venv
source venv/bin/activate  # Linux/Mac
pip install -r requirements.txt
```

### 2. Generate Synthetic Training Data
```bash
python -m src.data_generator
```

### 3. Train Model on Modal GPU
```bash
# First-time setup
pip install modal
modal setup  # Authenticate with Modal.com

# Launch GPU training
python modal_training/run_training.py

# Download trained model
python modal_training/download_model.py
```

### 4. Launch Dashboard
```bash
streamlit run dashboard/app.py
```

---

## 🏗️ Architecture

```
Layer 1: Weather Surveillance → Open-Meteo API polling, cloudburst detection
Layer 2: Spatial Engine       → GeoPandas catchment intersection, dam lookup
Layer 3: Feature Pipeline     → ORI, DBI, inflow surge computation
Layer 4: ML Predictor         → XGBoost + LightGBM ensemble + SHAP
Layer 5: Dashboard & Export   → Streamlit UI, KML/SHP for NDRF
```

---

## 📁 Project Structure

```
BG_model/
├── data/                      # Dam registry & training data
├── models/                    # Trained model artifacts
├── src/                       # Core engine modules
├── modal_training/            # Modal GPU training pipeline
├── dashboard/                 # Streamlit UI
├── requirements.txt
├── Dockerfile
└── README.md
```

---

## 🔬 Case Studies

| Scenario | Location | P(breach) | Alert Level |
|:---|:---|:---:|:---|
| Rishi Ganga 2021 | Chamoli, Uttarakhand | **0.94** | 🔴 IMMINENT |
| Phuktal River 2015 | Zanskar, Ladakh | **0.81** | 🟠 WARNING |
| Tehri Dam (nominal) | Uttarakhand | **0.08** | 🟢 NORMAL |

---

## 📄 License

This project is developed for the Smart India Hackathon (SIH). All rights reserved.
