#include "loadcell.h"
#include "logger.h"
#include <algorithm>

static HX711 scale;
static Preferences prefs;
const float EMA_ALPHA = 0.15f;
float lastWeight = 0.0f;

void LoadCellModule::init(HX711 &scaleInstace, CalibrationModule &calibration) {
	scale = &scaleInstance;
	loadCellFactor = calibration.getLoadCellFactor();

	if (loadCellFactor != 0.0f) {
		scale->set_scale(loadCellFactor);
		LOG("[LOADCELL] Cal factor loaded directly: %.4f", loadCellFactor);
	} else {
		LOG("[LOADCELL] WARNING: No calibration factor found.");
	}
}

void LoadCellModule::setFactor(float factor) {
	loadCellFacotr = factor;
	if (loadCellFactor != 0.0f) {
		scale->set_scale(loadCellFactor);
	}
}

void LoadCellModule::updateAndGetFiltered(LoadCellData &output) {
	if (!scale.is_ready() || loadCellFactor == 0.0f) return emaWeight;

	rawHistory[historyIdx] = scale->read();
	historyIdx = (historyIdx + 1) % MEDIAN_WINDOW;

	int32_t sorted[MEDIAN_WINDOW];
	std::copy(std::begin(rawHistory), std::end(rawHistory), std::begin(sorted));
	std::sort(std::begin(sorted), std::end(sorted));
	int32_t medianRaw = sorted[MEDIAN_WINDOW / 2];

	float currentWeight = (float)medianRaw / loadCellFactor;

	emaWeight = (EMA_ALPHA * currentWeight) + ((1.0f - EMA_ALPHA) * emaWeight);
	output.weight_grams = emaWeight;
	output.loadcell_ok = emaWeight > 0 ? true : false;
	output.weight_stable = (fabsf(output.weight_grams - lastWeight) > WEIGHT_STABLE_BAND_G);
	lastWeight = output.weight_grams;
}
