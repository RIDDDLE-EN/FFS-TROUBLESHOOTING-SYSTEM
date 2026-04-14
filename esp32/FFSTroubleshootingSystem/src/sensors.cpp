#include "sensors.h"
#include "config.h"

#include <DHT.h>
#include <Ultrasonic.h>
#include <HX711.h>
#include <Preferences.h>

//
// sensors.cpp - All sensor driver in one translation unit.
//
// Sections:
// 	1. Static driver instances
// 	2. DHT11
// 	3. ACS712 current sensor
// 	4. LDR beam-break
// 	5. Ultrasonic (HC-SR04 x2)
// 	6. HX711 load cell <- includes NVS calibration
// 	7. SW-420 vibration
// 	8. Public API (sensorsInit / sensorsReadAll)
//

// 1. Static Driver Instances

static DHT			dht(DHT_PIN, DHT11);
static Ultrasonic	sonar[SONAR_NUM] = {
	Ultrasonic(SONAR1_TRIG, SONAR1_ECHO),
	Ultrasonic(SONAR2_TRIG, SONAR2_ECHO),
};
static HX711		scale;
static Preferences 	prefs;

// 2. DHT11

static DhtReading dhtRead() {
	DhtReading r;
	r.hum 	= dht.readHumidity();
	r.temp	= dht.readTemperature();
	r.valid	= !isnan(r.temp) && !isnan(r.hum);
	if (!r.valid)
		Serial.println("[DHT] Read failed - check wiring.");
	else
		Serial.printf("[DHT] %.1fC %.1f%%\n", r.temp, r.hum);
	return r;
}

// 3. ACS712 Current Sensor

static constexpr int	MV_PER_AMP	= 185;
static constexpr float	MAINS_VOLTAGE 	= 240.0f;
static constexpr float	POWER_FACTOR	= 1.2f;

static CurrentReading currentRead(uint8_t pin) {
	int maxVal = 0, minVal = 4096, sample;
	uint32_t start = millis();
	while (millis() - start < 1000) {
		sample = analogRead(pin);
		if (sample > maxVal) maxVal = sample;
		if (sample < minVal) minVal = sample;
	}
	CurrentReading r;
	r.voltage = ((maxVal - minVal) * 3.3f) / 4096.0f;
	r.vrms 	  = (r.voltage / 2.0f) * 0.707f;
	r.ampsRms = ((r.vrms * 1000.0f) / MV_PER_AMP) - 0.3f;
	r.watt 	  = static_cast<int>(r.ampsRms * MAINS_VOLTAGE / POWER_FACTOR);
	Serial.printf("[ACS712] pin=%d V=%.3f Vrms=%.3f A=%.3f W=%d\n", 
			pin, r.voltage, r.vrms, r.ampsRms, r.watt);
	return r;
}

// 4. LDR Beam-Break

static bool 		beamBlocked		= false;
static unsigned long	blockStartMicros	= 0;
static float		lastBlockTimeSec	= 0.0f;

static LdrReading ldrRead() {
	LdrReading r;
	r.rawValue		= analogRead(LDR_PIN);
	r.beamJustUnblocked	= false;

	if (!beamBlocked && r.rawValue < LDR_THRESHOLD) {
		beamBlocked 		= true;
		blockStartMicros	= micros();
	} else if (beamBlocked && r.rawValue > (LDR_THRESHOLD + LDR_HYSTERESIS)) {
		beamBlocked 		= false;
		lastBlockTimeSec 	= (micros() - blockStartMicros) / 1000000.0f;
		r.beamJustUnblocked 	= true;
		Serial.printf("[LDR] Beam unblocked after %.4f s\n", lastBlockTimeSec);
	}
	r.blockTimeSec = lastBlockTimeSec;
	return r;
}

// 5. Ultrasonic

static UltrasonicReading ultrasonicRead() {
	return { sonar[0].read(), sonar[1].read() };
}

// 6. HX711 Load Cell
//
// Calibration flow
//
// First boot (no factor in NVS);
// 	1. Tare the scale (no load).
// 	2. Prrompt the user via Serial to place know weight (LOADCELL_CALIB_KG).
// 	3. Read raw average, derive factor = raw / knownWeight.
// 	4. Store factor in NVS.
//
// Subsequent boots:
// 	1. Load factor from NVS.
// 	2. Apply to scale - ready to weigh immediately.
//
// Recalibration: 
// 	Call loadCellCalibrate() explicitly (e.g. triggered by long button press).

static constexpr const char* NVS_LC_NS		= "loadcell";
static constexpr const char* NVS_LC_FACTOR	= "calFactor";

static float calFactor = 0.0f;

//
bool loadCellIsCalibrated() { return calFactor != 0.0f; }

//
static void saveCalFactor(float factor) {
	if (!prefs.begin(NVS_LC_NS, false)) {
		Serial.println("[HX711] ERROR: Could not open NVS for writing.");
		return;
	}
	prefs.putFloat(NVS_LC_FACTOR, factor);
	prefs.end();
	Serial.printf("[HX711] Calibration factor %.4f saved to NVS.\n", factor);
}

//
static bool loadCalFactor() {
	if (!prefs.begin(NVS_LC_NS, true)) return false;
	calFactor = prefs.getFloat(NVS_LC_FACTOR, 0.0f);
	prefs.end();
	return calFactor != 0.0f;
}

//
void loadCellCalibrate(float knownWeightKg) {
	Serial.println("\n[HX711] ------Calibration Mode --------------------------");
	Serial.println("[HX711] Remove ALL weight from the scael. Taring in 3s...");
	delay(3000);

	scale.tare(20);	// average 20 reading for a stable tare
	Serial.println("[HX711] Tare complete.");
	Serial.printf("[HX711] Place exactly %.2f kg on the scale, then send any key over Seria.\n",
			knownWeightKg);
	
	while (!Serial.available()) delay(100);
	Serial.read();

	long rawAvg = scale.get_value(LOADCELL_SAMPLES);
	if (rawAvg == 0) {
		Serial.println("[HX711] ERROR: Raw reading is 0 - check wiring.");
		return;
	}

	calFactor = static_cast<float>(rawAvg) / knownWeightKg;
	scale.set_scale(calFactor);

	saveCalFactor(calFactor);

	Serial.printf("[HX711] Calibration done. Factor = %.4f\n", calFactor);
	Serial.println("[HX711] ----------------------------------------------------\n");
}

//
static void loadCellInit() {
	scale.begin(HX_DT, HX_SCK);
	scale.set_gain(128);	// channel A, gain 128 (default, appropriate for most load cells)

	if (loadCalFactor()) {
		scale.set_scale(calFactor);
		scale.tare(10);
		Serial.printf("[HX711] Loaded calibration factor %.4f from NVS.\n", calFactor);
	} else {
		Serial.println("[HX711] No calibration found - starting calibration...");
		loadCellCalibrate(LOADCELL_CALIB_KG);
	}
}

//
static LoadCellReading loadCellRead() {
	if (!scale.is_ready()) {
		Serial.println("[HX711] Not ready.");
		return { 0.0f, false };
	}

	float weight = scale.get_units(LOADCELL_SAMPLES);

	// Negative cree (noise below zero) -> clamp to 0
	if (weight < 0.0f) weight = 0.0f;

	// Sanity-check against rated capacity
	bool valid = (weight <= LOADCELL_MAX_KG);
	if (!valid)
		Serial.printf("[HX711] WARNING: Reading %.3f kg exceeds max %.1f kg.\n",
				weight, LOADCELL_MAX_KG);
	else 
		Serial.printf("[HX711] %.4f kg\n", weight);

	return { weight, valid };
}

// 7. SW-420 vibration

static VibrationReading vibrationRead() {
	bool v = digitalRead(SW_420);
	if (v) Serial.println("[SW-420] Vibration detected.");
	return { v };
}

// 8. Public API

void sensorsInit() {
	dht.begin();
	loadCellInit();
}

SensorData sensorsReadAll() {
	SensorData d;
	d.dht		= dhtRead();
	d.current1	= currentRead(ACS712_1);
	d.ldr		= ldrRead();
	d.ultrasonic	= ultrasonicRead();
	d.loadCell	= loadCellRead();
	d.vibration	= vibrationRead();
	return d;
}
