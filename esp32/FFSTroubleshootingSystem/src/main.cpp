#include <Arduino.h>
#include "Config.h"
#include "Network.h"
#include "Sensors.h"

#define BUTTON_PIN 13
#define LED_PIN 4

Config config;
Network network(config);
Sensors sensors;

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);

    sensors.begin();

    bool configMode = (digitalRead(BUTTON_PIN) == LOW);
    network.setupWiFi(configMode);

    if (configMode && config.shouldSaveConfig()) {
        ESP.restart();
    }
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        network.setupWiFi(false);
    }
    if (!network.isMQTTConnected()) {
        network.reconnectMQTT();
    }
    network.loopMQTT();

    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 5000) {
        lastMsg = millis();
        SensorData data = sensors.readAll();

        sensors.sendJsonMessage(network.getMQTTClient(), "Ultrasonic Sensor1", "distance1", data.distance1, "esp32/distance1");
        sensors.sendJsonMessage(network.getMQTTClient(), "Ultrasonic Sensor2", "distance2", data.distance2, "esp32/distance2");
        sensors.sendJsonMessage(network.getMQTTClient(), "ACS712 Sensor", "voltage", data.voltage, "esp32/motor/voltage");
        sensors.sendJsonMessage(network.getMQTTClient(), "ACS712 Sensor", "VRMS", data.vrms, "esp32/motor/vrms");
        sensors.sendJsonMessage(network.getMQTTClient(), "ACS712 Sensor", "AmpsRMS", data.ampsRMS, "esp32/motor/ampRms");
        sensors.sendJsonMessage(network.getMQTTClient(), "ACS712 Sensor", "Wattage", data.watt, "esp32/motor/watt");
        sensors.sendJsonMessage(network.getMQTTClient(), "LDR", "blockTime", data.blockTimeSec, "esp32/ldr");

        if (!isnan(data.temperature)) {
            sensors.sendJsonMessage(network.getMQTTClient(), "DHT11 Sensor", "temperature", data.temperature, "esp32/temperature");
            sensors.sendJsonMessage(network.getMQTTClient(), "DHT11 Sensor", "humidity", data.humidity, "esp32/humidity");
        }
    }
} 
