#include "mqtt_manager.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

//Inbound Message Handler
static void onMessage(char* topic, byte* payload, unsigned int length) {
	String msg;
	msg.reserve(length);
	for (unsigned int i = 0; i < length; i++) msg +=  (char)payload[i];

	Serial.printf("[MQTT] <- %s: %s\n", topic, msg.c_str());

	if (String(topic) == TOPIC_SUB) {
		if	(msg == "on") digitalWrite(LED_PIN, HIGH);
		else if	(msg == "off") digitalWrite(LED_PIN,LOW);
	}
}

//Non-Blocking Reconnect
static void reconnect(const Creds &c) {
	if (mqttClient.connected()) return;

	static unsigned long lastAttempt = 0;
	unsigned long now = millis();
	if (now - lastAttempt < 5000) return;
	lastAttempt = now;

	String clientId = "ESP32-" + String(random(0xFFFF), HEX);
	Serial.printf("[MQTT] Connecting as %s ..\n", clientId.c_str());

	if (mqttClient.connect(clientId.c_str(), c.CLIENT_ID.c_str(), c.CLIENT_PSK.c_str())) {
		Serial.println("[MQTT] Connected.");
		mqttClient.subscribe(TOPIC_SUB);
	} else {
		Serial.printf("[MQTT] Failed (state=%d). Retrying in 5 s.\n", mqttClient.state());
	}
}

//Public API
void mqttInit(const Creds &c) {
	mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
	mqttClient.setCallback(onMessage);
	reconnect(c);
}

void mqttLoop(const Creds &c) {
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("[WiFi] Lost connection. Reconnecting...");
		WiFi.begin(c.SSID.c_str(), c.PASS.c_str());
		return;
	}
	reconnect(c);
	mqttClient.loop();
}

void mqttPublishJson(const char* sensorId, const char* label, float value, const char* topic) {
	StaticJsonDocument<256> doc;
	doc["sensor_id"] = sensorId;
	doc[label] = value;

	char buffer[256];
	serializeJson(doc, buffer);

	mqttClient.publish(topic, buffer);
	Serial.printf("[MQTT] -> %s: %s\n", topic, buffer);
}
