#pragma once
#include <Arduino.h>

void sensorsInit();
void resetBagCounter();

void sensorReadTask(void *pvParameters);
void vibrationAnalysisTask(void *pvParameters);
void dataProcessingTask(void *pvParameters);
