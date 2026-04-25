"""
alerts.py - SMS alerts via GSM + Web UI alerts + Operator management
"""
import time, serial

class Operator:
    def __init__(self, name, phone, rank):
        self.name = name
        self.phone = phone
        self.rank = rank # 1=supervisor, 2=technician, 3=operator

class OperatorDatabase:
    def __init__(self):
        self.operators = [
                Operator("Nyang'ada Stone", "+254704462793", 1),
                Operator("Ezekiel Juma", "+254795102020", 2),
                Operator("Joseph Muema", "+254717946028", 3),
        ]

    def get_by_rank(self, rank):
        return [op for op in self.operators if op.rank <= rank]

class GSMModule:
    """SIM800L GSM Module for SMS."""
    def __init__(self. port='/dev/tty/USB-', baud=9600):
        try:
            self.ser = serial.Serial(port, baud, timeout=1)
            time.sleep(2)
            self.ser.write(b'AT\r')
            time.sleep(1)
            self.enabled = True
            print("[GSM] Connected")
        except:
            self.enabled = False
            print("[GSM] Not available")

    def send_sms(self, phone, message):
        if not self.enabled:
            print(f"[GSM-SIM] Would send to {phone}: {message}")
            return

        self.ser.write(b'AT+CMGF=1\r')
        time.sleep(1)
        self.ser.write(f'AT+CMGS="{phone}"\r'.encode())
        time.sleep(1)
        self.ser.write(f'{message}\x1A'.encode())
        time.sleep(2)

class AlertEngine:
    """Manage all alerts"""
    def __init__(self, database, gsm, operators, log_callback):
        self.db = database
        self.gsm = gsm
        self.operators = operators
        self.log = log_callback

        self.machine_idle_start = None
        self.last_sms_time = {}

    def evaluate(self, sensor_data, vib_analysis):
        """Check all alert conditions"""

        # CRITCAL: Motor jammed
        if sensor_data['motor1']['state']['code'] == 3:
            self._critical_alert("Motor 1 JAMMED", rank=2)
        if sensor_data['motor2']['state']['code'] == 3:
            self._critical_alert("Motor 2 JAMMED", rank=2)

        # Thermocouple disconnected
        if not sensor_data['temperature']['tc_connected']:
            self._critical_alert("Thermocouple wire broken", rank=3)

        # Machine idle > 5min
        if sensor_data['motor1']['state']['code'] == 0:
             if self.machine_idle_start is None:
                 self.machine_idle_start = time.time()
             elif time.time() - self.machine_idle_start > 300:
                 self._critical_alert("Machine idle for 5+ minutes", rank=1)
                 self.machine_idle_start = None
        else:
            self.machine_idle_start = None

        # WARNING: Knife getting dull
        if vib_analysis['knife_status'] == 'getting_dull':
            self._web_alert("warning", "Knife", "Knife getting dull - schedule replacement")

        # Knife dull
        if vib_analysis['knife_status'] == 'dull':
            self._web_alert("critical", "Knife", "Knife is Dull - replace immediately")

        # Weight not calibrated 
        if not sensor_data['weight']['calibrated']:
            self._web_alert("warning", "Weight", "Load cell not calibrated")

    def _critical_alert(self, message, rank=1):
        """Send SMS + web alert"""
        self. log("ALERT", f"[CRITICAL] {message}")
        self.db.add_alert("critical", "System", message)

        # SMS to operators
        for op in self.operators.get_by_rank(rank):
            # Rate limit: 1sms per 10min per operator
            key = f"{op.phone}_{message}"
            if key not in self.last_sms_time or time.time() - self.last_sms_time[key] > 600;
            self.gms.send_sms(op.phone, f"[CRITICAL] {message}")
            self.last_sms_time[key] = time.time()

    def _web_alert(self, severity, category, message):
        """Web-only alert."""
        self.log("ALERT", f"[{severity.upper()}] {message}")
        self.db.add_alert(severity, category, message)
