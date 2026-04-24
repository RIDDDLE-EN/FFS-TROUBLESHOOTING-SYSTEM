import time
import smbus2
from typing import Optional, Callable

#
# MLX90614 IR Temprature sensor on Pi I2C
#

class MLX90614:
    def __init__(self, bus_num=1, address=0x5A):
        self.bus = smbus2.SMBus(bus_num)
        self.address = address
 
    def read_object_temp_c(self) -> float:
        # Read from register 0x07 (object temperature)
        data = self.bus.read_word_data(self.address, 0x07)
        temp_k = data * 0.02 - 0.01 #convert to Kelvin
        return temp_k - 273.15      #convert to celsius

    def read_ambient_temp_c(self) -> float:
        data = self.bus.read_word_data(self.address, 0x06)
        temp_k = data * 0.02 - 0.01
        return temp_k - 273.15

#
# Load Cell Calibration
#

class LoadCellCalibrator:
    """
    Calibrates HX711 load cell. 
    Steps:
        1. Tare (zero with no load)
        2. Place known weight
        3. Calculate factor = raw_value / known_weight
        4. Send factor ro ESP32 for NVS storage
    """
    
    def __init__(self, spi_reader_log_callback: Optional[Callable] = None):
        self.spi = spi_reader
        self.log = log_callback or print

        self.state = 'idle'     # idle, taring, waiting_for_weight, measuring, done
        self.known_weight_kg = 0.0
        self.raw_reading = 0
        self.calculated_factor = 0.0

    def start(self, known_weight_kg: float):
        self.known_weight_kg = known_weight_kg
        self.state = 'taring'

        self.log("LoadCell", f"Calibartion started with {known_weight_kg} kg reference")
        self.log("LoadCell", "Remove ALL weight from scale")

        #send tare command to ESP32
        self.spi.send_tare_command()

        time.sleep(3)

        self.state = 'waiting_for_weight'
        self.log("LoadCell", f"Tare complete. Place {known_weight_kg} kg on scale")
        self.log("LoadCell", "Click 'Confirm Weight Placed' when ready")

    def confirm_weight_placed(self):
        if self.state != 'waiting_for_weight':
            return False

        self.state = 'measuring'
        self.log("LoadCell", "measuring...")

        #read multiple samples from ESP32
        samples = []
        for _ in range(10):
            data = self.spi.get_latest_data()
            if data:
                samples.append(data['hx711_raw'])
            time.sleep(0.1)

        if not samples or all(s == 0 for s in samples):
            self.state = 'error'
            self.log("LoadCell", "ERROR: No valid reading from Load Cell")
            return False

        self.raw_reading = sum(samples) / len(samples)
        self.calculated_factor = self.raw_reading / self.known_weight_kg

        #send factor to ESP32 for NVS storage
        self.spi.send_set_loadcell_factor(self.calculated_factor)

        self.state = 'done'
        self.log("LoadCell", f"Calibration complete")
        self.log("LoadCell", f" Raw: {self.raw_reading:.0f}")
        self.log("LoadCell", f" Factor: {self.calculated_factor:.2f}")
        self.log("LoadCell", "Remove calibration weight - scale ready to use")

        return True

    def cancel(self):
        self.state = 'idle'
        self.log("LoadCell", "Calibration Cancelled")


# 
# Thermocouple Calibration
#

class ThermocoupleCalibrator:
    """
    Calibrates thermocouple using MLX90614 IR Sensor as reference.

    Steps:
        1. Heat system to stable temperature (user-controlled)
        2. Read both thermocouple ADC and MLX90614
        3. Calculate offset
        4. send offset to ESP32 for NVS storage
    """
    def __init__(self, spi_reader, log_callback: Optional[Callable] = None
