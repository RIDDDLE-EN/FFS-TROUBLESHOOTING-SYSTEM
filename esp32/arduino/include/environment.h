#pragma once

struct EnvironmentData {
	float temperature;
	float humidity;
	bool valid;
};

class EnvironmentModule {
	public:
		void init();
		EnvironmentData read();
};
