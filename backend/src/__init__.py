# HydroGuard-AI: Core Engine Package
"""
Automated Real-Time Meteorological Surveillance & Dam Failure Probability Prediction System.

Modules:
    weather_surveillance  - Open-Meteo API polling & cloudburst detection
    spatial_engine        - GeoPandas catchment intersection & dam lookup
    feature_pipeline      - Telemetry ETL, ORI/DBI/inflow computation
    predictor             - ML inference engine + SHAP explainer
    export_tools          - KML/SHP export for NDRF/HADR dispatch
    data_generator        - Physics-informed synthetic training data generator
"""

__version__ = "1.0.0"
__author__ = "HydroGuard-AI Team"

from src.predictor import DamBreachPredictor, ALERT_LEVELS, FEATURE_COLS
from src.feature_pipeline import FeaturePipeline

__all__ = [
    "DamBreachPredictor",
    "FeaturePipeline",
    "ALERT_LEVELS",
    "FEATURE_COLS",
]
