from __future__ import annotations

import logging, threading, time
from datetime import datetime

from flask import Flask, jsonify, request
from flask_cors import CORS
from flask_socketio import SocketIO, emit

import config as cfg
import motors
from alerts import AlertEngine
from database import Database
from interpreter import interpret

log = logging.getLogger("FFTS.Backend")

app = Flask(__name__)
app.config["JSON_SORT_KEYS"] = False
CORS(app)
sio = socketio(app, cors_allowed_origins="*", async_mode="threading")


class _State:
    spi = None
    cycle = None
    db = None
    alerts = None
    latest_raw = None
    latest_ui = {}
    start_time = time.time()

S = _State()


def _emit_log(tag: str, message: str, level: str = "INFO"):
    now = datetime.now().isoformat(timespec="seconds")
    if S.db:
        S.db.add_log(tag, message, level)
    sio.emit("log_message", {"ts": now, "tag": tag.upper(), "level": level.upper(), "message": message})
    log.info("[%s] %s", tag.upper(), message)

def _fire_alert(key: str, severity: str, category: str, message: str):
    now = datetime.now().isoformat(timespec="seconds")
    alert_id = S.db.add_alert(severity, category, message) if S.db else -1
    payload = {
        "id": alert_id,
        "timestamp": now,
        "severity": severity,
        "category": category,
        "message": message,
        "key": key,
    }
    if S.db:
        S.db.add_log("ALERT", f"{severity} {category}: {message}", "WARNING" if severity != "CRITICAL" else "ERROR")
    sio.emit("alert", payload)

def _apply_machine_actions(ui_data: dict):
    temp = float(ui_data.get("temperature", {}).get("ambient", 0.0))
    hum = float(ui_data.get("temperature", {}).get("humidity", 0.0))
    env_ok = bool(ui_data.get("env_sensor_ok", True))
    roll = ui_data.get("roll", {})
    offset_cm = float(roll.get("diff_cm", 0.0))
    centered = bool(roll.get("centered", True))

    fan_state = motors.auto_control_environment(temp, hum, env_ok)
    ui_data.setdefault("temperature", {})["fan_requered"] = bool(fan_state)

    motors.auto_center_roll(offset_cm=offset_cm, centered=centered, ultrasonic_ok=True)

def telemetry_loop():
    tick = 0
    while True:
        try:
            if S.spi is not None:
                raw_obj = None
                if S.spi.data_ready():
                    raw_obj = S.spi.readSensors()
                else: 
                    raw_obj = S.spi.readSensors()

                if raw_obj:
                    S.latest_raaw = raw_obj
                    S.latest__ui = interpret(raw_obj)

                    _apply_machine_actions(S.latest_ui)

                    if S.alerts:
                        S.alerts.evaluate(S.latest_ui)

                    tick += 1
                    if S.db and tick >= cfg.DB_WRITE_EVERY:
                        tick = 0
                        S.db.write_sensor(S.latest_ui)

                    sio.emit("sensor_data", {"ts": raw_obj.timestamp_ms, "data": S.latest_ui})

            time.sleep(1.0 / max(1, cfg.SPI_POLL_HZ))
        except Exception as exc:
            _emit_log("SYSTEM", f"Telemetry loop error: {exc}", "ERROR")
            time.sleep(1.0)

@app.get("/api/health")
def api_health():
    return jsonify({
        "status": "ok",
        "uptime_s": int(time.time() - S.start_time),
        "cycle_running": S.cycle.is_running() if S.cycle else False,
        "fan_ok": motors.fan_enabled(),
        "feed_mode": "single_motor_stepwise",
    })


@app.get("/api/sensors/latest")
def api_sensors_latest():
    if not S.latest_ui:
        return jsonify({"error": "No data yet"}), 503
    return jsonify({"ts": S.latest_raw.timestamp_ms, "data": S.latest_ui})

@app.get("/api/sensors/history")
def api_sensors_history():
    hours = int(request.args.get("hours", 1))
    interval = int(request.args.get("interval", 1))
    return jsonify(S.db.get_sensor_history(hours=hours, interval=interval) if S.db else [])

@app.get("/api/alerts")
def api_alerts():
    limit = int(request.args.get("limit", 50))
    unresolved = request.args.get("unresolved", "false").lower() == "true"
    return jsonify(S.db.get_alerts(limit=limit, unresolved_only=unresolved) if S.db else[])

@app.post("/api/alerts/<int:alert_id>/resolve")
def api_resolve_alert(alert_id: int):
    ok = S.db.resolve_alert(alert_id) if S.db else False
    return jsonify({"ok": ok})

@app.get("/api/logs/history")
def api_logs_history():
    hours = int(request.args.get("hours", 24))
    limit = int(request.args.get("limit", 500))
    tag = request.args.get("tag")
    return jsonify(S.db.get_logs(hours=hours, limit=limit, tag=tag) if S.db else [])

@app.get("/api/logs/tags")
def api_logs_tags():
    return jsonify(S.db.get_log_tags() if S.db else [])

@app.post("/api/control/emergency_stop")
def api_emergency_stop():
    if S.cycle:
        S.cycle.stop()
    motors.retract_all()
    _emit_log("SYSTEM", "Emergency stop activated", "WARNING")
    return jsonify({"ok": True})

@app.post("/api/control/reset_bags")
def api_reset_bags():
    ok = S.spi.reset_bags() if S.spi else False
    _emit_log("SYSTEM", "Bag counter reset" if ok else "Bag reset failed", "INFO" if ok else "ERROR")
    return jsonify({"ok": ok})

@app.post("/api/control/ping")
def api_ping():
    ok = S.spi.ping() if S.spi else False
    return jsonify({"ok": ok})

@app.post("/api/control/motors/feed1/speed")
def api_feed_speed():
    body = request.get_json(silent=True) or {}
    speed = max(0,  min(100, int(body.get("speed", cfg.FEED_DEFAULT_SPEED))))
    motors.set_feed_speed(speed)
    _emit_log("MOTOR", f"Feed speed set to {speed}%", "INFO")
    return jsonify({"ok": True, "motor": "feed1", "speed": speed})

@app.post("/api/control/motors/feed2/speed")
def api_feed_speed():
    body = request.get_json(silent=True) or {}
    speed = max(0,  min(100, int(body.get("speed", cfg.FEED_DEFAULT_SPEED))))
    motors.set_feed_speed(speed)
    return jsonify({"ok": True, "motor": "feed2", "speed": speed, "note": "feed2 removed: mapped to feed1"})

@app.post("/api/control/feed/pulse")
def api_feed_pulse():
    body = request.get_json(silent=True) or []
    pulses = int(body.get("pulses", cfg.FEED_BATCH_PULSES))
    on_s = body.get("on_s")
    off_s = body.get("off_s")
    if S.cycle:
        S.cycle.pulse_feed(pulses=pulses, on_s=on_s, off_s=off_s)
    else: 
        motors.feed_batch(pulses=pulses, on_s=on_s, off_s=off_s)
    _emit_log("MOTOR", f"Feed pulse batch executed({pulses})", "INFO")
    return jsonify({"ok": True, "pulses": pulses})

@app.post("/api/control/fan")
def api_fan():
    body = request.get_json(silent=True) or {}
    state = str(body.get("state", "")).lower()
    if state in {"on", "1", "true"}:
        motors.fan_on()
        return jsonify({"ok": True, "fan_on": True})
    if state in {"off", "0", "false"}:
        motors.fan_off()
        return jsonify({"ok": True, "fan_on": False})
    return jsonify({"ok": False, "error": "state must be on/off"}), 400

@app.post("/api/control/stepper/center")
def api_stepper_center():
    body = request.get_json(silent=True) or []
    offset = float(body.get("offset_cm", 0.0))
    mm = motors.auto_center_roll(offset_cm=offset, centered=False, ultrasonic_ok=True)
    return jsonify({"ok": True, "requested_mm": mm, "position_mm": motors.stepper_get_position_mm()})

@app.post("/api/calibrate/loadcell/start")
def api_lc_start():
    if not S.spi:
        return jsonify({"error": "SPI unavailable"}), 503
    raw = S.spi.calibration_start()
    return jsonify({"status": "taring", "raw": raw}) if raw is not None else (jsonify({"error": "SPI failed"}), 503)

@app.post("/api/calibrate/loadcell/confirm")
def api_lc_confirm():
    body = request.get_json(silent=True) or {}
    factor = float(body.get("factor", 1.0))
    ok = S.spi.calibration_set_factor(factor) if S.spi else False
    _emit_log("CALIBRATION", f"Load cell factor set to {factor}", "INFO" if ok else "ERROR")
    return jsonify({"ok": ok})

@app.post("/api/calibrate/loadcell/cancel")
def api_lc_cancel():
    _emit_log("CALIBRATION", "Load cell calibration cancelled", "INFO")
    return jsonify({"ok": True})

@app.post("/api/control/set_temp")
def api_set_temp():
    body = request.get_json(silent=True) or {}
    temp = body.get("temp")
    if temp is None:
        return jsonify({"ok": False, "error": "temp is required"}), 400
    return jsonify({"ok": True, "temp": float(temp)})

@sio.on("connect")
def on_connect():
    if S.latest_ui:
        emit("sensor_data", {"ts": S.latest_raw.timestamp_ms, "data": S.latest_ui})

def start_backend(spi_instance, cycle_instance, db_instance, alerts_instance):
    S.spi = spi_instance
    S.cycle = cycle_instance
    S.db = db_instance
    S.alerts = alerts_instance

    if S.db:
        _emit_log("SYSTEM", "Backend initialized", "INFO")

    threading.Thread(target=telemetry_loop, daemon=True, name="TelemetryLoop").start()
    log.info("Starting web API on %s:%d", cfg.BACKEND_HOST, cfg.BACKEND_PORT)
    sio.run(app, host=cfg.BACKEND_HOST, port=cfg.BACKEND_PORT, debug=Fasle, use_reloader=False)
