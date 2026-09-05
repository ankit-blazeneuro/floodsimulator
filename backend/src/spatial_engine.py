"""
HydroGuard-AI: Layer 2 — Spatial Catchment Engine

Performs geospatial analysis to identify dams whose upstream catchment
areas intersect with detected rainfall anomaly zones. Uses GeoPandas
for spatial operations and the Rational Method for runoff estimation.
"""

import json
import os
import geopandas as gpd
import pandas as pd
import numpy as np
from shapely.geometry import Point, Polygon, box
from typing import List, Optional
from dataclasses import dataclass


@dataclass
class AtRiskDam:
    """A dam identified as being at risk from a rainfall anomaly."""
    dam_id: str
    name: str
    state: str
    lat: float
    lon: float
    rain_1h_mm: float
    rain_24h_mm: float
    distance_km: float
    dam_data: dict


class SpatialCatchmentEngine:
    """
    Spatial engine for catchment-rainfall intersection analysis.

    Loads dam registry and catchment polygons, then intersects with
    rainfall anomaly bounding boxes to find at-risk dams.
    """

    def __init__(
        self,
        dam_registry_path: str = "data/india_dams_seed.json",
        catchment_path: str = "data/catchments/sample_catchments.geojson",
    ):
        self.dam_registry_path = dam_registry_path
        self.catchment_path = catchment_path
        self._dams_gdf = None
        self._catchments_gdf = None
        self._load_data()

    def _load_data(self):
        """Load dam registry and catchment polygons."""
        # Load dam registry
        with open(self.dam_registry_path, "r") as f:
            data = json.load(f)

        dams = data["dams"]
        df = pd.DataFrame(dams)
        geometry = [Point(row["lon"], row["lat"]) for _, row in df.iterrows()]
        self._dams_gdf = gpd.GeoDataFrame(df, geometry=geometry, crs="EPSG:4326")

        print(f"[SPATIAL] 📍 Loaded {len(self._dams_gdf)} dams from registry")

        # Load catchment polygons
        if os.path.exists(self.catchment_path):
            self._catchments_gdf = gpd.read_file(self.catchment_path)
            print(f"[SPATIAL] 🗺️  Loaded {len(self._catchments_gdf)} catchment polygons")
        else:
            self._catchments_gdf = None
            print("[SPATIAL] ⚠️  No catchment file found, using proximity-based matching")

    def find_at_risk_dams(
        self,
        anomalies: list,
        search_radius_km: float = 100.0,
    ) -> List[AtRiskDam]:
        """
        Find dams at risk from detected rainfall anomalies.

        Uses spatial intersection of anomaly bounding boxes with:
        1. Catchment polygons (if available)
        2. Proximity-based search (fallback)
        """
        at_risk = []
        seen_ids = set()

        for anomaly in anomalies:
            # Create bounding box from anomaly
            bbox = anomaly.bounding_box if hasattr(anomaly, 'bounding_box') else anomaly
            if isinstance(bbox, dict):
                anomaly_poly = box(
                    bbox["min_lon"], bbox["min_lat"],
                    bbox["max_lon"], bbox["max_lat"]
                )
            else:
                # Fallback: create 0.5° bbox around point
                anomaly_poly = box(
                    anomaly.lon - 0.5, anomaly.lat - 0.5,
                    anomaly.lon + 0.5, anomaly.lat + 0.5
                )

            # Method 1: Catchment intersection
            if self._catchments_gdf is not None:
                intersecting = self._catchments_gdf[
                    self._catchments_gdf.geometry.intersects(anomaly_poly)
                ]

                for _, catch_row in intersecting.iterrows():
                    dam_id = catch_row.get("dam_id")
                    if dam_id and dam_id not in seen_ids:
                        dam_row = self._dams_gdf[self._dams_gdf["id"] == dam_id]
                        if not dam_row.empty:
                            dam = dam_row.iloc[0]
                            dist = self._haversine(
                                anomaly.lat, anomaly.lon, dam["lat"], dam["lon"]
                            )
                            at_risk.append(AtRiskDam(
                                dam_id=dam_id,
                                name=dam["name"],
                                state=dam["state"],
                                lat=dam["lat"],
                                lon=dam["lon"],
                                rain_1h_mm=anomaly.max_intensity_mm_hr,
                                rain_24h_mm=anomaly.cumulative_mm,
                                distance_km=round(dist, 1),
                                dam_data=dam.to_dict(),
                            ))
                            seen_ids.add(dam_id)

            # Method 2: Proximity-based search (for dams without explicit catchments)
            for _, dam in self._dams_gdf.iterrows():
                dam_id = dam["id"]
                if dam_id in seen_ids:
                    continue

                dist = self._haversine(
                    anomaly.lat, anomaly.lon, dam["lat"], dam["lon"]
                )

                # Check if dam is within search radius
                if dist <= search_radius_km:
                    at_risk.append(AtRiskDam(
                        dam_id=dam_id,
                        name=dam["name"],
                        state=dam["state"],
                        lat=dam["lat"],
                        lon=dam["lon"],
                        rain_1h_mm=anomaly.max_intensity_mm_hr,
                        rain_24h_mm=anomaly.cumulative_mm,
                        distance_km=round(dist, 1),
                        dam_data=dam.to_dict(),
                    ))
                    seen_ids.add(dam_id)

        # Sort by distance (closest first)
        at_risk.sort(key=lambda x: x.distance_km)

        print(f"[SPATIAL] 🎯 Found {len(at_risk)} dams at risk from "
              f"{len(anomalies)} rainfall anomalies")

        return at_risk

    def get_all_dams(self) -> List[dict]:
        """Return all dams in the registry as a list of dicts."""
        return self._dams_gdf.drop(columns=["geometry"]).to_dict(orient="records")

    def get_dam_by_id(self, dam_id: str) -> Optional[dict]:
        """Fetch a single dam by ID."""
        result = self._dams_gdf[self._dams_gdf["id"] == dam_id]
        if result.empty:
            return None
        return result.iloc[0].drop("geometry").to_dict()

    def estimate_upstream_inflow(
        self,
        catchment_area_sqkm: float,
        rain_intensity_mm_hr: float,
        is_natural: bool = False,
        terrain: str = "mountain",
    ) -> float:
        """
        Estimate peak upstream inflow using the Rational Method.

        Q_in = 0.278 * C * I * A_catchment

        Args:
            catchment_area_sqkm: Upstream drainage area (km²)
            rain_intensity_mm_hr: Rainfall intensity (mm/hr)
            is_natural: Whether the dam is a natural landslide barrier
            terrain: Terrain type for runoff coefficient selection

        Returns:
            Peak inflow rate (m³/s)
        """
        # Runoff coefficient selection
        if is_natural:
            c = 0.75  # Steep, bare rock/debris
        elif terrain == "mountain":
            c = 0.65
        elif terrain == "hills":
            c = 0.55
        else:
            c = 0.45  # Plains

        q_in = 0.278 * c * rain_intensity_mm_hr * catchment_area_sqkm
        return round(q_in, 2)

    @staticmethod
    def _haversine(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
        """Calculate great-circle distance between two points (km)."""
        R = 6371.0  # Earth radius in km
        dlat = np.radians(lat2 - lat1)
        dlon = np.radians(lon2 - lon1)
        a = (
            np.sin(dlat / 2) ** 2
            + np.cos(np.radians(lat1))
            * np.cos(np.radians(lat2))
            * np.sin(dlon / 2) ** 2
        )
        c = 2 * np.arctan2(np.sqrt(a), np.sqrt(1 - a))
        return R * c
