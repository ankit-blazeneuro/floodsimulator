"""
HydroGuard-AI: Dam Dataset Ingestion Tool

Parses dam_dataset/dam.geojson (6,640+ CWC/India WRIS dams), maps coordinates
from DMS to decimal degrees, extracts physical parameters (height, length, storage,
spillway capacity, completion year, dam type), and updates data/india_dams_seed.json.
"""

import json
import os
import re
from datetime import datetime


def dms_to_dd(dms_str):
    """Convert Degrees Minutes Seconds (DMS) string to Decimal Degrees."""
    if not dms_str:
        return None
    match = re.search(r'(\d+)°\s*(\d+)\'\s*([\d.]+)\"\s*([NSEW])', str(dms_str))
    if not match:
        return None
    deg, m, s, direction = match.groups()
    dd = float(deg) + float(m) / 60.0 + float(s) / 3600.0
    if direction in ['S', 'W']:
        dd = -dd
    return round(dd, 5)


def map_structural_type(dm_type_raw):
    """Map raw CWC dam type text to standardized categories and codes."""
    if not dm_type_raw:
        return "earthen", 0

    text = str(dm_type_raw).lower()
    if "earthen" in text or "earth" in text or "homogeneous" in text:
        return "earthen", 0
    elif "gravity" in text or "concrete" in text or "masonry" in text:
        return "gravity", 1
    elif "rockfill" in text or "cfrd" in text:
        return "rockfill", 2
    elif "barrage" in text or "weir" in text:
        return "barrage", 4
    elif "arch" in text:
        return "arch", 3
    else:
        return "earthen", 0


def ingest_geojson(
    geojson_path: str = "dam_dataset/dam.geojson",
    output_seed_path: str = "data/india_dams_seed.json",
):
    print("=" * 65)
    print("  HydroGuard-AI: Ingesting 6,600+ Dams from GeoJSON Dataset")
    print("=" * 65)

    if not os.path.exists(geojson_path):
        print(f"❌ Error: GeoJSON file not found at {geojson_path}")
        return

    with open(geojson_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    features = data.get("features", [])
    print(f"\n📂 Loaded {len(features)} raw dam features from {geojson_path}")

    # Load existing seed to preserve natural landslide case studies
    existing_dams = []
    if os.path.exists(output_seed_path):
        with open(output_seed_path, "r") as f:
            seed_data = json.load(f)
            existing_dams = seed_data.get("dams", [])

    # Keep special natural dams / case studies (is_natural=True)
    natural_dams = [d for d in existing_dams if d.get("is_natural", False)]
    print(f"📌 Preserving {len(natural_dams)} natural landslide dam case studies (Rishi Ganga, Phuktal, etc.)")

    ingested_dams = []
    seen_ids = set()

    for idx, feat in enumerate(features):
        props = feat.get("properties", {})

        # Coordinates
        lat = dms_to_dd(props.get("latitude"))
        lon = dms_to_dd(props.get("longitude"))

        # Fallback to geometry coordinates if DMS parse failed
        if (lat is None or lon is None) and "geometry" in feat and feat["geometry"]:
            coords = feat["geometry"].get("coordinates", [])
            if len(coords) == 2:
                # Some coordinates might be in projected CRS, check bounds
                x, y = coords[0], coords[1]
                if 5.0 <= y <= 38.0 and 68.0 <= x <= 98.0:
                    lon, lat = round(x, 5), round(y, 5)

        if lat is None or lon is None:
            continue

        # Skip outside India bounding box
        if not (5.0 <= lat <= 38.0 and 68.0 <= lon <= 98.0):
            continue

        pic_id = props.get("PIC") or f"DAM_IND_{idx+1:04d}"
        if pic_id in seen_ids:
            pic_id = f"{pic_id}_{idx}"
        seen_ids.add(pic_id)

        dam_name = (props.get("dm_name") or f"Dam {pic_id}").strip()
        state = (props.get("state") or "India").strip()
        river = (props.get("river") or "Local Stream").strip()

        # Physical parameters
        ht_found = float(props.get("ht_found") or 25.0)
        if ht_found <= 0 or ht_found > 350:
            ht_found = 25.0

        length_m = float(props.get("dm_length") or 0.0)
        if length_m <= 0:
            length_m = round(ht_found * 10.0, 1)

        storage_mcm = float(props.get("gs_st_cap") or 0.0)
        if storage_mcm <= 0:
            storage_mcm = round((ht_found ** 2) * 0.05, 2)

        spillway_cap = float(props.get("ds_sp_cap") or 0.0)
        if spillway_cap <= 0:
            spillway_cap = round(storage_mcm * 18.0 + ht_found * 15.0, 1)

        # Catchment area estimate
        catchment_sqkm = round(max(storage_mcm * 8.5 + ht_found * 3.5, 12.0), 1)

        # Freeboard estimate
        mx_level = float(props.get("mx_wt_lel") or 0.0)
        frl_level = float(props.get("frl") or 0.0)
        if mx_level > frl_level > 0:
            freeboard = round(mx_level - frl_level, 2)
        else:
            freeboard = round(max(ht_found * 0.12, 2.5), 2)

        # Completion year / age
        cmp_year = props.get("cmp_year")
        current_year = datetime.now().year
        try:
            cmp_year = int(cmp_year)
            if 1500 <= cmp_year <= current_year:
                dam_age = current_year - cmp_year
            else:
                dam_age = 35
        except (ValueError, TypeError):
            dam_age = 35

        # Dam type
        struct_type, struct_code = map_structural_type(props.get("dm_type"))

        dam_entry = {
            "id": pic_id,
            "name": dam_name,
            "state": state,
            "river": river,
            "lat": lat,
            "lon": lon,
            "dam_height_m": ht_found,
            "crest_length_m": length_m,
            "catchment_area_sqkm": catchment_sqkm,
            "current_storage_mcm": storage_mcm,
            "freeboard_remaining_m": freeboard,
            "spillway_capacity_cumec": spillway_cap,
            "crest_displacement_mm_yr": 1.2,
            "structural_type": struct_type,
            "structural_type_code": struct_code,
            "is_natural": False,
            "dam_age_years": dam_age,
            "purpose": props.get("purpose") or "Irrigation / Hydro",
        }

        ingested_dams.append(dam_entry)

    # Combine natural case studies + ingested dams
    final_dams = natural_dams + ingested_dams

    print(f"✅ Processed {len(ingested_dams)} dams from GeoJSON")
    print(f"📊 Total dams in registry: {len(final_dams)}")

    output_data = {
        "metadata": {
            "source": "India WRIS / CWC National Dam Inventory + HydroGuard Case Studies",
            "total_dams": len(final_dams),
            "updated_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        },
        "dams": final_dams
    }

    os.makedirs(os.path.dirname(output_seed_path), exist_ok=True)
    with open(output_seed_path, "w", encoding="utf-8") as f:
        json.dump(output_data, f, indent=2)

    print(f"🎉 Updated {output_seed_path} successfully!")


if __name__ == "__main__":
    ingest_geojson()
