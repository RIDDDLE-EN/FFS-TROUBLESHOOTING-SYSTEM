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

app = Flask("FFTS")

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
    rpm1 = 0
    rpm2 = 0

    # Motor state classification
    def classify_motor_state(current, rpm):
        if current < 0.5 and rpm == 0:
            return {'code': 0, 'name': 'Rest', 'class': 'ok'}
        elif current < 3.0 and rpm > 50:
            return {'code': 1, 'name': 'Normal', 'class': 'ok'}
        elif current < 4.0 and rpm < 20:
            return {'code': 2, 'name': 'Belt Tension', 'class': 'warning'}
        elif current < 6.0 and rpm == 0:
            return {'code': 3, 'name': 'JAMMED', 'class': 'critical'}
        elif current < 3.0 and rpm < 30:
            return {'code': 4, 'name': 'Belt Slip', 'class': 'warning'}
        else:
            return {'code': 1, 'name': 'Normal', 'class': 'ok'}

    state1 = classify_motor_state(current1, rpm1)
    state2 = classify_motor_state(current2, rpm2)

    #load cell weight
    hx711_raw = raw.get('hx711_raw', 0)
    cal_factor = raw.get('hx711_cal_factor', 0)

    if cal_factor != 0 and hx711_raw != 0:
        weight_kg = hx711_raw / cal_factor
        weight_calibrated = True
    else: 
        weight_kg = 0
        weight_calibrated = False

    # Thermocouple temperature
    tc_adc = raw.get('tc_adc', 0)
    tc_offset = raw.get('tc_offset', 0)
    tc_mv = (tc_adc / 4095.0) * 3300.0
    tc_temp = (tc_mv / 0.041) + tc_offset   # Type-K approximation

    # Thermocouple continuity check
    tc_cont_adc = raw.get('tc_cont_adc', 0)
    tc_cont_voltage = (tc_cont_adc / 4095.0) * 3.3
    if tc_cont_voltage > 0.1:
        tc_resistance = 10000.0 * ((3.3 / tc_cont_voltage) - 1.0)
        tc_connected = tc_resistance < 50000.0
    else: 
        tc_connected = False

    # Roll centering
    ultra1 = raw.get('ultra1_cm', 0)
    ultra2 = raw.get('ultra2_cm', 0)
    roll_diff = ultra1 - ultra2
    roll_centered = abs(roll_diff) < 0.5

    # Vibration magnitude
    ax = raw.get('accel_x', 0)
    ay = raw.get('accel_y', 0)
    az = raw.get('accel_z', 0)
    vib_magnitude = (ax**2 + ay**2 + az**2) ** 0.5

    return {
        'motor1': {
            'current': round(current1, 2),
            'rpm': rpm1,
            'state': state1,
            },
        'motor2': {
            'current': round(current2, 2),
            'rpm': rpm2,
            'state': state2,
            },
        'weight': {
            'kg': round(weight_kg, 3),
            'calibrated': weight_calibrated,
            },
        'temperature': {
            'ambient': round(raw.get('dht_temp', 0), 1),
            'humidity': round(raw.get('dht_hum', 0), 1),
            'thermocouple': round(tc_temp, 1),
            'tc_connected': tc_connected,
            },
        'roll': {
            'left_cm': round(ultra1, 1),
            'right_cm': round(ultra2, 1),
            'diff_cm': round(roll-diff, 1),
            'centered': roll_centered,
            },
        'vibration': {
            'magnitude_g': round(vib_magnitude, 2),
            },
        }

# SPI Data Callback

def on_sensor_data(raw_data: dict):
    """called at 100hz when new sensor data arrives."""
    global latest_sensor_data, latest_interpreted_data

    latest_sensor_data = raw_data
    latest_interpreted_data = interpret_sensor_data(raw_data)

    # Check for critical alerts
    if latest_interpreted_data['motor1']['state']['code'] == 3:
        log_message("ALERT", "Motor 1 JAMMED - High current, zero RPM")

    if latest_interpreted_data['motor2']['state']['code'] == 3:
        log_message("ALERT", "Motor2 JAMMED - High current, Zero RPM")
    if not latest_interpreted_data['temperature']['tc_connected']:
        log_message("ALERT", "Thermocouple disconnected")

# Flask Routes

@app.route('/')
def index():
    return render_template('dashboard.html')

@app.route('/calibrate')
def calibrate():
    return render_template('calibrate.html')

@app.route('/logs')
def logs():
    return render_template('logs.html')

# Live log stream (Server-Sent Events SSE)

@app.route('/stream/logs')
def stream_logs():
    """SSE endpoint - browser recieves logs in real time."""
    def generate():
        # Send recent log on connect
        recent = []
        temp_queue = queue.Queue()

        while not log_queue.empty():
            try: 
                recent.append(log_queue.get_nowait())
            except queue.Empty:
                break

        for log in recent[-50]: # Last 50 lines
            yield f"data: {log}\n\n"
            temp_queue.put(log)

        # Put them back
        for log in recent: 
            log_queue.put(log)

        # Streaming new logs
        while True:
            try:
                log_line = log_queue.get(timeout=30)
                yield f"data: {log_line}\n\n"
            except queue.Empty:
                yield ": keepalive\n\n"

    return Response(generate(), minetype='text/event-stream')


# API: Sensor Data

@app.route('/api/sensors')
def api_sensors():
    """Get latest interpreted sensor data."""
    if not latest_interpreted_data:
        return jsonify({'error': 'No data available'}), 503

    return jsonify({
        'timestam': latest_sensor_data.get('timestamp', 0),
        'data': latest_interpreted_data,
        })

# API: Load Cell Calibration

@app.route('/api/calibrate/loadcell/start', methods=['POST'])
def start_loadcell_calibration():
    """Begin load cell calibration."""
    global lc_calibrator

    data = request.get_json() or {}
    known_weight = data.get('weight_kg', 5.0)

    lc_calibrator = LoadCellCalibrator(spi_reader, log_callback=log_message)
    lc_calibrator.start(known_weight_kg = known_weight)

    return jsonify({
        'status': lc_calibrator.state,
        'weight_kg': known_weight,
        })

@app.route('/api/calibrate/loadcell/confirm', methods=['POST'])
def confirm_loadcell_weight():
    """User confirms weight is placed."""
    if not lc_calibrator:
        return jsonify({'error': 'No calibration in progress'}), 400

    success = lc_calibrator.confirm_weight_placed()

    return jsonify ({
        'success': success,
        'status': lc_calibrator.state,
        'factor': lc_calibrator.calculated_factor if success else 0,
        })

@app.route('/api/calibrate/loadcell/cancel', methods=['POST'])
def cancel_loadcell_calibration():
    """Cancel calibration."""
    if lc_calibrator:
        lc_calibrator.cancel()
    return jsonify({'ok': True})

# API: Thermocouple Calibration

@app.route('/api/calibrate/thermocouple', methods=['POST'])
def calibrate_thermocouple():
    """Run thermocouple calibration using MLX90614"""
    global tc_calibrator

    tc_calibrator = ThermocoupleCalibrator(spi_reader, log_callback=log_message)
    success = tc_calibrator.calibrate()

    return jsonify({'success': success})

@app.route('/api/calibrate/thermocouple/check', mehods=['POST'])
def check_thermocouple():
    """Check thermocouple accruacy against MLX90614"""
    global tc_calibrator
    
    tc_calibrator = ThermocoupleCalibrator(spi_reader, log_callback=log_message)
    result = tc_calibrator.check_accuracy()

    return jsonify(result)

# Startup

def init_system():
    """Initialize SPI reader and start data acquisition."""
    global spi_reader

    log_message("System", "Initializing...")

    spi_reader = ESP32SPIReader(
            on_data=on_sensor_data, 
            on_log=log_message
    )

    spi_reader.start()
    log_message("System", "Ready - Dashboard at http://<pi-ip>:5000")

if __name__ = '__main__':
    # Start SPI reader in background
    init_thread = threading.Thread(target=init_system, daemon=True)
    init_thread.start()

    time.sleep(2)   # Let SPI initialize

    # Start Flask web server
    app.run(host='0.0.0.0.', port=5000, debug=False, threaded=True)
