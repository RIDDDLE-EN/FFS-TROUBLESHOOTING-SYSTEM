#!usr/bin/python3

import RPi.GPIO as GPIO
from time import sleep

class Servo:
    def __init__(self, pin):
        self.pin = pin
        global pwm = GPIO.PWM(self.pin, 50)

    def begin(self):
        GPIO.setmode(GPIO.BOARD)
        GPIO.setup(11, GPIO.OUT)
        pwm.start(0)
    
    def move(self, angle):
        self.angle = angle
        duty = angle / 18 + 3
        GPIO.output(self.pin, True)
        self.pwm.ChangeDutyCycle(duty)
        sleep(1)
        GPIO.output(self.pin, False)
        pwm.ChangeDutyCycle(duty)
