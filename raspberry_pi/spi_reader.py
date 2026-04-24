import spidev
import struct
import threading
import time
from typing import Callable, Optional

try:
    import RPI.GPIO as GPIO
except ImportError:
    GPIO = None

CMD_NONE = 0x00
CMD_SET_LC_FAC = 0x01
CMD_SET_TC_OFF = 0x02
CMD_TARE_LC = 0x03

DATA_READY_PIN = 24

class ESP32SPIReader:
    def __init__(self, on_data: Optional[Callable] = None, on_log: Optional[Callable = None):
        self.on_data = on_data
        self.on_log = on_log or print

        self.spi = None
        self.running = False
        self.thread = None

        self.latest_data = None
        self.stats = {'frames': 0, 'crc_errors':0}

        self._data_ready = threading.Event()

    def start(self):
        self.spi = spidev.SpiDev()
        self.spi.open(0,0)
        self.spi.max_speed_hz = 10000000
        self.spi.mode = 0

        if GPIO: 
            GPIO.setmode(GPIO.BCM)
            GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
            GPIO.add_event_detect(DATA_READY_PIN, GPIO.RISING, callback=lambda ch: self._data_ready.set())

        self.running = True
        self.thread  = threading.Thread(target=self._read_loop, daemon=True)
        self.thread.start()

        self.on_log("SPI", "Connected at 10MHZ")

    def stop(self):
        self.running = False
        if self.thread:
            self.thread.join()
        if self.spi:
            self.spi.close()
        if GPIO:
            GPIO.cleanup(DATA_READY_PIN)

    def _read_loop(self):
        while self.running:
            if GPIO:
                triggered = self._data_ready.wait(timeout=0.1)
                if not triggered:
                    continue
                self._data_ready.clear()
            else:
                time.sleep(0.01)

            try:
                data = self._read_frame()
                if data:
                    self.latest_data = data
                    self.stats['frames'] += 1
                    if self.on_data:
                        self.on_data(data)
            except Exception as e:
                self.on_log("SPI", f"Error: {e}")

    def _read_frame(self):
        tx = bytearray(256)
        rx = bytes(self.spi.xfer2(list(tx)))

        # CRC check (last 2 bytes)
        crc_rx = struct.unpack('<H', rx[254:256])[0]
        crc_calc= self._crc16(rx[:254])
        if crc_rx != crc_calc:
            self.stats['crc_errors'] += 1
            return None

        #unpack raw sensor data (bytes 8-254)
        return self._unpack(rx[8:254])

    def _unpack(self, payload):
        return {
                'timestamp': struct.unpack('<I', payload[0:4])[0],
                'dht_temp': struct.unpack('<f', payload[4:8])[0],
                'dht_hum': struct.unpack('<f', payload[8:12])[0],
                'adc_current1': struct.unpack('<H', payload[12:14])[0],
                'encoder1_pulses': struct.unpack('<i', payload[14:18])[0],
                'adc_current2': struct.unpack('<H', payload[18:20])[0],
                'encoder2_pulses': struct.unpack('<i', payload[20:24])[0],
                'hx711_raw': struct.unpack('i', payload[24:28])[0],
                'hx711_cal_factor': struct.unpack('<f', payload[28:32])[0],
                'ultra1_cm': struct.unpack('<f', payload[32:36])[0],
                'ultra2_cm': struct.unpack('<f', payload[36:40])[0],
                'ldr_raw': struct.unpack('<H', payload[40:42])[0],
                'tc_adc': struct.unpack('<H', payload[42:44])[0],
                'tc_offset': struct.unpack('<f', payload[44:48])[0],
                'accel_x': struct.unpack('<f', payload[48:52])[0],
                'accel_y': struct.unpack('<f', payload[52:56])[0],
                'accel_z': struct.unpack('<f', payload[56:60])[0],
                }
    
    def _crc16(self, data):
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc <<=1
                crc &= 0xFFFF
        return crc

    def get_latest_data(self):
        return self.latest_data

    def send_set_loadcell_factor(self, factor: float):
        self._send_command(CMD_SET_LC_FAC, struct.pack('<f', factor))

    def send_set_thermo_offset(self, offset: float):
        self._send_command(CMD_SET_TC_OFF, struct.pack('<f', offset))

    def send_tare_command(self):
        self._send_comand(CMD_TARE_LC)

    def _send_command(self, cmd, params=b''):
        #commands sent on next SPI read
        pass #implement full duplex command sending
