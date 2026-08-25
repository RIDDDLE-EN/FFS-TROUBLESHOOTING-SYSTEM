#pragma once
#include <stdint.h>

struct BagData {
	float baglength;
	int32_t bagsProduced;
};

class BagModule {
	public:
		void init(uint8_t laserPin, uint8_t ldrPin);
		void processBag(BagData &output, float beltSpeed);
		void resetBagCounter(BagData &output);

	private:
		bool     beamBlocked 		= false;
		uint32_t blockStartMicros	= 0;
		uint32_t lastBlockDurationMicros = 0;
};
