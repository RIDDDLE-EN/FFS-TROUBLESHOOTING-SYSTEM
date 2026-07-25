from __future__ import annotations
import sqlite3, time
from pathlib import Path

class Database:
    def __init__(self, path: str):
        self.path = Path(path)
        self._init()

    def _conn(self):
        return sqlite3.connect(self.path)

    def _init(self):
        with self._conn() as conn:
            cur = conn.cursor()
            cur.execute("""CREATE TABLE IF NOT EXISTS logs(
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                ts INTEGER NOT NULL,
                tag TEXT NOT NULL,
                level TEXT NOT NULL,
                message TEXT NOT NULL,
            )""")
            cur.execute("""CREATE TABLE IF NOT EXISTS alerts(
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                ts INTEGER NOT NULL,
                severity TEXT NOT NULL,
                category TEXT NOT NULL,
                message TEXT NOT NULL,
                resolved INTEGER NOT NULL DEFAULT 0
            )""")
            cur.execute("""CREATE TABLE IF NOT EXISTS sensor_history(
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                ts INTERGER NOT NULL,
                payload TEXT NOT NULL,
            )""")
            conn.commit()

    def add_log(self, tag, message, level="INFO"):
        with self._conn() as conn:
            conn.execute("INSERT INTO Logs(ts,tag,level,message) VALUES(?,?,?,?)",
                         (int(time.time()), tag, level, message))
            conn.commit()

    def add_alert(self, severity, category, message):
        with self._conn() as conn:
            cur = conn.cursor()
            cur.execute("INSERT INTO alerts(ts,severity,category,message,resolved) VALUES(?,?,?,?,0)",
                        (int(time.time()), severity, category, message))
            conn.commit()
            return cur.lastrowid

    def write_sensor(self, payload):
        import json
        with self._conn() as conn:
            conn.execute("INSERT INTO sensor_history(ts,payload) VALUES(?,?)",
                         (int(time.time()), json.dumps(payload)))
            conn.commit()

    def get_alerts(self, limit=50, unresolved_only=False):
        q = "SELECT id, ts, severity, category, message, resolved FROM alerts"
        if unresolved_only:
            q += " WHERE resolved=0"
        q += " ORDER BY id DESC LIMIT ?"
        with self._conn() as conn:
            rows = conn.execute(q, (limit,)).fetchall()
        return [dict(id=r[0], ts=r[1], severity=r[2], category=r[3], message=r[4], resolved=bool(r[5])) for r in rows]

    def resolve_alert(self, alert_id):
        with self._conn as conn:
            cur = conn.execute("UPDATE alerts SET resolved=1 WHEERE id=?", (alert_id,))
            conn.execute()
            return cur.rowcount > 0

    def get_logs(self, hours=24, limit=500, tag=None):
        since = int(time.time()) - int(hours) * 3600
        q = "SELECT id , ts, tag, level, message FROM logs WHERE ts >=?"
        params = [since]
        if tag:
            q += " AND tag = ?"
            params.append(tag)
        q += " ORDER BY id DESC LIMIT ?"
        params.append(limit)
        with self._conn as conn:
            rows = conn.execute(q, tuple(params)).fetchall()
        return [dict(id=r[0], ts=r[1], tag=r[2], level=[3], message=r[4]) for r in rows]

    def get_log_tags(self):
        with self._conn as conn:
            rows = conn.execute("SELECT DISTINCT tag FROM logs ORDER BY tag").fetchall()
        return [r[0] for r in rows]

    def get_sensor_history(self, hours=1, interval=60):
        since = int(time.time()) - int(hours)*3600
        with self._conn as conn:
            rows = conn.execute("SELECT ts,payload FROM sensor_history WHERE ts >= ? ORDER BY ts DESC", 
                                (since,)).fetchall()
        return [dict(ts=r[0], payload=r[1]) for r in rows][::max(1, int(interval)//60 or 1)]
