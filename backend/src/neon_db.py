"""
HydroGuard-AI: Neon Serverless PostgreSQL Database Connector

Persists nationwide meteorological surveillance and dam failure predictions into Neon Serverless Postgres:
1. `surveillance_runs`: Audit trail of every scan cycle (interval, timestamp, counts, danger flag).
2. `dam_live_status`: High-speed live status upsert table for instant external queries & distribution.
3. `dam_telemetry_records`: Time-series analytical history of failure risk & precipitation.

Designed for zero-downtime resilient operation:
- Gracefully degrades to local caching if `NEON_DATABASE_URL` is not set or DB is offline.
- Uses batch upserts and thread-safe operations.
"""

import os
import json
import logging
from datetime import datetime, timezone
from typing import List, Dict, Any, Optional

logger = logging.getLogger("hydroguard_neon")

# Load environment variables from .env if present
def _load_env_file():
    backend_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    root_dir = os.path.dirname(backend_dir)
    env_paths = [os.path.join(backend_dir, ".env"), os.path.join(root_dir, ".env")]
    for p in env_paths:
        if os.path.exists(p):
            try:
                with open(p, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith("#") and "=" in line:
                            k, v = line.split("=", 1)
                            k = k.strip()
                            v = v.strip().strip("'\"")
                            if k not in os.environ:
                                os.environ[k] = v
            except Exception as e:
                logger.warning(f"Error reading env file {p}: {e}")

_load_env_file()


def get_neon_database_url() -> Optional[str]:
    """Retrieves Neon Postgres connection URL from environment."""
    return os.getenv("NEON_DATABASE_URL") or os.getenv("DATABASE_URL")


class NeonDatabaseManager:
    """Manages connection, schema initialization, and batch sync to Neon Serverless Postgres."""

    def __init__(self):
        self._schema_initialized = False
        self.last_synced_at: Optional[str] = None
        self.total_synced_runs: int = 0
        self.last_synced_records: int = 0
        self.last_error: Optional[str] = None

    def is_configured(self) -> bool:
        url = get_neon_database_url()
        return bool(url and url.startswith("postgres"))

    def _get_connection(self):
        import psycopg2
        url = get_neon_database_url()
        if not url:
            raise ValueError("NEON_DATABASE_URL environment variable is not configured.")
        return psycopg2.connect(url)

    def init_schema(self) -> bool:
        """Initializes tables and indexes in Neon Postgres if not existing."""
        if not self.is_configured():
            return False

        if self._schema_initialized:
            return True

        import psycopg2
        try:
            with self._get_connection() as conn:
                with conn.cursor() as cur:
                    # 1. surveillance_runs table
                    cur.execute("""
                        CREATE TABLE IF NOT EXISTS surveillance_runs (
                            id SERIAL PRIMARY KEY,
                            run_timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                            total_dams INT NOT NULL,
                            imminent_count INT NOT NULL DEFAULT 0,
                            warning_count INT NOT NULL DEFAULT 0,
                            watch_count INT NOT NULL DEFAULT 0,
                            normal_count INT NOT NULL DEFAULT 0,
                            is_danger_active BOOLEAN NOT NULL DEFAULT FALSE,
                            cycle_interval_seconds INT NOT NULL,
                            scan_duration_seconds REAL,
                            weather_provider VARCHAR(100),
                            compute_engine VARCHAR(100)
                        );
                    """)

                    # 2. dam_live_status table (latest state per dam for instant external distribution)
                    cur.execute("""
                        CREATE TABLE IF NOT EXISTS dam_live_status (
                            pic VARCHAR(64) PRIMARY KEY,
                            dam_name VARCHAR(255) NOT NULL,
                            state VARCHAR(100) NOT NULL,
                            lat DOUBLE PRECISION NOT NULL,
                            lon DOUBLE PRECISION NOT NULL,
                            rain_1h_mm REAL NOT NULL DEFAULT 0.0,
                            rain_24h_mm REAL NOT NULL DEFAULT 0.0,
                            failure_probability REAL NOT NULL DEFAULT 0.0,
                            alert_level VARCHAR(20) NOT NULL DEFAULT 'NORMAL',
                            alert_color VARCHAR(20) NOT NULL DEFAULT '#22C55E',
                            trigger_reason TEXT,
                            explanation JSONB,
                            source VARCHAR(64),
                            updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                        );
                        CREATE INDEX IF NOT EXISTS idx_dam_live_alert ON dam_live_status (alert_level);
                        CREATE INDEX IF NOT EXISTS idx_dam_live_state ON dam_live_status (state);
                        CREATE INDEX IF NOT EXISTS idx_dam_live_prob ON dam_live_status (failure_probability DESC);
                    """)

                    # 3. dam_telemetry_records table (historical time-series data)
                    cur.execute("""
                        CREATE TABLE IF NOT EXISTS dam_telemetry_records (
                            id BIGSERIAL PRIMARY KEY,
                            run_id INT REFERENCES surveillance_runs(id) ON DELETE CASCADE,
                            pic VARCHAR(64) NOT NULL,
                            dam_name VARCHAR(255) NOT NULL,
                            state VARCHAR(100) NOT NULL,
                            lat DOUBLE PRECISION NOT NULL,
                            lon DOUBLE PRECISION NOT NULL,
                            rain_1h_mm REAL NOT NULL DEFAULT 0.0,
                            rain_24h_mm REAL NOT NULL DEFAULT 0.0,
                            failure_probability REAL NOT NULL DEFAULT 0.0,
                            alert_level VARCHAR(20) NOT NULL DEFAULT 'NORMAL',
                            trigger_reason TEXT,
                            recorded_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                        );
                        CREATE INDEX IF NOT EXISTS idx_telemetry_pic_recorded ON dam_telemetry_records (pic, recorded_at DESC);
                        CREATE INDEX IF NOT EXISTS idx_telemetry_alert ON dam_telemetry_records (alert_level);
                        CREATE INDEX IF NOT EXISTS idx_telemetry_run_id ON dam_telemetry_records (run_id);
                    """)
                conn.commit()
            self._schema_initialized = True
            logger.info("✅ Neon PostgreSQL schema verified and initialized.")
            return True
        except Exception as e:
            self.last_error = str(e)
            logger.error(f"Failed to initialize Neon schema: {e}")
            return False

    def sync_surveillance_results(
        self,
        surveillance_results: List[Dict[str, Any]],
        counts: Dict[str, int],
        is_danger_active: bool,
        cycle_interval_seconds: int,
        scan_duration_seconds: float = 0.0,
        weather_provider: str = "Open-Meteo",
        compute_engine: str = "Modal Serverless GPU"
    ) -> bool:
        """
        Persists a surveillance run and upserts dam telemetry into Neon Postgres.
        Non-blocking safe: fails gracefully without raising exceptions to caller.
        """
        if not self.is_configured():
            logger.debug("Neon Database URL not configured. Skipping Neon sync (data safely saved to local cache).")
            return False

        if not self._schema_initialized:
            if not self.init_schema():
                return False

        import psycopg2
        import psycopg2.extras

        try:
            with self._get_connection() as conn:
                with conn.cursor() as cur:
                    # 1. Insert surveillance run
                    cur.execute("""
                        INSERT INTO surveillance_runs (
                            total_dams,
                            imminent_count,
                            warning_count,
                            watch_count,
                            normal_count,
                            is_danger_active,
                            cycle_interval_seconds,
                            scan_duration_seconds,
                            weather_provider,
                            compute_engine
                        ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                        RETURNING id;
                    """, (
                        len(surveillance_results),
                        counts.get("IMMINENT", 0),
                        counts.get("WARNING", 0),
                        counts.get("WATCH", 0),
                        counts.get("NORMAL", 0),
                        is_danger_active,
                        cycle_interval_seconds,
                        scan_duration_seconds,
                        weather_provider,
                        compute_engine
                    ))
                    run_id = cur.fetchone()[0]

                    # 2. Upsert into dam_live_status for rapid distribution
                    if surveillance_results:
                        now_utc = datetime.now(timezone.utc)
                        live_rows = []
                        telemetry_rows = []

                        for dam in surveillance_results:
                            pic = str(dam.get("pic") or dam.get("id") or dam.get("dam_name"))
                            name = str(dam.get("dam_name", "Unknown Dam"))
                            state = str(dam.get("state", "India"))
                            lat = float(dam.get("lat", 20.0))
                            lon = float(dam.get("lon", 78.0))
                            rain_1h = float(dam.get("rain_1h_mm", 0.0))
                            rain_24h = float(dam.get("rain_24h_mm", 0.0))
                            prob = float(dam.get("failure_probability", 0.0))
                            alert_level = str(dam.get("alert_level", "NORMAL"))
                            alert_color = str(dam.get("alert_color", "#22C55E"))
                            trigger_reason = str(dam.get("trigger_reason", ""))
                            explanation = json.dumps(dam.get("explanation", {}))
                            source = str(dam.get("source", "modal_gpu"))

                            live_rows.append((
                                pic, name, state, lat, lon, rain_1h, rain_24h,
                                prob, alert_level, alert_color, trigger_reason,
                                explanation, source, now_utc
                            ))

                            telemetry_rows.append((
                                run_id, pic, name, state, lat, lon, rain_1h, rain_24h,
                                prob, alert_level, trigger_reason, now_utc
                            ))

                        # Batch upsert live status
                        psycopg2.extras.execute_values(
                            cur,
                            """
                            INSERT INTO dam_live_status (
                                pic, dam_name, state, lat, lon, rain_1h_mm, rain_24h_mm,
                                failure_probability, alert_level, alert_color, trigger_reason,
                                explanation, source, updated_at
                            ) VALUES %s
                            ON CONFLICT (pic) DO UPDATE SET
                                dam_name = EXCLUDED.dam_name,
                                state = EXCLUDED.state,
                                lat = EXCLUDED.lat,
                                lon = EXCLUDED.lon,
                                rain_1h_mm = EXCLUDED.rain_1h_mm,
                                rain_24h_mm = EXCLUDED.rain_24h_mm,
                                failure_probability = EXCLUDED.failure_probability,
                                alert_level = EXCLUDED.alert_level,
                                alert_color = EXCLUDED.alert_color,
                                trigger_reason = EXCLUDED.trigger_reason,
                                explanation = EXCLUDED.explanation,
                                source = EXCLUDED.source,
                                updated_at = EXCLUDED.updated_at;
                            """,
                            live_rows,
                            page_size=200
                        )

                        # Batch insert telemetry history
                        psycopg2.extras.execute_values(
                            cur,
                            """
                            INSERT INTO dam_telemetry_records (
                                run_id, pic, dam_name, state, lat, lon, rain_1h_mm, rain_24h_mm,
                                failure_probability, alert_level, trigger_reason, recorded_at
                            ) VALUES %s;
                            """,
                            telemetry_rows,
                            page_size=200
                        )

                conn.commit()

            self.last_synced_at = datetime.now(timezone.utc).isoformat()
            self.total_synced_runs += 1
            self.last_synced_records = len(surveillance_results)
            self.last_error = None
            logger.info("⚡ Successfully synced run #%d (%d dams) to Neon Serverless Postgres.", run_id, len(surveillance_results))
            return True
        except Exception as e:
            self.last_error = str(e)
            logger.error(f"Error syncing telemetry to Neon Postgres: {e}")
            return False

    def get_status(self) -> Dict[str, Any]:
        """Returns Neon database health and telemetry sync status."""
        configured = self.is_configured()
        url = get_neon_database_url() if configured else None
        masked_host = None
        if url:
            try:
                part = url.split("@")[-1]
                masked_host = part.split("/")[0]
            except Exception:
                masked_host = "neon.tech"

        return {
            "configured": configured,
            "status": "CONNECTED" if (configured and not self.last_error) else ("ERROR" if self.last_error else "NOT_CONFIGURED"),
            "host": masked_host,
            "last_synced_at": self.last_synced_at,
            "total_synced_runs": self.total_synced_runs,
            "last_synced_records": self.last_synced_records,
            "error": self.last_error,
            "distribution_ready": configured and self.total_synced_runs > 0
        }


# Global singleton instance
neon_db = NeonDatabaseManager()
