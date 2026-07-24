#include <Arduino.h>

#include "sensors.h"
#include "spi_comms.h"
#include "logger.h"

void  setup() {
	Serial.begin(115200);

	loggerInit();
	sensorsInit();
	spiCommsInit();

	xTaskCreatePinnedToCore(sensorReadTask, "SensorRead", 4096, nullptr, 5, nullptr, 1);
	xTaskCreatePinnedToCore(vibrationAnalysisTask, "VibrationFFT", 8192, nullptr, 4, nullptr, 1);
	xTaskCreatePinnedToCore(dataProcessingTask, "DataProcess", 4096, nullptr, 3, nullptr, 1);
	xTaskCreatePinnedToCore(spiCommTask, "SPIComm", 4096, nullptr, 6, nullptr, 0);

	LOG("[MAIN] All tasks started - free heap: %u bytes", (unsigned)esp_get_free_heap_size());
}

void loop() {
	vTaskDelay(pdMS_TO_TICKS(5000));
}
