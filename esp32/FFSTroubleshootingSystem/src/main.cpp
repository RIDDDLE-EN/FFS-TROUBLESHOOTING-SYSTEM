#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "spi_protocol.h"

static unsigned long lastLoop = 0;

void setup() {
	Serial.begin(115200);
	delay(1000);

	Serial.println("ESP32 Raw Sensor Array (SPI Slave)");

	sensorsInit();
	spiInit();

	lastLoop = micros();
	Serial.println("[Ready] Streaming raw data at 100 Hz\n");
}

void loop() {
	unsigned long now = micros();
	if (now - lastLoop < MAIN_LOOP_US) {
		delayMicroseconds(MAIN_LOOP_US - (now - lastLoop));
		now = micros();
	}
	lastLoop = now;

	RawSensorData data = sensorsRead();
	spiUpdatData(data);
	spiSignalReady();
	spiProcessCommands();
}
