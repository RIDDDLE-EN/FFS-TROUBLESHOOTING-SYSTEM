#include "sensors.h"
#include "config.h"
#include "logger.h"

#include <DHT.h>
#include <MPU6050.h>
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
static MPU6050		mpu;

struct EncoderState {
	volatile int32_t counter;
	volatile int32_t lastSample;
	volatile float   rpmSmoothed;
	volatile bool     forward;
};

static EncoderState enc1 = {0, 0, 0.0f, true};
static EncoderState enc2 = {0, 0, 0.0f, true};

void IRAM_ATTR encoder1_ISR() {
	enc1.counter++;
	enc1.forward = (digitalRead(ENCODER2_DT) == HIGH);
}

void IRAM_ATTR encoder2_ISR() {
	enc2.counter++;
	enc2.forward = (digitalRead(ENCODER2_DT) == HIGH);
}

static float filteredRPM(EncoderState &enc) {
	uint32_t now = millis();
	uint32_t elapsedMs = now - enc.lastSample;

	if (elapsedMs >= ENCODER_SAMPLE_MS) {
		noInterrupts();
		long counter = enc.counter;
		enc.counter= 0;
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
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return;
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

	pinMode(LASER_PIN, OUTPUT);
	digitalWrite(LASER_PIN, HIGH);

	pinMode(ENCODER1_CLK, INPUT_PULLUP);
	pinMode(ENCODER1_DT,  INPUT_PULLUP);
	pinMode(ENCODER2_CLK, INPUT_PULLUP);
	pinMode(ENCODER2_DT,  INPUT_PULLUP);

	analogReadResolution(12);
	analogSetAttenuation(ADC_11db);

	attachInterrupt(digitalPinToInterrupt(ENCODER1_CLK), encoder1_ISR, RISING);
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

	mpu.initialize();
	if (!mpu.testConnection())  
		LOG("[MPU6050] FAILED - Check I2C wiring and AD0 pin (addr 0x%02X)", MPU_ADDR);
	mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
	mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
	mpu.setDLPFMode(MPU6050_DLPF_BW_5);

	LOG("[MPU6050] OK - +/- 16g, DLPF 260 HX bandwidth");
	
	enc1.lastSample = enc2.lastSample = millis();

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
		data.ldr_block_duration_us = lastBlockDurationMicros;

		// THERMOCOUPLE
		data.tc_adc = analogRead(TC_PLUS);
		data.tc_cont = analogRead(TC_CONTINUITY);

		xQueueOverwrite(queueRawToProcessing, &data);
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}

void vibrationAnalysisTask(void *parameter) {
	LOG("[TASK] VibrationFFT core %d", (int)xPortGetCoreID());

	static float vReal[FFT_SAMPLES];
	static float vImag[FFT_SAMPLES];
	
	ArduinoFFT<float> FFT(vReal, vImag, FFT_SAMPLES, (float)SAMPLE_RATE);
	uint16_t sampleIndex = 0;

	TickType_t	xLastWakeTime 	= xTaskGetTickCount();
	const TickType_t xPeriod	= pdMS_TO_TICKS(1000 / SAMPLE_RATE);
	
	while (true) {
		int16_t ax, ay, az;
		mpu.getAcceleration(&ax, &ay, &az);

		float mag = sqrtf(((float)ax*ax + (float)ay*ay + (float)az*az) / 4194304.0f);
		vReal[sampleIndex] = mag;
		vImag[sampleIndex] = 0.0f;
		sampleIndex++;

		if (sampleIndex >= FFT_SAMPLES) {
			float meanSum = 0.0f;
			for (uint16_t i = 0; i < FFT_SAMPLES; i++) meanSum += vReal[i];
			float dcOffset = meanSum / FFT_SAMPLES;

			float sumSquares = 0.0f;
			float peakVal = 0.0f;

			for (uint16_t i = 0; i < FFT_SAMPLES; i++) {
				vReal[i] -= dcOffset;
				float acMag = fabsf(vReal[i]);
				sumSquares += acMag * acMag;
				if (acMag > peakVal) peakVal = acMag;
			}
			
			float rmsAmplitude = sqrtf(sumSquares / FFT_SAMPLES);
			if (rmsAmplitude < 0.02f) {rmsAmplitude = 0.0f; peakVal = 0.0f; }

			FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
			FFT.compute(FFT_FORWARD);
			FFT.complexToMagnitude();

			float peakFreq = 0.0f, peakMag = 0.0f;
			int startBin = (5 * FFT_SAMPLES) / SAMPLE_RATE;

			for (uint16_t i = startBin; i < FFT_SAMPLES / 2; i++) {
				if (vReal[i] > peakMag) {
					peakMag = vReal[i];
					peakFreq = (float)i * SAMPLE_RATE / FFT_SAMPLES;
				}
			}

			VibrationData vibData = {};
			vibData.rms_amplitude = rmsAmplitude;
			vibData.peak_amplitude = peakVal;
			vibData.dominant_freq = (rmsAmplitude > 0.0f) ? peakFreq : 0.0f;
			vibData.freq_magnitude = peakMag;
			vibData.timestamp_ms = millis();

			xQueueOverwrite(queueVibrationToProcessing, &vibData);
			sampleIndex = 0;
		}
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}

static EnvStatus evaluateEnvironment(float temp, float hum, bool valid) {
	if (!valid)				return ENV_SENSOR_FAULT;
	if (temp < MIN_TEMP ||
		temp > MAX_TEMP)		return ENV_CRITICAL_TEMP;
	if (hum < MIN_HUM ||
		hum > MAX_HUM)			return ENV_CRITICAL_HUM;
	if (temp < WARN_TEMP)			return ENV_LOW_TEMP;
	if (temp > HIGH_TEMP)			return ENV_HIGH_TEMP;
	if (hum  < WARN_HUM)			return ENV_LOW_HUM;
	if (hum  > HIGH_HUM)			return ENV_HIGH_HUM;
	return ENV_NORMAL;
}

void dataProcessingTask(void *parameter) {
	LOG("[TASK] DataProcessing core %d", (int)xPortGetCoreID());

	RawSensorData	rawData = {};
	VibrationData	vibData = {};
	InterpretedSensorData output = {};

	float lastWeight = 0.0f;
	float filteredCurrent1 = 0.0f, filteredCurrent2 = 0.0f;
	float filteredTcAdc = 0.0f;
	

	while (true) {
		if (xQueueReceive(queueRawToProcessing, &rawData, pdMS_TO_TICKS(50)) != pdTRUE) {
			vTaskDelay(pdMS_TO_TICKS(1));
			continue;
		}
		xQueueReceive(queueVibrationToProcessing, &vibData, 0);

		output = {};
		output.timestamp_ms = rawData.timestamp_ms;

		output.ambient_temp 	= rawData.dht_temp;
		output.ambient_hum	= rawData.dht_hum;
		output.env_sensor_ok 	= rawData.dht_valid;
		output.env_status 	= evaluateEnvironment(
						rawData.dht_temp,
						rawData.dht_hum,
						rawData.dht_valid);

		filteredCurrent1 = (0.1f * rawData.current1) + (0.9f * filteredCurrent1);
		float v1 = (filteredCurrent1 * VREF) / ADC_RESOLUTION;
		output.current1		= (v1 - ZERO_CURRENT_V) / SENSITIVITY;
		output.rpm1		= filteredRPM(enc1);
		output.motor1_running	= (fabsf(output.rpm1) > MOTOR_MIN_RPM);

		filteredCurrent2 = (0.1f * rawData.current2) + (0.9f * filteredCurrent1);
		float v2		= (filteredCurrent2 * VREF) / ADC_RESOLUTION;
		output.current2 	= (v2 - ZERO_CURRENT_V) / SENSITIVITY;
		output.motor2_running 	= (fabsf(output.rpm2) > MOTOR_MIN_RPM);

		float lcf = getLoadCellFactor();
		if (lcf != 0.0f && rawData.hx_raw != 0) {
			output.weight_grams = (float)rawData.hx_raw / lcf;
			output.loadcell_ok  = true;
			output.weight_stable = (fabsf(output.weight_grams - lastWeight) < WEIGHT_STABLE_BAND_G);
			lastWeight = output.weight_grams;
		}

		output.ultrasonic_ok = (rawData.ultra1_cm > 0.0f && rawData.ultra2_cm > 0.0f);
		if (output.ultrasonic_ok) {
			output.roll_center_offset_cm = rawData.ultra1_cm - rawData.ultra2_cm;
			output.roll_centered = fabsf(output.roll_center_offset_cm) < ROLL_CENTER_TOLERANCE_CM;
		}

		output.bags_counted = bagCounter;
		output.bag_detected = rawData.ldr_beam_blocked;
		if (rawData.ldr_block_duration_us > 0 && output.motor1_running) {
			float blockTime_s = rawData.ldr_block_duration_us / 1000000.0f;
			float beltSpeed   = fabsf(output.rpm1) * 0.5f;
			output.bag_length_cm = blockTime_s * beltSpeed;
		}

		filteredTcAdc = (0.1f * rawData.tc_adc) + (0.9f * filteredTcAdc);
		float tc_mv = ADC_TO_MV(filteredTcAdc);
		output.tc_connected = (rawData.tc_cont > 2000);
		if (output.tc_connected) {
			output.seal_temp = (tc_mv / MV_PER_DEGC) + getThermoOffset();
			output.thermocouple_ok = true;
		}

		output.vibration_rms_g 	= vibData.rms_amplitude;
		output.vibration_peak_g = vibData.peak_amplitude;
		output.vibration_freq_hz = vibData.dominant_freq;
		output.vibration_freq_mag = vibData.freq_magnitude;

		xQueueOverwrite(queueProcessedToSPI, &output);
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}
