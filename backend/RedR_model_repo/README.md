---
language:
- en
license: mit
tags:
- climate
- dam-safety
- hydroguard-ai
- xgboost
- lightgbm
- disaster-response
- hydrological-forecasting
metrics:
- roc_auc
- brier_score
pipeline_tag: tabular-classification
library_name: scikit-learn
---

# 🌊 RedR: HydroGuard-AI Dam Failure Risk Prediction Ensemble

**RedR** is a production-grade Gradient Boosted Decision Tree (GBT) ensemble model developed for real-time dam breach probability estimation, risk assessment, and early warning intelligence across national dam registries (including 6,648 registered Indian dams).

The model combines physics-informed features (Rational Method Peak Inflow Surge, Stefanelli Dam Breach Index, Overtopping Risk Index) with structural metadata and real-time precipitation vectors.

---

## 📊 Model Performance Highlights

- **Ensemble AUC-ROC**: `0.9748`
- **XGBoost AUC-ROC**: `0.9785`
- **LightGBM AUC-ROC**: `0.9786`
- **Brier Score (Probability Calibration)**: `0.0337`
- **Stress-Test Accuracy (Verified Disasters)**: `100.0%` (15/15 scenarios)
- **Inference Latency**: `~9.3 ms` per dam prediction

---

## 🛠️ Model Architecture

The ensemble consists of:
1. **XGBoost Classifier** (GPU-accelerated, Optuna Bayesian hyperparameter tuned)
2. **LightGBM Classifier** (Isotonic probability calibrated)
3. **Isotonic Calibrator** (`CalibratedClassifierCV`) mapping raw GBT logits into true physical breach probabilities $[0, 1]$.

---

## 📁 Repository Contents

- `ensemble_model.pkl`: Full soft-voting ensemble model weights.
- `dam_failure_xgb.pkl`: Trained XGBoost sub-model.
- `dam_failure_lgbm.pkl`: Trained LightGBM sub-model.
- `calibrator.pkl`: Fitted Isotonic Probability Calibrator.
- `training_metrics.json`: Detailed evaluation metrics, ROC scores, and confusion matrix.
- `shap_summary.png`: Global TreeSHAP feature importance visualization.
- `predictor.py`: Python inference class (`DamBreachPredictor`).
- `feature_pipeline.py`: Physics-informed feature computation module (`FeaturePipeline`).

---

## 🚀 Quickstart Inference

```python
import joblib
from feature_pipeline import FeaturePipeline
from predictor import DamBreachPredictor

# Initialize predictor
predictor = DamBreachPredictor(model_dir=".")
pipeline = FeaturePipeline()

# Define dam metadata & precipitation scenario
dam_data = {
    "name": "Sample Earthen Dam",
    "height_m": 30.0,
    "crest_length_m": 450.0,
    "catchment_area_sqkm": 850.0,
    "current_storage_mcm": 75.0,
    "freeboard_m": 0.5,
    "spillway_capacity_cumec": 1200.0,
    "structural_type": "Earthfill",
    "dam_age_years": 40,
    "is_natural": False,
    "crest_displacement_mm_yr": 3.0
}

# Compute 14-dimensional feature vector under 45mm/hr rain scenario
features = pipeline.compute_features(dam_data, rain_1h=45.0, rain_24h=180.0)

# Predict breach probability & alert level
prob = predictor.predict_probability(features)
alert = predictor.classify_alert(prob)

print(f"Breach Probability: {prob*100:.2f}%")
print(f"Alert Level: {alert['level']} {alert['emoji']}")
```

---

## 📜 Citation & License

Developed as part of the **HydroGuard-AI** Dam Safety Intelligence Platform. Released under the MIT License.
