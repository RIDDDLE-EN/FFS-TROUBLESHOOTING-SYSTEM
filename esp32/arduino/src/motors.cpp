#include "motors.h"
#include "config.h"
#include "logger.h"
#include <Arduino.h>
#include <cmath>

struct EncoderState {
	volatile int32_t counter;
	volatile int32_t lastSample;
	volatile float rpmSmoothed;
	volatile bool forward;
};

static EncoderState enc1 = {0, 0, 0.0f, true};
static EncoderState enc2 = {0, 0, 0.0f, true};
static MotorModule *instanceRef = nullptr;

void IRAM_ATTR MotorModule::encoder1_ISR() {
	enc1.counter++;
	enc1.forward = (digitalRead(ENCODER1_DT) == HIGH);
}

void IRAM_ATTR MotorModule::encoder2_ISR() {
	enc2.counter++;
	enc2.forward = (digitalRead(ENCODER2_DT) == HIGH);
}

void MotorModule::init() {
	pinMode(c1, INPUT_PULLUP);
	pinMode(d1, INPUT_PULLUP);
	pinMode(curr1, INPUT);

	pinMode(c2, INPUT_PULLUP);
	pinMode(d2, INPUT_PULLUP);
	pinMode(curr2, INPUT);

	attachInterrupt(digitalPinToInterrupt(c1), encoder1_ISR, RISING);
	attachInterrupt(digitalPinToInterrupt(c2), encoder2_ISR, RISING);

	analogReadResolution(12);
	analogSetAttenuation(ADC_11db);

	enc1.lastSample = enc2.lastSample = millis();
	calibrateCurrentOffsets();
}

void MotorModule::calibrateCurrentOffsets() {
	LOG("[MOTORS] Calibrating ACS712 Zero Offsets (Ensure motors are OFF)...");
	long sum1 = 0, sum2 = 0;
	for (int i = 0; i < 200; i++) {
		sum1 += analogRead(curr1);
		sum2 += analogRead(curr2);
		vTaskDelay(pdMS_TO_TICKS(2));
	}
	zeroAdc1 = (float)sum1 / 200.0f;
	zeroAdc2 = (float)sum2 / 200.0f;
	LOG("[MOTORS] Baseline established -> Zero1: %.1f ADC, Zero2: %.1f ADC", zeroAdc1, zeroAdc2);
}

float MotorModule::calculateRPM(EncoderState &enc) {
	uint32_t now = millis();
	uint32_t elapsedMs = now - enc.lastSample;

	if (elapsedMs >= ENCODER_SAMPLE_MS) {
		noInterrupts();
		long counter = enc.counter;
		enc.counter = 0;
		bool direction = enc.forward;
		interrupts();

		float elapsedMins = (float)elapsedMs / 60000.0f;
		float rawRpm = ((float)counter / ENCODER_PPR) / elapsedMins;
		if (counter == 0 && (elapsedMs * 1000UL > ENCODER_STOPPED_US)) rawRpm = 0.0f;
		
		const float ALPHA = 0.3f;
		float targetRpm = direction ? rawRpm : -rawRpm;
		enc.rpmSmoothed = (ALPHA * targetRpm) + ((1.0f - ALPHA) * enc.rpmSmoothed);
		enc.lastSample = now;
	}
	return enc.rpmSmoothed;
}

MotorData MotorModule::update(MotorData &data) {
	int rawC1 = analogRead(curr1);
	filteredCurr1 = (0.1f * rawC1) + (0.9f * filteredCurr1);
	float v1 = ((filteredCurr1 - zeroAdc1) / ADC_RESOLUTION) * VREF;
	data.current1 = v1 / SENSITIVITY;
	if (fabsf(data.current1) < 0.12f) data.current1 = 0.0f;
	
	int rawC2 = analogRead(curr2);
	filteredCurr2 = (0.1f * rawC2) + (0.9f * filteredCurr2);
	float v2 = ((filteredCurr2 - zeroAdc2) / ADC_RESOLUTION) * VREF;
	data.current2 = v2 / SENSITIVITY;
	if (fabsf(data.current2) < 0.12f) data.current2 = 0.0f;

	data.rpm1 = calculateRPM(enc1);
	data.rpm2 = calculateRPM(enc2);
	data.motor1_running = (fabsf(data.rpm1) > MOTOR_MIN_RPM);
	data.motor2_running = (fabsf(data.rpm2) > MOTOR_MIN_RPM);

	return data;
}
