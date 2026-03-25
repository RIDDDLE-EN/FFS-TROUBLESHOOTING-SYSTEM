#include <WiFiManager.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <MyLogin.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <DHT.h>
#include <Ultrasonic.h>

#define DHT_PIN 21
#define SONAR_NUM 2
#define MAX_DISTANCE 200

WiFiManager wm;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht11(DHT_PIN, DHT11);
Ultrasonic sonar[SONAR_NUM] = {
  Ultrasonic(5, 18),
  Ultrasonic(19, 21)
};

int button = 13;
int ledPin = 4;

long lastMsg = 0;
unsigned long duration = 0;
float distance1, distance2, temp, hum;

bool shouldSaveConfig = false;

char ssid[32];
char pass[32];
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* dist1Topic = "esp32/distance1";
const char* dist2Topic = "esp32/distance2";
const char* tempTopic = "esp32/temperature";
const char* humTopic = "esp32/humidity";
const char* subTopic = "esp32/test";


void sendJsonMessage(char* sensorId, const char* label, float value, const char* pubTopic) {
  StaticJsonDocument<200> doc;
  doc["sensor_id"] = sensorId;
  doc[label] = value;
  char buffer[256];
  serializeJson(doc, buffer);

  mqttClient.publish(pubTopic, buffer);
  Serial.print("Published to ");
  Serial.print(pubTopic);
  Serial.print(": ");
  Serial.println(buffer);

  delay(5000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message");

  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == subTopic) {
    Serial.print("Changing output to: ");
    if (message == "on") {
      Serial.print("led on");
      digitalWrite(ledPin, HIGH);
    } else if (message == "off") {
      Serial.print("led Off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveCredentials(const char* newSSID, const char* newPass) {
  Serial.println("Saving WiFi Credentials to EEPROM...");

  for (int i = 0; i < 32; i ++) {
    EEPROM.write(0 + i, newSSID[i]);
  }

  for (int i = 0; i < 32; i ++) {
    EEPROM.write(100 + i, newPass[i]);
  }

  EEPROM.commit();
}

void readCredentials() {
  for (int i = 0; i < 32; i ++) {
    ssid[i] = EEPROM.read(0 + i);
  }
  ssid[31] = '\0';

  for (int i = 0; i < 32; i ++) {
    pass[i] = EEPROM.read(100 + i);
  }
  pass[31] = '\0';

  Serial.println("SSID ");
  Serial.println(ssid);
  Serial.println("Password ");
  Serial.println(pass);

  delay(5000);
}

void setup_wifi() {
  readCredentials();
  Serial.println("connecting ...");
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
}

void espTest() {
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);
}


void measureHumTemp(){
  hum = dht11.readHumidity();
  temp = dht11.readTemperature();

  if (isnan(temp) || isnan(hum)){
    Serial.println("Failed to read from DHT11 sensor");
  } else {
    Serial.println("Humidity: ");
    Serial.print(hum);
    Serial.print("%");

    Serial.print(" | ");

    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.print("C ");
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  dht11.begin();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  pinMode(button, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  wm.setSaveConfigCallback(saveConfigCallback);

  if (digitalRead(button) == LOW) {
    Serial.println("Button pressed, starting WiFiManager...");
    wm.startConfigPortal("ESP32_CONFIG");

    if (shouldSaveConfig) {
      saveCredentials(wm.getWiFiSSID().c_str(), wm.getWiFiPass().c_str());
      Serial.println("Credentials saved. ");
      ESP.restart();
    }
  } else {
    setup_wifi();
  }
  espTest();
}

void reconnect() {
  if (mqttClient.connected()) return;
  Serial.print("MQTT state: ");
  Serial.println(mqttClient.state());
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), CLIENTID, CLIENTPSK)) {
      Serial.println("Connected. ");
      mqttClient.subscribe(subTopic);
    }
    delay(2000);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) setup_wifi();
  if (!mqttClient.connected()) reconnect();
  mqttClient.loop();
  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;
    distance1 = sonar[0].read();
    sendJsonMessage("Ultrasonic Sensor1:", "distance1", distance1, dist1Topic);
    distance2 = sonar[1].read();
    sendJsonMessage("Ultrasonic Sensor2:", "distance2", distance2, dist2Topic);

    measureHumTemp();
    sendJsonMessage("DHT11 Sensor:", "temperature", temp, tempTopic);
    sendJsonMessage("DHT11 Sensor:", "humidity", hum, humTopic);
  }
} 
