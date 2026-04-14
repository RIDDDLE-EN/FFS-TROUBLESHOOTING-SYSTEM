#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "credentials.h"
#include "mqtt_manager.h"
#include "sensors.h"

static Creds 		creds;
static unsigned long	lastSensorRead = 0;

// Helpers

static void ledBlink(int times = 1) {
	for (int i = 0; i < times; i++) {
		digitalWrite(LED_PIN, HIGH); delay(200);
		digitalWrite(LED_PIN, LOW);  delay(200);
	}
}

static void connectWiFi() {
	Serial.printf("[WiFi] Connecting to %s", creds.SSID.c_str());
	WiFi.begin(creds.SSID.c_str(), creds.PASS.c_str());
	while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
	Serial.printf("\n[WiFi] Connected - IP: %s\n", WiFi.localIP().toString().c_str());
}

// Publish full SensorData snapshot

static void publishAll(const SensorData &d) {
	// Ultrasonic
	mqttPublishJson("Ultrasonic-1", "distance1", d.ultrasonic.dist1, TOPIC_DIST1);
	mqttPublishJson("Ultrasonic-2", "distance2", d.ultrasonic.dist2, TOPIC_DIST2);

	// Current
	mqttPublishJson("ACS712-1", "voltage", d.current1.voltage, TOPIC_VOLTAGE);
	mqttPublishJson("ACS712-1", "vrms", d.current1.vrms, TOPIC_VRMS);
	mqttPublishJson("ACS712-1", "ampsRms", d.current1.ampsRms, TOPIC_AMP_RMS);
	mqttPublishJson("ACS712-1", "watt", static_cast<float>(d.current1.watt), TOPIC_WATT);

	// LDR
	mqttPublishJson("LDR", "blockTime", d.ldr.blockTimeSec, TOPIC_LDR);

	// DHT11
	if (d.dht.valid) {
		mqttPublishJson("DHT11", "temperature", d.dht.temp, TOPIC_TEMP);
		mqttPublishJson("DHT11", "humidity", d.dht.hum, TOPIC_HUM);
	}

	// Load Cell
	if (d.loadCell.valid)
		mqttPublishJson("LoadCell", "weightKg", d.loadCell.weightKg, TOPIC_WEIGHT);

	// Vibration
	mqttPublishJson("SW-420", "vibration", d.vibration.vibrating ? 1.0f : 0.0f, TOPIC_VIB);
}

// Setup

void setup() {
	Serial.begin(115200);
	pinMode(BUTTON, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);
	pinMode(SW_420, INPUT);
	analogReadResolution(12);

	// Credential / portal check
	if (loadCredentials(creds)) {
		printCreds(creds);
	} else {
		Serial.println("[Setup] No credentials - Launching config portal.");
	}

	if (digitalRead(BUTTON) == LOW) {
		runConfigPortal();
	}

	connectWiFi();

	// Sensor init
	sensorsInit();

	// Hold BUTTON for 3s after boot -> force load-cell recalibration.
	// Lets you recalibrate without wiping WiFi credentials.
	Serial.println("[Setup] Hold BUTTON for 3s to recalibrate laod cell...");
	unsigned long held = millis();
	while (digitalRead(BUTTON) == LOW && millis() - held < 3000) delay(50);

	if (millis() - held >= 300 && digitalRead(BUTTON) == LOW) {
		Serial.println("[Setup] Recalibration triggered.");
		loadCellCalibrate(LOADCELL_CALIB_KG);
		ledBlink(3);
	}

	mqttInit(creds);
	ledBlink(1);
	Serial.println("[Setup] Ready.\n");
}

// Loop

void loop() {
	mqttLoop(creds);

	unsigned long now = millis();
	if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
		lastSensorRead = now;
		SensorData data = sensorsReadAll();
		publishAll(data);
	}
}
