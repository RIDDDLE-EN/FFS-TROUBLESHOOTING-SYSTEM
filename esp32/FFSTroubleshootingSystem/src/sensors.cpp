#include "sensors.h"
#include "config.h"
#include "logger.h"

#include <DHT.h>
#include <Adafruit_Sensors.h>
#include <Adafruit_MPU6050.h>
#include <Ultrasonic.h>
#include <HX711.h>
#include <Wire.h>
#include <Preferences.h>
#include <arduinoFFT.h>
#include <cmath>

static DHT		dht(DHT_PIN, DHT11);
static Ultrasonic	ultra1(ULTRA1_TRIG, ULTRA1_ECHO);
static Ultrasonic	ultra2(ULTRA2_TRIG, ULTRA2_ECHO);
static Preferences	prefs;
static HX711		scale;
static Adafruit_MPU6050	mpu;

struct EncoderState {
	volatile int32_t counter;
	volatile int32_t lastSample;
	volatile float	 rpm;
	volatile float   rpmSmoothed;
	volatile bool     forward;
};

static EncoderState enc1 = {0, 0, 0, 0, true};
static EncoderState enc2 = {0, 0, 0, 0, true};

void IRAM_ATTR encoder1_ISR() {
	enc1.counter++;
	enc1.forward = (digitalRead(ENCODER2_DT) == HIGH);
}

void IRAM_ATTR encoder2_ISR() {
	enc2.counter++;
	enc2.forward = (digitalRead(ENCODER2_DT) == HIGH);
}

static void encoderRPM(const EncoderState &enc) {
	uint32_t now = millis();
	const float ALPHA = 0.3f;

	if (now - enc.lastSample >= ENCODER_SAMPLE_MS) {
		noInterrupts();
		long counter = enc.counter;
		enc.counter = 0;
		interrupts();

		float rawRPM = ((float)counter / ENCODER_PPR) * (60000.0f / ENCODER_SAMPLE_MS);
		rawRPM = constrain(rawRPM, 0.0f, (float)RPM_MAX);

		enc.rpmSmoothed = ALPHA * rawRPM + (1.0f - ALPHA) *rpmSmoothed;
		if (!enc.forward) enc.rpmSmoothed = -enc.rpmSmoothed;
		enc.rpm = enc.rpmSmoothed;

		enc.lastSample = now;
	}
}

// LDR
static bool  	beamBlocked 		= false;
static uint32_t blockStartMicros 	= 0;
static uint32_t lastBlockDurationMicros	= 0;
static uint32_t bagCounter		= 0;

// Calibration
static float loadCellFactor = 0.0f;
static float thermoOffset   = 0.0f;

SemaphoreHandle_t mutexCalibration = nullptr;

void setLoadCellFactor(float factor) {
	if (xSemaphoreTake(mutextCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return;
	loadCellFactor = factor;
	if (prefs.begin("cal", false)) {
		prefs.putFloat("lc-factor", factor);
		prefs.end();
	}
	scale.set_scale(factor);
	xSemaphoreGive(mutexCalibration);
	LOG("[CAL] Load cell factor set: %.4f", factor);
}

float getLoadCellFactor() {
	float val = 0.0f;
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) == pdTRUE) {
		val = loadCellFactor;
		xSemaphoreGive(mutexCalibration);
	}
	return val;
}

void setThermoOffset(float offset) {
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return;
	thermoOffset = offset;
	if (prefs.begin("cal", false)) {
		prefs.putFloat("offset", thermoOffset);
		prefs.end();
	}
	xSemaphoreGive(mutexCalibration);
	LOG("[CAL] Thermocouple offset set: %.2f C", offset);
}

float getThermoOffset() {
	float val = 0.0f;
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) == pdTRUE) {
		val = thermoOffset;
		xSemaphoreGive(mutexCalibration);
	}
	return val;
}

void tareLoadCell() {
	scale.tare(20);
	LOG("[LoadCell] Tared");
}

int32_t readRawWeightAverage(uint8_t samples) {
	long sum = 0; int count = 0;
	for (uint8_t i = 0; i < samples; i++) {
		if (scale.is_ready()) { sum += scale.read(); count++; }
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	int32_t result = count > 0 ? (int32_t)(sum / count) : 0;
	LOG("[CAL] Raw weight average (%d samples): %ld", count, (long)result);
	return result;
}

void resetBagCounter() {
	bagCounter = 0;
	LOG("[BAGS] Counter reset");
}

// FreeRTOS queques
QueueHandle_t queueRawToProcessing		= nullptr;
QueueHandle_t queueVibrationToProcessing	= nullptr;
QueueHandle_t queueProcessedToSPI		= nullptr;

// sensorInit
void sensorsInit(){
	LOG("[SENSORS] Initializing");

	mutexCalibration 	= xSemaphoreCreateMutex();
	queueRawToProcessing	= xQueueCreate(2, sizeof(RawSensorData));
	queueVibrationToProcessing = xQueueCreate(2, sizeof(VibrationData));
	queueProcessedToSPI	= xQueueCreate(2, sizeof(InterpretedSensorData));

	pinMode(LASER_PIN, OUTUPUT);
	digitalWrite(LASER_PIN, HIGH);

	pinMode(ENCODER1_CLK, INPUT_PULLUP);
	pinMode(ENCODER1_DT,  INPUT_PULLUP);
	pinMode(ENCODER2_CLK, INPUT_PULLUP);
	pinMode(ENCODER2_DT,  INPUT_PULLUP);

	analogReadReasolution(12);
	analogSetAttenuation(ADC_11db);

	attachInterrupt(digitalPinToInterrupt(ENCOER1_CLK), encoder1_ISR, RISING);
	attachInterrupt(digitalPinToInterrupt(ENCODER2_CLK), encoder2_ISR, RISING);

	Wire.begin(MPU_SDA, MPU_SCL);
	Wire.setClock(400000);

	dht.begin();
	scale.begin(HX_DT, HX_SCK);

	if (prefs.begin("cal", true)) {
		loadCellFactor = prefs.getFloat("lc_factor", 0.0f);
		thermoOffset   = prefs.getFloat("tc_offset", 0.0f);
		prefs.end();
	}

	if (loadCellFactor != 0.0f) {
		scale.set_scale(loadCellFactor);
		scale.tare(10);
		LOG("[LOADCELL] Calibration factor loaded: %.4f", loadCellFactor);
	} else {
		LOG("[LOADCELL] WARNING: No calibration factor - run calibration from web UI");
	}

	if (mpu.begin(MPU_ADDR)) {
		mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
		mpu.setGyroRAnge(MPU_RANGE_500_DEG);
		mpu.setFilterBandwidth(MPU_BAND_260_HZ);
		LOG("[MPU6050] OK - +/- 16g, DLPF 260 HX bandwidth");
	} else {
		LOG("[MPU6050] FAILED - Check I2C wiring and AD0 pin (addr 0x%02X)", MPU_ADDR);
	}

	enc1.lastSample, enc2.lastSample = millis();

	LOG("[SENSORS] Ready");
}

// Sensor Read Task
void sensorReadTask(void *parameter) {
	TickType_t 	 xLastWakeTime = xTaskGetTickCount();
	const TickType_t xPeriod       = pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS);

	LOG("[TASK] SensorRead core %d", (int)xPortGetCoreID());

	while (true) {
		RawSensorData data = {};
		data.timestamp_ms = millis();

		// DHT11
		data.dht_temp = dht.readTemperature();
		data.dht_hum  = dht.readHumidity();
		data.dht_valid = !isnan(data.dht_temp) && !isnan(data.dht_hum);

		// MOTOR1
		data.current1 = analogRead(ACS712_1);
		noInterrupts();
		data.encoder1_counter = enc1.counter;
		data.encoder1_last_us = enc1.lastSample;
		interrupts();

		// MOTOR2
		data.current2 = analogRead(ACS712_1);
		noInterrupts();
		data.encoder2_counter 	= enc2.counter;
		data.encoder2_last_us	= enc2.lastSample;
		interrupts();

		// LOAD CELL
		data.hx_raw = scale.is_ready() ? scale.read() : 0;

		// ULTRASONICS
		data.ultra1_cm = ultra1.read();
		data.ultra2_cm = ultra2.read();

		// LDR
		data.ldr_raw = analogRead(LDR_PIN);
		if (!beamBlocked && data.ldr_raw < LDR_BLOCKED_THRESHOLD) {
			beamBlocked = true;
			blockStartMicros = micros();
			data.ldr_beam_blocked = true;
			data.ldr_block_start_us = blockStartMicros;
		} else if (beamBlocked && data.ldr_raw > LDR_UNBLOCKED_THRESHOLD) {
			beamBlocked = false;
			lastBlockDurationMicros = micros() - blockStartMicros;
			bagCounter++;
			data.ldr_beam_blocked = false;
			data.ldr_block_start_us = 0;
		} else {
			data.ldr_beam_blocked = beamBlocked;
			data.ldr_block_start_us = beamBlocked ? blockStartMicros : 0;
		}

		// THERMOCOUPLE
		data.tc_adc = analogRead(TC_PLUS);
		data.tc_cont = analogRead(TC_CONTINUITY);

		// MPU6050
		sensors_event_t a, g, temp;
		if (mpu.getEvent(&a, &g, &temp)) {
			data.accel_x = a.acceleration.x;
			data.accel_y = a.acceleration.y;
			data.accel_z = a.acceleration.z;
		}

		xQueueOverwrite(queueRawToProcessing, &data);
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}
