from __future__ import annotations

import os
from typing import Any

import requests
from flask import Flask, jsonify, render_template, request
from flask_cors import CORS

app = Flask(__name__, template_folder="templates", static_folder="static")
app.config["JSON_SORT_KEYS"] = False
app.config["TEMPLATES_AUTO_RELOAD"] = True
CORS(app)

PI_BACKEND_URL = os.getenv("PI_BACKEND_URL", "http://127.0.0.1:5001").rstrip("/")
REQUEST_TIMEOUT = float(os.getenv("REQUEST_TIMEOUT", "8"))

CACHE: dict[str, Any] = {"connected": False, "latest": None, "alerts": []}


def _full_url(endpoint: str) -> str:
    if not endpoint.startswith("/"):
        endpoint = "/" + endpoint
    return f"{PI_BACKEND_URL}{endpoint}"


def _safe_json(resp: requests.Response):
    try:
        return resp.json()
    except Exception:
        return {"ok": resp.ok, "text": resp.text}


def _get_pi(endpoint: str, params=None):
    try:
        resp = requests.get(_full_url(endpoint), params=params, timeout=REQUEST_TIMEOUT)
        CACHE["connected"] = resp.ok
        return _safe_json(resp), resp.status_code
    except Exception as exc:
        CACHE["connected"] = False
        return {"ok": False, "error": str(exc)}, 503


def _post_pi(endpoint: str, data=None):
    try:
        resp = requests.post(_full_url(endpoint), json=data, timeout=REQUEST_TIMEOUT)
        CACHE["connected"] = resp.ok
        return _safe_json(resp), resp.status_code
    except Exception as exc:
        CACHE["connected"] = False
        return {"ok": False, "error": str(exc)}, 503


def _fallback_latest():
    return {
        "ok": True,
        "ready": False,
        "data": CACHE.get("latest") or {
            "motor1": {"current": 0.0, "rpm": 0.0, "state": {"class": "idle", "name": "Idle"}, "running": False},
            "weight": {"kg": 0.0, "grams": 0.0, "calibrated": False, "stable": False},
            "temperature": {
                "ambient": 0.0,
                "humidity": 0.0,
                "thermocouple": 0.0,
                "tc_connected": False,
                "fan_required": False,
                "fan_state": "auto",
            },
            "roll": {"diff_cm": 0.0, "centered": True, "correction_active": False},
            "vibration": {"rms_g": 0.0, "peak_g": 0.0, "dominant_freq_hz": 0.0},
            "bags_counted": 0,
            "bag_detected": False,
            "bag_length_cm": 0.0,
            "env_status": "No Data",
            "env_sensor_ok": False,
            "feed_mode": "single_motor_stepwise",
            "settings": {},
        },
    }


@app.get("/")
def dashboard():
    return render_template("dashboard.html", active="dashboard")


@app.get("/controls")
def controls():
    return render_template("controls.html", active="controls")


@app.get("/calibration")
def calibration():
    return render_template("calibration.html", active="calibration")


@app.get("/diagnostics")
def diagnostics():
    return render_template("diagnostics.html", active="diagnostics")


@app.get("/logs")
def logs():
    return render_template("logs.html", active="logs")


@app.get("/monitor")
def monitor():
    return render_template("monitor.html", active="monitor")


@app.get("/api/status")
def api_status():
    data, status = _get_pi("/api/status")
    if not isinstance(data, dict) or status != 200:
        return jsonify({
            "ok": True,
            "status": "ok",
            "connected": CACHE["connected"],
            "latest": _fallback_latest()["data"],
        }), 200
    return jsonify(data), 200


@app.get("/api/health")
def api_health():
    data, status = _get_pi("/api/health")
    if status != 200:
        data = {"ok": True, "status": "ok"}
    return jsonify(data), 200


@app.get("/api/sensors/latest")
def api_latest():
    data, status = _get_pi("/api/sensors/latest")
    if isinstance(data, dict) and "data" in data:
        CACHE["latest"] = data["data"]
        return jsonify(data), 200
    return jsonify(_fallback_latest()), 200


@app.get("/api/alerts")
def api_alerts():
    data, status = _get_pi(
        "/api/alerts",
        {
            "limit": request.args.get("limit", 50),
            "unresolved": request.args.get("unresolved", "false"),
        },
    )
    if isinstance(data, list):
        CACHE["alerts"] = data
    return jsonify(data), 200 if status == 404 else status


@app.post("/api/alerts/<int:alert_id>/resolve")
def api_resolve_alert(alert_id: int):
    data, status = _post_pi(f"/api/alerts/{alert_id}/resolve")
    return jsonify(data), status if status != 404 else 200


@app.get("/api/logs/history")
def api_logs_history():
    params = {"hours": request.args.get("hours", 24), "limit": request.args.get("limit", 500)}
    if request.args.get("tag"):
        params["tag"] = request.args["tag"]
    data, status = _get_pi("/api/logs/history", params)
    return jsonify(data), 200 if status == 404 else status


@app.get("/api/logs/tags")
def api_logs_tags():
    data, status = _get_pi("/api/logs/tags")
    return jsonify(data), 200 if status == 404 else status


def _control_post(endpoint: str):
    data, status = _post_pi(endpoint, request.get_json(silent=True) or {})
    return jsonify(data), 200 if status == 404 else status


@app.post("/api/control/ping")
def api_ping():
    return _control_post("/api/control/ping")


@app.post("/api/control/reset_bags")
def api_reset_bags():
    return _control_post("/api/control/reset_bags")


@app.post("/api/control/emergency_stop")
def api_emergency_stop():
    return _control_post("/api/control/emergency_stop")


@app.post("/api/control/motors/feed1/speed")
def api_feed_speed():
    return _control_post("/api/control/motors/feed1/speed")


@app.post("/api/control/feed/pulse")
def api_feed_pulse():
    return _control_post("/api/control/feed/pulse")


@app.post("/api/control/fan")
def api_fan():
    return _control_post("/api/control/fan")


@app.post("/api/control/stepper/center")
def api_center():
    return _control_post("/api/control/stepper/center")


@app.get("/api/control/settings")
def api_settings():
    data, status = _get_pi("/api/control/settings")
    return jsonify(data), 200 if status == 404 else status


@app.post("/api/control/bag_length")
def api_bag_length():
    return _control_post("/api/control/bag_length")


@app.post("/api/control/target_weight")
def api_target_weight():
    return _control_post("/api/control/target_weight")


@app.post("/api/control/seal_temp")
def api_seal_temp():
    return _control_post("/api/control/seal_temp")


@app.post("/api/control/fan_threshold")
def api_fan_threshold():
    return _control_post("/api/control/fan_threshold")


@app.post("/api/control/pid")
def api_pid():
    return _control_post("/api/control/pid")


@app.post("/api/control/thermo_offset")
def api_thermo_offset():
    return _control_post("/api/control/thermo_offset")


@app.post("/api/calibrate/loadcell/start")
def api_lc_start():
    return _control_post("/api/calibrate/loadcell/start")


@app.post("/api/calibrate/loadcell/confirm")
def api_lc_confirm():
    return _control_post("/api/calibrate/loadcell/confirm")


@app.post("/api/calibrate/loadcell/cancel")
def api_lc_cancel():
    return _control_post("/api/calibrate/loadcell/cancel")


@app.get("/api/calibrate/thermocouple/check")
def api_tc_check():
    data, status = _get_pi("/api/calibrate/thermocouple/check")
    return jsonify(data), 200 if status == 404 else status


@app.post("/api/calibrate/thermocouple")
def api_tc_calibrate():
    return _control_post("/api/calibrate/thermocouple")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.getenv("PORT", "8000")), debug=False)
