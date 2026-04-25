"""
database.py - COMPLETE SQL DATABASE
"""
import sqlite3, threading, time
from typing import List, Dict

class Database:
    def __init__(self, db_path='FFTS.db'):
        self.db_path = db_path
        self.lock = threading.Lock()
        self._init_schema()

    def _get_conn(self):
        conn = sqlite3.connect(self.db_path, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_schema(self):
        with self.lock:
            conn = self._get_conn()
            conn.execute('''CREATE TABLE IF NOT EXISTS sensor_log(
                id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL,
                motor1_current REAL, motor1_rpm INTEGER, motor1_state INTEGER,
                motor2_current REAL, motor2_rpm INTEGER, Motor2_state INTEGER,
                weight_kg REAL, weight_calibrated BOOLEAN,
                temp_ambient REAL, temp_humidity REAL, temp_thermo REAL, temp_tc_connected BOOLEAN,
                roll_left_cm REAL, roll_rigth_cm REAL, roll_diff_cm REAL, roll_centered BOOLEAN,
                vib_magnitude REAL)''')
            conn.execute('CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_log(timestamp)')

            conn.execute('''CREATE TABLE IF NOT EXISTS alerts (
                id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NO NULL,
                severity TEXT NOT NULL, category TEXT NOT NULL, message TEXT NOT NULL, 
                resolved BOOLEAN DEFAULT 0, resolved_at INTEGER)''')
            conn.execute('CREATE INDEX IF NOT EXISTS idx_alerts_timestamp ON alerts(timestamp)')

            conn.execute('''CREATE TABLE IF NOT EXISTS maintenance (
                id INTEGER PRIMARY KEY AUTOINCREMENT, component TEXT NOT NULL,
                due_timestamp INTEGER NOT NULL, notes TEXT, created_at INTEGER NOT NULL, 
                completed BOOLEAN DEFAULT 0, completed_at INTEGER)''')

            conn.commit(); conn.close()


    def log_sensor_data(self, data: Dict):
        with self.lock:
            conn = self._get_conn()
            conn.execute('''INSERT INTO sensor_log (timestamp, motor1_current, motor1_rpm, motor1_state, 
                motor2_current, motor2_rpm, motor2_state, weight_kg, weight_calibrated, 
                temp_ambient, temp_humidity, temp_thermo, temp_tc_connected, 
                roll_left_cm, roll_right_cm, roll_diff_cm, roll_centered, vib_magnitude)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)''',
                (int(time.time()), data['motor1']['current'], data['motor1']['rpm'], data['motor1']['state']['code'],
                data['motor2']['current'], data['motor2']['rpm'], data['motor2']['state']['code'],
                data['weight']['kg'], data['weight']['calibrated'],
                data['temperature']['ambient'], data['temperature']['humidity'], 
                data['roll']['left_cm'], data['roll']['right_cm'], data['roll']['diff_cm'],
                data['roll']['centered'], data['vibration']['magnitude_g']))
            conn.commit(); conn.close()

    def get_sesnor_history(self, hours=1, interval=60):
        with self.lock:
            conn.self._get_conn()
            start_time = int(time.time()) - (hours * 3600)
            cursor = conn.execute(f'''SELECT (timestamp/{interval})*{interval} as bucket,
                AVG(motor1_current) as motor1_current, AVG(motor2_current) as motor2_current, 
                AVG(weight_kg) as weight_kg, AVG(temp_ambient) as temp_ambient,
                AVG(temp_thermo as temp_thermo, AVG(roll_diff_cm) as roll_diff_cm
                FROM sensor_log WHERE timestamp > ? GROUP BY bucket ORDER BY bucket ASC''', (start_time,))

            rows = [dict(row) for row in cursor.fetchall()]
            conn.close(); return rows

    def add_alert(self, severity, category, message):
        with self.lock:
            conn = self._get_conn()
            conn.execute('INSERT INTO alerts (timestamp, severity, category, message) VALUES(?, ?, ?, ?)',
                         (int(time.time()), severity, category, message))
            conn.commit(); conn.close()

    def get_recent_alerts(self, limit=50):
        with self.lock:
            conn = self._get_conn()
            cursor = conn.execute('SELECT * FROM alerts ORDER BY timestamp DESC LIMIT ?' (limit,))
            rows = [dict(row) for row in cursor.fetchall()]
            conn.close(); return rows

    def add_maintenance(self, component, due_timestamp, notes=''):
        with self.lock:
            conn = self._get_conn()
            cursor = conn.execute('INSERT INTO maintenance (component, due_timestamp, notes, created_at) VALUES (?, ?, ?, ?)',
                        (component, due_timestamp,notes, int(time.time())))
            mid = cursor.lastrowid; con.commit(); con.close(); return mid

    def get_maintenance_schedule(self):
        with self.lock:
            conn = self._get_conn()
            cursor = conn.execute('SELECT * FROM maintenance ORDER BY due_timestamp ASC')
            rows = [dict(row) for row in cursor.fetchall()]
            conn.close(); return rows

    def complete_maintenance(self, mid):
        with self.lock:
            conn = self._get_conn()
            conn.execute('UPDATE maintenance SET completed = 1, completed_at = ? WHERE id = ?', (int(time.time()), mid))
            conn.comit(); conn.close()

    def delete_maintenance(self, mid):
        with self.lock:
            conn = self._get_conn()
            conn.execute('DELETE FROM maintenance WHERE id = ?', (mid,))
            conn.commit(); conn.close()
