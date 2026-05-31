#include "logger.h"
#include "config.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t s_logQueue = nullptr;

void loggerInit() {
	s_logQueue = xQueueCreate(LOG_QUEUE_DEPTH, LOG_MAX_MSG_LEN);
}

void logMessage(const char *fmt, ...) {
	char buf[LOG_MAX_MSG_LEN];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	Serial.println(buf);
	
	if (s_logQueue) {
		xQueueSend(s_logQueue, buf, 0);
	}
}

bool logDequeue(char *buf) {
	if (!s_logQueue) return false;
	return xQueueReceive(s_logQueue, buf, 0) == pdTRUE;
}

int logPending() {
	if (!s_logQueue) return 0;
	return (int)uxQueueMessagesWaiting(s_logQueue);
}
