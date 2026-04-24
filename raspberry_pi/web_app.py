"""
web_app.py

Complete web application for FFS Troubleshooting system.
Features:
    - Live sensor dashboard
    - Real-time log streaming (no Serial Monitor needed)
    - Smooth calibration wizards
    - System status monitoring
"""

from flask import Flask, render_template, jsonify, Response, request
import queue
import threading
import time
from datetime import datetime
from spi_reader import ESP32SPIReader
from calibration import LoadCellCalibrator, ThermocoupleCalibrator

app = Flask(__name__)

log_queue = queue.Queue()   # For SSE log streaming
spi_reader = None
lc_calibrator = None
tc_calibrator = None

latest_sensor_data = {}
latest_interprete_data = {}

def log_message(tag: str, message: str):
    """Thread-safe logging that appears in both console and web UI."""
    timestamp = datetime.now().strftime("%H:%M:%S")
    log_line = f"[{timestamp}] [{tag}] {message}"
    print(log_line)
    log_queue.put(log_line)

def interpret_sensor_data(raw: dict) -> dict:
    #motor 1 current (ACS712: 1.65V = 0A, 185mV/A)
    adc1 = raw.get('adc_current1', 0)
    voltage1 = (adc1 / 4095.0) * 3.3
    current1 = ((voltage1 - 1.65) * 1000.0) / 185.0
    current1 = max(0, current1)

    #motor 2 Current
    adc2 = raw.get('adc_current2', 0)
    voltage2 = (adc1 / 4095.0) * 3.3
    current2 = ((voltage2 - 1.65) * 1000.0) / 185.0
    current2 = max(0, current2)

    #RPM 
