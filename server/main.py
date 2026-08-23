import os
import json
import sqlite3
from typing import Dict, Any, Optional
from contextlib import asynccontextmanager

import aiosqlite
from fastapi import FastAPI, HTTPException, Response, status, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

# Path to the MBTiles database file
MBTILES_PATH = os.getenv("MBTILES_PATH", os.path.join(os.path.dirname(__file__), "..", "data", "map.mbtiles"))

# Database connection holder
db_conn: Optional[aiosqlite.Connection] = None
mbtiles_metadata: Dict[str, Any] = {}


async def load_metadata(db: aiosqlite.Connection) -> Dict[str, Any]:
    """Extract and parse all key-value pairs from the MBTiles metadata table."""
    metadata: Dict[str, Any] = {}
    try:
        async with db.execute("SELECT name, value FROM metadata") as cursor:
            async for row in cursor:
                key, val = row[0], row[1]
                if key in ["bounds", "center"]:
                    try:
                        metadata[key] = [float(x.strip()) for x in val.split(",")]
                    except Exception:
                        metadata[key] = val
                elif key in ["minzoom", "maxzoom"]:
                    try:
                        metadata[key] = int(val)
                    except Exception:
                        metadata[key] = val
                elif key == "json":
                    try:
                        metadata[key] = json.loads(val)
                    except Exception:
                        metadata[key] = val
                else:
                    metadata[key] = val
    except Exception as e:
        print(f"[WARN] Error reading metadata table: {e}")
    return metadata


@asynccontextmanager
async def lifespan(app: FastAPI):
    """FastAPI Lifespan context manager for connection pooling and metadata loading."""
    global db_conn, mbtiles_metadata

    target_path = os.path.abspath(MBTILES_PATH)
    print(f"[*] Initializing MBTiles database connection at: {target_path}")

    if not os.path.exists(target_path):
        print(f"[WARN] Target file '{target_path}' does not exist yet. Server running in standby mode.")
        db_conn = None
    else:
        try:
            # Read-only URI mode for high performance concurrency
            db_uri = f"file:{target_path}?mode=ro"
            db_conn = await aiosqlite.connect(db_uri, uri=True)
            # Optimize SQLite performance for vector tile serving
            await db_conn.execute("PRAGMA query_only = ON")
            await db_conn.execute("PRAGMA synchronous = OFF")
            await db_conn.execute("PRAGMA journal_mode = OFF")
            await db_conn.execute("PRAGMA cache_size = -64000")  # ~64MB memory cache

            mbtiles_metadata = await load_metadata(db_conn)
            print(f"[✓] MBTiles loaded successfully. Zoom range: {mbtiles_metadata.get('minzoom', 0)} - {mbtiles_metadata.get('maxzoom', 14)}")
        except Exception as e:
            print(f"[ERROR] Failed to open MBTiles database: {e}")
            db_conn = None

    yield

    if db_conn:
        await db_conn.close()
        print("[*] Closed MBTiles database connection.")


app = FastAPI(
    title="MapLibre Vector Tile Server",
    description="High-throughput asynchronous vector tile server reading from MBTiles.",
    version="1.0.0",
    lifespan=lifespan
)

# Enable CORS for frontend and desktop clients
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health", summary="Health Check and Metadata Inspection")
async def health_check():
    """Returns the tile server health status and loaded MBTiles metadata."""
    is_ready = db_conn is not None
    return {
        "status": "online" if is_ready else "waiting_for_mbtiles",
        "database_path": os.path.abspath(MBTILES_PATH),
        "file_exists": os.path.exists(MBTILES_PATH),
        "metadata": mbtiles_metadata
    }


@app.get("/tilejson.json", summary="TileJSON 3.0 Metadata")
async def get_tilejson(request: Request):
    """Returns valid TileJSON 3.0 metadata for MapLibre / Mapbox clients."""
    base_url = str(request.base_url).rstrip("/")
    minzoom = mbtiles_metadata.get("minzoom", 0)
    maxzoom = mbtiles_metadata.get("maxzoom", 14)
    bounds = mbtiles_metadata.get("bounds", [-180.0, -85.05112878, 180.0, 85.05112878])
    center = mbtiles_metadata.get("center", [0.0, 0.0, minzoom])

    vector_layers = []
    if "json" in mbtiles_metadata and isinstance(mbtiles_metadata["json"], dict):
        vector_layers = mbtiles_metadata["json"].get("vector_layers", [])

    tilejson_data = {
        "tilejson": "3.0.0",
        "name": mbtiles_metadata.get("name", "Assam Vector Map"),
        "description": mbtiles_metadata.get("description", "OpenStreetMap Vector Tiles"),
        "version": mbtiles_metadata.get("version", "1.0.0"),
        "attribution": mbtiles_metadata.get("attribution", "© OpenStreetMap contributors"),
        "scheme": "xyz",
        "tiles": [
            f"{base_url}/tiles/{{z}}/{{x}}/{{y}}.pbf"
        ],
        "minzoom": minzoom,
        "maxzoom": maxzoom,
        "bounds": bounds,
        "center": center,
        "format": "pbf",
        "vector_layers": vector_layers
    }

    return JSONResponse(
        content=tilejson_data,
        headers={
            "Cache-Control": "public, max-age=3600",
            "Access-Control-Allow-Origin": "*"
        }
    )


@app.get("/tiles/{z}/{x}/{y}.pbf", summary="Vector Tile Fetcher (PBF)")
async def get_tile(z: int, x: int, y: int):
    """
    Extracts raw vector tile (Protocolbuffer) from MBTiles database.
    Converts standard Slippy map XYZ coordinate to TMS coordinate format (y_tms = 2^z - 1 - y).
    """
    if db_conn is None:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="MBTiles database is not loaded or file is missing."
        )

    # Basic range validation
    max_coord = 1 << z
    if z < 0 or x < 0 or x >= max_coord or y < 0 or y >= max_coord:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid tile coordinates.")

    # Convert Slippy map XYZ y to TMS y coordinate used in MBTiles
    tms_y = (1 << z) - 1 - y

    query = "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ? LIMIT 1"
    async with db_conn.execute(query, (z, x, tms_y)) as cursor:
        row = await cursor.fetchone()

    if not row or not row[0]:
        # Return 204 No Content for empty/missing tile
        return Response(status_code=status.HTTP_204_NO_CONTENT)

    tile_bytes: bytes = row[0]

    # Return raw gzip-encoded protobuf tile
    return Response(
        content=tile_bytes,
        media_type="application/x-protobuf",
        headers={
            "Content-Encoding": "gzip",
            "Content-Type": "application/x-protobuf",
            "Cache-Control": "public, max-age=86400",
            "Access-Control-Allow-Origin": "*"
        }
    )


@app.get("/style.json", summary="Dark MapLibre Style Specification")
async def get_dark_style(request: Request):
    """Returns a full dark theme MapLibre GL style specification referencing local vector tiles."""
    base_url = str(request.base_url).rstrip("/")
    center = mbtiles_metadata.get("center", [92.8, 26.2, 8])
    if isinstance(center, list) and len(center) >= 2:
        center_coords = [center[0], center[1]]
        initial_zoom = center[2] if len(center) > 2 else 8
    else:
        center_coords = [92.8, 26.2]
        initial_zoom = 8

    style = {
        "version": 8,
        "name": "Dark Matter Minimalist",
        "center": center_coords,
        "zoom": initial_zoom,
        "sources": {
            "openmaptiles": {
                "type": "vector",
                "url": f"{base_url}/tilejson.json"
            }
        },
        "layers": [
            {
                "id": "background",
                "type": "background",
                "paint": {
                    "background-color": "#18181B"  # shadcn zinc-900 dark background
                }
            },
            {
                "id": "landcover",
                "type": "fill",
                "source": "openmaptiles",
                "source-layer": "landcover",
                "paint": {
                    "fill-color": "#1C241D",
                    "fill-opacity": 0.6
                }
            },
            {
                "id": "water",
                "type": "fill",
                "source": "openmaptiles",
                "source-layer": "water",
                "paint": {
                    "fill-color": "#142533"  # Deep dark water
                }
            },
            {
                "id": "waterway",
                "type": "line",
                "source": "openmaptiles",
                "source-layer": "waterway",
                "paint": {
                    "line-color": "#19364C",
                    "line-width": 1.5
                }
            },
            {
                "id": "park",
                "type": "fill",
                "source": "openmaptiles",
                "source-layer": "park",
                "paint": {
                    "fill-color": "#1B2F21",
                    "fill-opacity": 0.8
                }
            },
            {
                "id": "building",
                "type": "fill",
                "source": "openmaptiles",
                "source-layer": "building",
                "minzoom": 13,
                "paint": {
                    "fill-color": "#27272A",  # shadcn zinc-800
                    "fill-outline-color": "#3F3F46"
                }
            },
            {
                "id": "road_minor",
                "type": "line",
                "source": "openmaptiles",
                "source-layer": "transportation",
                "filter": ["all", ["!=", "class", "motorway"], ["!=", "class", "primary"]],
                "paint": {
                    "line-color": "#3F3F46",  # shadcn zinc-700
                    "line-width": ["interpolate", ["linear"], ["zoom"], 10, 0.8, 16, 3.0]
                }
            },
            {
                "id": "road_major",
                "type": "line",
                "source": "openmaptiles",
                "source-layer": "transportation",
                "filter": ["all", ["in", "class", "motorway", "trunk", "primary"]],
                "paint": {
                    "line-color": "#FB923C",  # Vibrant Orange Primary / Highway
                    "line-width": ["interpolate", ["linear"], ["zoom"], 6, 1.2, 16, 5.0]
                }
            },
            {
                "id": "boundary_district",
                "type": "line",
                "source": "openmaptiles",
                "source-layer": "boundary",
                "paint": {
                    "line-color": "#52525B",
                    "line-dasharray": [3, 2],
                    "line-width": 1.2
                }
            },
            {
                "id": "place_label_city",
                "type": "symbol",
                "source": "openmaptiles",
                "source-layer": "place",
                "filter": ["==", "class", "city"],
                "layout": {
                    "text-field": "{name}",
                    "text-size": 13,
                    "text-font": ["Noto Sans Regular", "Open Sans Regular"]
                },
                "paint": {
                    "text-color": "#FAFAFA",  # Crisp White Text
                    "text-halo-color": "#09090B",
                    "text-halo-width": 2
                }
            },
            {
                "id": "place_label_town",
                "type": "symbol",
                "source": "openmaptiles",
                "source-layer": "place",
                "filter": ["==", "class", "town"],
                "minzoom": 8,
                "layout": {
                    "text-field": "{name}",
                    "text-size": 11,
                    "text-font": ["Noto Sans Regular", "Open Sans Regular"]
                },
                "paint": {
                    "text-color": "#D4D4D8",
                    "text-halo-color": "#09090B",
                    "text-halo-width": 1.5
                }
            }
        ]
    }

    return JSONResponse(
        content=style,
        headers={"Cache-Control": "public, max-age=3600", "Access-Control-Allow-Origin": "*"}
    )


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
