#include "sensors.h"
#include "config.h"

#include <DHT.h>
#include <HX711.h>
#include <Ultrasonic.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <Preferences.h>

//
// Driver instances
//

static DHT dht(DHT_PIN, DHT_11);
static HX711 scale;
static Ultrasonic ultra1(ULTRA1_TRIG, ULTRA1_ECHO);
static Ultrasonic ultra2(ULTRA2_TRIG, ULTRA2_ECHO);
static Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
static Preferences prefs;

// 
// Encoder state
//

struct EncoderState {
	volatile int32_t pulseCount;
	volatile uint32_t lastPulseMicros;
};

static EncoderState enc1 = {0, 0, 0};
static EncoderState enc2 = {0, 0, 0};

void IRAM_ATTR encoder1_ISR() {
	enc1.pulseCount++;
	enc1.lastPulseTime = micors();
}

void IRAM_ATTR encoder2_ISR() {
	enc2.pulseCount++;
	enc2.lastPulseTime = micros();
}

// 
// LDR Beam Break State
//

static bool beamBlocked = false;
static uint32_t blockStartMicors = 0;
static uint32_t lastBlockDuration = 0;

// 
// Calibration Storage (NVS)
//

static float loadCellFactor = 0.0f;
static float thermoOffset = 0.0f;

void setLoadCellFactor(float factor) {
	loadCellFactor = factor;
	if (!prefs.begin("cal", false)) return;
	prefs.putFloat("lc_factor", factor);
	prefs.end();
	scale.set_scale(factor);
}

float getLoadCellFactor() {
	return loadCellFactor;
}

void setThermoOffset(float offset) {
	thermoOffset = offset;
	if (!prefs.begin("cal", false)) return;
	prefs.putFloat("tc_offset", offset);
	prefs.end();
}

float getThermoOffset() {
	return thermoOffset;
}

// 
// Initialization
//

void sensorsInit() {
	Serial.println("[Sensors] Init (RAW mode)");

	pinMode(LASER_PIN, OUTPUT);
	digitalWrite(LASER_PIN, HIGH);

	pinMode(ENCODER1_A, INPUT_PULLUP);
	pinMode(ENCODER2_A, INPUT_PULLUP);

	analogReadResolution(12);
	analogSetAttenuation(ADC_11db);

	// Encoder interrupts
	attachInterrupt(digitalPinToInterrupt(ENCODER1_A), encoder1_ISR, RISING);
	attachInterrupt(digitalPinToInterrupt(ENCODER2_A), encoder2_ISR, RISING);

	// I2C
	Wire.begin(I2C_SDA, I2C_SCL);

	dht.begin();

	scale.begin(HX_DT, HX_SCK);

	// Load Calibration From NVS
	if (prefs.begin("cal", true)) {
		loadCellFactor = prefs.getFloat("lc_factor", 0.0f);
		thermoOffset   = prefs.getFloat("tc_factor", 0.0f);
		prefs.end();
	}

	if (loadCellFactor != 0.0f) {
		scale.set_scale(loadCellFactor);
		scale.tare(10);
		Serial.printf("[LoadCell] Factor loaded: %.2f\n", loadCellFactor);
	}

	// ADXL345
	if (accel.begin()) {
		accel.setRange(ADXL345_RANGE_16_G);
		accel.setDataRate(ADXL345_DATARATE_1600_HZ);
		Serial.println("[ADXL345] OK");
	}

	Serial.prinltn("[Sensors] Ready");
}

// 
// Read All Sensors (RAW values only)
//

RawSensorData sensorsRead() {
	RawSensorData data;

	data.timestamp = millis();

	// DHT11
	data.dht_temp = dht.readTemperature();
	data.dht_hum  = dht.readHumidity();
	data.dht_valid = !isnan(data.dht_temp) && !isnan(data.dht_hum);

	// Motor 1
	data.adc_current1 	= analogRead(ACS712_1);
	data.encoder1_pulses	= enc1.pulses;
	data.encoder1_last_us	= enc1.lastPulseMicros;

	// Load cell
	if (scale.is_ready()) {
		data.hx711_raw = scale.read();
	} else {
		data.hx711_raw = 0;
	}
	data.hx711_cal_factor = loadCellFactor;

	// Ultrasonic
	data.ultra1_cm = ultra1.read();
	data.ultra2_cm = ultra2.read();

	// LDR beam blocked
	data.ldr_raw = analogRead(LDR_PIN);

	// Simple threshold detection for beam break timing
	if (!beamBlocked && data.ldr_raw < 2000) {
		beamBlocked = true;
		blockStartMicros = micros();
		data.ldr_block_start_us = blockStartMicros;
	} else if (beamBlocked && data.ldr_raw > 2100) {
		beamBlocked = false;
		lastBlockDuration = micros() - blockStartMicros;
		data.ldr_block_start_us = 0;
	} else {
		data.ldr_block_start_us = beamBlocked ? blockStartMicros : 0;
	}
	data.ldr_last_block_us = lastBlockDuration;

	// Thermocouple
	data.tc_adc		= analogRead(TC_PLUS);
	data.tc_cont_adc	= analogRead(TC_CONTINUITY);
	data.tc_offset		= thermoOffset;

	// Accelerometer
	sensors_event_t	event;
	if (accel.getEvent(&event)) {
		data.accel_x = event.accelaration.x;
		data.accel_y = event.accelaration.y;
		data.accel_z = event.accelaration.z;
	} else {
		data.accel_x = data.accel_y = data.accel_z = 0.0f;
	}

	return data;
}
