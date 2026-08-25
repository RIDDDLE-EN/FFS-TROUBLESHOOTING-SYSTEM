#include "bag.h"
#include <Arduino.h>
#include "config.h"
#include "logger.h"

void BagModule::init(uint8_t laserPin, uint8_t ldrPin) {
	pinMode(laserPin, OUTPUT);
	digitalWrite(laserPin, HIGH);
}

void BagModule::processBag(BagData &output, float beltSpeed) {
	int ldr_raw = analogRead(LDR_PIN);

	if (!beamBlocked && ldr_raw < LDR_BLOCKED_THRESHOLD) {
		beamBlocked = true;
		blockStartMicros = micros();
	} else if (beamBlocked && ldr_raw > LDR_UNBLOCKED_THRESHOLD) {
		beamBlocked = false;
		lastBlockDurationMicros = micros() - blockStartMicros;

		float blockTime_s = lastBlockDurationMicros / 1000000.0f;
		output.baglength = blockTime_s * beltSpeed;

		output.bagsProduced++;
		LOG("[BAG] Bag produced! Length : %.2f cm | Total BAgs %d", 
			output.baglength, output.bagsProduced);
	}
}

void BagModule::resetBagCounter(BagData &output) {
	output.bagsProduced = 0;
	LOG("[BAG] Bag counter is set to 0");
}

