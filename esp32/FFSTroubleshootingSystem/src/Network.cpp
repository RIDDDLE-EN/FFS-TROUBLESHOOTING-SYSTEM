#include "Network.h"
#include <Arduino.h>

bool Network::shouldSaveConfig_ = false;

Network::Network(Config& config) : config_(config), mqttClient_(espClient_) {
    mqttClient_.setServer("broker.hivemq.com", 1883);
    mqttClient_.setCallback(callback);
    wm_.setSaveConfigCallback(saveConfigCallback);
}

void Network::saveConfigCallback() {
    shouldSaveConfig_ = true;
}

void Network::callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");

    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);

    if (String(topic) == "esp32/test") {
        if (message == "on") {
            digitalWrite(4, HIGH);
        } else if (message == "off") {
            digitalWrite(4, LOW);
        }
    }
}

void Network::setupWiFi(bool configMode) {
    if (configMode) {
        wm_.startConfigPortal("ESP32_CONFIG");
        if (shouldSaveConfig_) {
            config_.saveWiFiCredentials(wm_.getWiFiSSID().c_str(), wm_.getWiFiPass().c_str());
            config_.setShouldSaveConfig(true);
            shouldSaveConfig_ = false;
        }
    } else {
        WiFi.begin(config_.getSSID(), config_.getPass());
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
        }
        Serial.println("WiFi connected");
    }
}

void Network::reconnectMQTT() {
    if (mqttClient_.connected()) return;
    while (!mqttClient_.connected()) {
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        if (mqttClient_.connect(clientId.c_str(), CLIENTID, CLIENTPSK)) {
            Serial.println("MQTT connected");
            subscribe("esp32/test");
        } else {
            delay(2000);
        }
    }
}

void Network::publish(const char* topic, const char* payload) {
    mqttClient_.publish(topic, payload);
}

void Network::subscribe(const char* topic) {
    mqttClient_.subscribe(topic);
}