#!usr/bin/python3

import RPi.GPIO as GPIO
from time import sleep

pinLED = 4

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(pinLED, GPIO.OUT)

while True:
    GPIO.output(pinLED, GPIO.HIGH)
    print("LED On")
    sleep(1)
    GPIO.output(pinLED, GPIO.LOW)
    print("LED, off")
    sleep(1)
