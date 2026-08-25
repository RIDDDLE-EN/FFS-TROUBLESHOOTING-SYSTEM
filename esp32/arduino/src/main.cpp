#include <Arduino.h>
#include "sensors.h"
#include "spi_comms.h"
#include "logger.h"

void setup() {
	Serial.begin(115200);

	loggerInit();
	sensorsInit();
	spiCommsInit();

	xTaskCreatePinnedToCore(spiCommTask, "SPIComm", 4096, nullptr, 4, nullptr, 0);
	
	LOG("[MAIN] All tasks started - free heap: %u bytes", (unsigned)esp_get_free_heap_size());
}

void loop() {
	vTaskDelete(NULL);
}
