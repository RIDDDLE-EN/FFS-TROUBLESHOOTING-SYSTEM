"""
control.py - manages:
    feeding > filling > sealing > cutting
    Includes vibration frequency analysis for knife sharpness
"""

import time
import numpy as np

class VibrationAnalyzer:
    """ Analyzes vibration for knife sharpness and jaw pressure."""
    def __init__(self):
        self.sample_buffer = []
        self.sample_rate = 100

        #knife sharpness thresholds
        self.sharp_freq_min = 45
        self.shar_freq_max = 55
        self.dull_freq_min = 30
        self.dull_freq_max = 40

    def add_sample(self, accel_x, accel_y, accel_z):
        """Add vibration sample"""
        magnitude = (accel_x**2 + accel_y**2 + accel_z**2) ** 0.5
        self.sample_buffer.append(magnitude)

        #keep last 2 seconds of data
        if len(self.sample_buffer) > 200:
            self.sample_buffer.pop(0)

    def analyze(self):
        """Returns {amplitude, freq, knife_status}"""
        if len(self.sample_buffer) < 50:
            return {'amplitude_g':0, 'frequency_hz':
