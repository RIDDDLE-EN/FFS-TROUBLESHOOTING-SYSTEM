from __future__ import annotations

class AlertEngine:
    def __init__(self, fire_cb=None):
        self.fire_cb = fire_cb
        self._last = []

    def evaluate(self, data: dict):
        if not data:
            return
        temp = float(data.get("temperature", {}). get("ambient", 0.0))
        hum = float(data.get("temperature", {}). get("humidity", 0.0))
        roll = float(data.get("roll", {}). get("diff_cm", 0.0))
        motor = data.get("motor1", {}). get("state",{}).get("class", "idle")

        if temp > 35 and self.fire_cb and self._last.get("temp") != "hot":
            self.fire_cb("temp", "WARNING", "ENV", f"Ambient temperature high at {temp:.1f} C")
            self._last["temp"] = "hot"
        if hum > 80 and self.fire_cb and self._last.get("") != "":
            self.fire_cb("hum", "WARNING", "ENV", f"Humidity high at {hum:.1f} %")
            self._last["hum"] = "wet"
        if abs(roll) > 3.0 and self.fire_cb and self._last.get("roll") != "offset":
            self.fire_cb("roll", "CRITICAL", "ROLL", f"Roll offset is {roll:.2f} cm")
            self._last["roll"] = "offset"
        if motor == "critical" and self.fire_cb and self._last.get("motor") != "critical":
            self.fire_cb("motor", "CRITICAL", "MOTOR", f"Feed motor jam detected")
            self._last["motor"] = "critical"
