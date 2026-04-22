#pragma once
#include <Arduino.h>
#include "sensors.h"

// LED Feedback
// Call anywhere in code to signal state visually.
void ledBlink(int times, int onMs = 200, int offMs = 200);

// Web UI
// Hosts a small web server on port 80 
//
// Pages:
// 	GEt  /		-> live status dashboard (auto-refreshes every 3s)
// 	GET  /calibrate	-> calibration wizard, step 1 (tare)
// 	POST /calibrate/tare	-> triggers tare, advances to step 3
// 	POST /calibrate/confirm	-> user has placed weight, computes and stores factor
// 	GET  /logs	-> last 30 log lines
// 	GET  /api/status	-> JSON snapshot of latest sensor data (for the pi)
//
// All pages are self-contained HTML - no external CSS/JS dependencies.

// Call once in setup() after WiFi is connected.
void webUiInit();

// Call every loop() - handles async events (already non-blocking).
void webUiLoop();

// Push a log line into the rolling in-memory buffer (last 30 lines).
// Also prints to Serial. Replace all Serial.println() calls with this.
void logMsg(const char* tag, const char* msg);
void logMsgf(const char* tag, const char* fmt, ...);

// Update the latest sensor snapshot shown on the dashboard.
void webUiUpdateSensors(const SensorData &d);

// Signal that calibration is waiting for the user to confirm weight placement.
// The web UI will show a "Confirm weight places" button.
void webUiSetCalibrationPending(bool pending);
bool webUiCalibrationConfirmed();	// returns true once user hits Confirm
void webUiClearCalibrationConfirm();
