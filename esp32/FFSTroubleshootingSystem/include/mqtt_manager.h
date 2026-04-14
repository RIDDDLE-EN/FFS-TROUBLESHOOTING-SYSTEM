#pragma once
#include <Arduino.h>
#include "credentials.h"

//Call once in setup() to configure server, callback, and make first connection
void mqttInit(const Creds &c);

//Call every loop() iteration - handles WiFi/MQTT reconnects and client polling.
void mqttLoop(const Creds &c);

//Serialize {sensor_id, label: value} as JSON and publish to topic.
void mqttPublishJson(const char* sensorId, const char* label, float value, const char* topic);
