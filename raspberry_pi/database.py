from __future__ import annotations

import json
import sqlite3
import threading
import time
from pathlib import Path
from typing import Any

import config as cfg


class Database:
    def __init__(self, db_path: str = cfg.DB_PATH):
        self.db_path = Path(db_path)
        self._lock = threading.Lock()
        self._init_db()

    def _connect(self):
        conn = sqlite3.connect(self.db_path, timeout=30, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self):
        with self._connect() as conn:
            conn.executescript("""
                PRAGMA journal_mode=WAL;

                CREATE TABLE IF NOT EXISTS sensor_logs (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts INTEGER NOT NULL,
                    json TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS alerts (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts INTEGER NOT NULL,
                    severity TEXT NOT NULL,
                    category TEXT NOT NULL,
                    message TEXT NOT NULL,
                    resolved INTEGER NOT NULL DEFAULT 0,
                    resolved_ts INTEGER
                );

                CREATE TABLE IF NOT EXISTS logs (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts INTEGER NOT NULL,
                    tag TEXT NOT NULL,
                    level TEXT NOT NULL,
                    message TEXT NOT NULL
                );
            """)

    def write_sensor(self, payload: dict[str, Any]) -> None:
        ts = int(time.time() * 1000)
        with self._lock, self._connect() as conn:
            conn.execute(
                "INSERT INTO sensor_logs (ts, json) VALUES (?, ?)",
                (ts, json.dumps(payload, separators=(',', ':'), ensure_ascii=False)),
            )
            conn.commit()

    def add_alert(self, severity: str, category: str, message: str) -> int:
        ts = int(time.time() * 1000)
        with self._lock, self._connect() as conn:
            cur = conn.execute(
                "INSERT INTO alerts (ts, severity, category, message, resolved) VALUES (?, ?, ?, ?, 0)",
                (ts, severity.upper(), category, message),
            )
            conn.commit()
            return int(cur.lastrowid)

    def get_alerts(self, limit: int = 50, unresolved_only: bool = False):
        sql = "SELECT * FROM alerts"
        params: list[Any] = []
        if unresolved_only:
            sql += " WHERE resolved = 0"
        sql += " ORDER BY ts DESC LIMIT ?"
        params.append(limit)
        with self._connect() as conn:
            rows = conn.execute(sql, params).fetchall()
        return [dict(r) for r in rows]

    def resolve_alert(self, alert_id: int) -> bool:
        ts = int(time.time() * 1000)
        with self._lock, self._connect() as conn:
            cur = conn.execute(
                "UPDATE alerts SET resolved = 1, resolved_ts = ? WHERE id = ?",
                (ts, alert_id),
            )
            conn.commit()
            return cur.rowcount > 0

    def add_log(self, tag: str, message: str, level: str = "INFO") -> None:
        ts = int(time.time() * 1000)
        with self._lock, self._connect() as conn:
            conn.execute(
                "INSERT INTO logs (ts, tag, level, message) VALUES (?, ?, ?, ?)",
                (ts, tag.upper(), level.upper(), message),
            )
            conn.commit()

    def get_logs(self, hours: int = 24, limit: int = 500, tag: str | None = None):
        since = int((time.time() - (hours * 3600)) * 1000)
        sql = "SELECT * FROM logs WHERE ts >= ?"
        params: list[Any] = [since]
        if tag:
            sql += " AND tag = ?"
            params.append(tag.upper())
        sql += " ORDER BY ts DESC LIMIT ?"
        params.append(limit)
        with self._connect() as conn:
            rows = conn.execute(sql, params).fetchall()
        return [dict(r) for r in rows]

    def get_log_tags(self):
        with self._connect() as conn:
            rows = conn.execute("SELECT DISTINCT tag FROM logs ORDER BY tag ASC").fetchall()
        return [r[0] for r in rows]

    def get_sensor_history(self, hours: int = 1, interval: int = 60):
        since = int((time.time() - hours * 3600) * 1000)
        with self._connect() as conn:
            rows = conn.execute(
                "SELECT ts, json FROM sensor_logs WHERE ts >= ? ORDER BY ts ASC",
                (since,),
            ).fetchall()

        history = []
        for row in rows:
            try:
                data = json.loads(row["json"])
            except Exception:
                data = {}
            history.append({"ts": row["ts"], "data": data})
        if interval > 1 and len(history) > interval:
            step = max(1, len(history) // interval)
            history = history[::step]
        return history

    def purge_old(self, days: int = cfg.DB_PURGE_DAYS):
        cutoff = int((time.time() - days * 86400) * 1000)
        with self._lock, self._connect() as conn:
            conn.execute("DELETE FROM sensor_logs WHERE ts < ?", (cutoff,))
            conn.execute("DELETE FROM logs WHERE ts < ?", (cutoff,))
            conn.execute("DELETE FROM alerts WHERE resolved = 1 AND resolved_ts IS NOT NULL AND resolved_ts < ?", (cutoff,))
            conn.commit()
