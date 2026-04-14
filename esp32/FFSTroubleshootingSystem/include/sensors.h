#pragma once
#include <Arduino.h>

//
// sensors.h - All sensors types and init/read declarations in one place
//
//  Usage: 
//  	#include "sensors.h"
//  	sensorsInit();			//call in setup
//  	SensorData d = sensorsReadAll();//call in loop
//

//Per-sensor reading structs

struct DhtReading {
	float temp;
	float hum;
	bool valid;
};

struct CurrentReading {
	float voltage;
	float vrms;
	float ampsRms;
	int watt;
};

struct LdrReading {
	int	rawValue;
	float	blockTimeSec;
	bool 	beamJustUnblocked;
};

struct UltrasonicReading {
	float dist1;
	float dist2;
};

struct LoadCellReading {
	float weightKg;
	bool valid;
};

struct VibrationReading {
	bool vibrating;
};

// Aggregate of all sensor data

struct SensorData {
	DhtReading			dht;
	CurrentReading		current1;
	LdrReading			ldr;
	UltrasonicReading	ultrasonic;
	LoadCellReading		loadCell;
	VibrationReading	vibration;
};

// Lifecycle

// Initialise all sensor harware. Call once in setup().
void sensorsInit();

//Read every sensor and return an aggregate snapshot.
SensorData sensorsReadAll();

// Load Cell - Calibration API
// Calibration is handled automatically by sensorsInit() on first boot.
// call this explicitly only when you want to force a recalibration.

// Tare, wait for the user to place the known weight, compute and store factor.
// `knownWeightKg` should match LOADCELL_CALIB_KG in config.h.
void loadCellCalibrate(float knownWeightKg);

// Returns true if a calibration factor exists in NVS.
bool loadCellIsCalibrated();
