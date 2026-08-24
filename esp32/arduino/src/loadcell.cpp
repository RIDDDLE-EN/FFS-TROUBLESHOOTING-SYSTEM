#include "loadcell.h"
#include "logger.h"
#include <HX711.h>
#include <Preferences.h>
#include <algorithm>

static HX711 scale;
static Preferences prefs;
const float EMA_ALPHA = 0.15f;

void LoadCellModule::init() {
	scale.begin(dtPin, sckPin);
	if (prefs.begin("cal", true)) {
		loadCellFactor = prefs.getFloat("lc_factor", 0.0f);
		prefs.end();
	}
	if (loadCellFactor != 0.0f) {
		scale.set_scale(loadCellFactor);
		scale.tare(10);
		LOG("[LOADCELL] Cal factor loaded: %.4f", loadCellFactor);
	} else {
		LOG("[LOADCELL] WARNING: No calibration factor found.");
	}
}

void LoadCellModule::setFactor(float factor) {
	loadCellFactor = factor;
	if (prefs.begin("cal", false)) {
		prefs.putFloat("lc_factor", factor);
		prefs.end();
	}
	scale.set_scale(factor);
	LOG("[CAL] Load cell factor set: %.4f", factor);
}

float LoadCellModule::getFactor() const {
	return loadCellFactor;
}

void LoadCellModule::tare(uint8_t times) {
	scale.tare(times);
	LOG("[LOADCELL] Tared");
}

int32_t LoadCellModule::readRawAverage(uint8_t samples) {
	long sum = 0;
	int count = 0;
	for (uint8_t i = 0; i < samples; i++) {
		if (scale.is_ready()) {
			sum += scale.read();
			count++;
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	int32_t result = count > 0 ? (int32_t)(sum / count) : 0;
	LOG("[CAL] Raw weight average (%d samples): %ld", cound, (long)result);
	return result;
}


float LoadCellModule::updateAndGetFiltered() {
	if (!scale.is_ready() || loadCellFactor == 0.0f) return emaWeight;

	rawHistory[historyIdx] = scale.read();
	historyIdx = (historyIdx + 1) % MEDIAN_WINDOW;

	int32_t sorted[MEDIAN_WINDOW];
	std::copy(std::begin(rawHistory), std::end(rawHistory), std::begin(sorted));
	std::sort(std::begin(sorted), std::end(sorted));
	int32_t medianRaw = sorted[MEDIAN_WINDOW / 2];

	float currentWeight = (float)medianRaw / loadCellFactor;

	emaWeight = (EMA_ALPHA * currentWeight) + ((1.0f - EMA_ALPHA) * emaWeight);
	return emaWeight;
}
