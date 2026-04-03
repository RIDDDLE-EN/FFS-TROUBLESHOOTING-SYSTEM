#ifndef NETWORK_H
#define NETWORK_H

#include <WiFiManager.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <MyLogin.h>
#include "Config.h"

class Network {
public:
    Network(Config& config);
    void setupWiFi(bool configMode);
    void reconnectMQTT();
    bool isMQTTConnected() { return mqttClient_.connected(); }
    void loopMQTT() { mqttClient_.loop(); }
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);
    PubSubClient& getMQTTClient() { return mqttClient_; }

private:
    static void saveConfigCallback();
    static void callback(char* topic, byte* payload, unsigned int length);

    Config& config_;
    WiFiManager wm_;
    WiFiClient espClient_;
    PubSubClient mqttClient_;
    static bool shouldSaveConfig_;
};

#endif // NETWORK_H