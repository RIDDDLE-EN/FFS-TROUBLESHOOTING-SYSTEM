#include "bag.h"
#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t bagMutex = nullptr;
static uint8_t s_ldrPin = LDR_PIN;

void BagModule::init(uint8_t laserPin, uint8_t ldrPin) {
	s_ldrPin = ldrPin;
	if (bagMutex == nullptr) {
		bagMutex = xSemaphoreCreateMutex();
	}
	pinMode(laserPin, OUTPUT);
	digitalWrite(laserPin, HIGH);
	pinMode(s_ldrPin, INPUT);
}

void BagModule::processBag(BagData &output, float beltSpeed) {
	int ldr_raw = analogRead(s_ldrPin);

	if (!beamBlocked && ldr_raw < LDR_BLOCKED_THRESHOLD) {
		beamBlocked = true;
		blockStartMicros = micros();
	} else if (beamBlocked && ldr_raw > LDR_UNBLOCKED_THRESHOLD) {
		beamBlocked = false;
		lastBlockDurationMicros = micros() - blockStartMicros;

		float blockTime_s = lastBlockDurationMicros / 1000000.0f;
		
		if (xSemaphoreTake(bagMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
			output.baglength = blockTime_s * beltSpeed;
			output.bagsProduced++;
			xSemaphoreGive(bagMutex);
		}

		LOG("[BAG] Bag produced! Length : %.2f cm | Total BAgs %d", 
			output.baglength, output.bagsProduced);
	}
}

void BagModule::resetBagCounter(BagData &output) {
	if (xSemaphoreTake(bagMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
		output.bagsProduced = 0;
		output.baglength = 0.0f;
		xSemaphoreGive(bagMutex);
		LOG("[BAG] Bag counter reset to 0");
	}
}

