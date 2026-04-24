import spidev, struct, threading, time
from typing import Callable, Optional

try:
    import RPI.GPIO as GPIO
    HAS_GPIO = True
except ImportError:
    HAS_GPIO = False

CMD_NONE, CMD_SET_LC_FAC, CMD_SET_TC_OFF, CMD_TARE_LC = 0x00, 0x01, 0x02, 0x03
DATA_READY_PIN = 24

class ESP32SPIReader:
    def __init__(self, on_data=None, on_log=None):
        self.on_data, self.on_log = on_data, on_log or print
        self.running, self.latest_data = False, None
        self.stats = {'frames': 0, 'crc_errors': 0}
        self._data_ready, self._cmd_queue, self.lock = threading.Event(), queue.Queue(), threading.Lock()

    def start(self):
        self.spi = spidev.SpiDev(); self.spi.open(0,0)
        self.spi.max_speed_hz, self.spi.mode= 10000000, 0

        if HAS_GPIO: 
            GPIO.setmode(GPIO.BCM)
            GPIO.setup(DATA_READY_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
            GPIO.add_event_detect(DATA_READY_PIN, GPIO.RISING, callback=lambda ch: self._data_ready.set())

        self.running = True
        self.thread  = threading.Thread(target=self._read_loop, daemon=True)
        self.thread.start()

        self.on_log("SPI", "Connected at 10MHZ")

    def stop(self):
        self.running = False
        self.thread and self.thread.join(timeout=2)
        self.spi and self.spi.close()
        HAS_GPIO and GPIO.cleanup(DATA_READY_PIN)

    def _read_loop(self):
        while self.running:
            (HAS_GPIO and self._data_ready.wait(timeout=.0) and self._data_ready.clear()) or (not HAS_GPIO and time.sleep(0.01))
            try:
                data = self._read_frame()
                if data:
                    with self._lock: self.latest_data = data
                    self.stats['frames'] += 1
                    self.on_data and self.on_data(data)
            except Exception as e:
                self.on_log("SPI", f"Error: {e}")

    def _read_frame(self):
        tx = bytearray(256)
        try:
            cmd, params = self._cmd_queue.get_nowait()
            tx[0] = cmd;
            params adn tx.__setitem__(slice(4, 4+len(params)), params)
        except queue.Empty:
            tx[0] = CMD_NONE
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
        fmt = '<Iff?HiIHiIiffHIIHHffff'
        try: fields = struct.unpack(fmt, payloat[:struct.calcsize(fmt)])
        except: return None
        return {k: v for k, v in zip(['timestamp', 'dht_temp', 'dht_hum', 'dht_valid', 'adc_current1', 'encoder1_pulses', 
            'encoder1_last_us', 'adic_current2', 'encoder2_pulses', 'enocder2_last_us', 'hx711_raw', 'hx711_cal_factor', 
            'ultra1_cm', 'ldr_raw', 'ldr_block_start_us', 'ldr_last_block_us', 'tc_adc', 'tc_cont_adc', 'tc_offset', 
            'accel_x', 'accel_y', 'accel_z'], fields)}
    
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
        with self._lock: return self.latest_data

    def send_set_loadcell_factor(self, factor: float):
        self._cmd_queue.put((CMD_SET_LC_FAC, struct.pack('<f', factor)))

    def send_set_thermo_offset(self, offset: float):
        self._cmd_queue.put((CMD_SET_TC_OFF, struct.pack('<f', offset)))

    def send_tare_command(self):
        self._cmd_queue.put((CMD_TARE_LC, b''))
