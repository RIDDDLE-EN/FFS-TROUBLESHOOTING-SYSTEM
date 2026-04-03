#include <WiFiManager.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <MyLogin.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <DHT.h>
#include <Ultrasonic.h>

#define DHT_PIN       4
#define SONAR_NUM     2
#define MAX_DISTANCE  200
#define button        13
#define ledPin        5
#define sensorIn1     32
#define sensorIn2     33
#define ldrPin        35
#define hxDT          18
#define hxSCK         19
#define vibration     34

WiFiManager wm;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht11(DHT_PIN, DHT11);
Ultrasonic sonar[SONAR_NUM] = {
  Ultrasonic(25, 26),
  Ultrasonic(27, 14)
};

int ldrValue = 0;
int mVperAmp = 185;
int watt = 0;
double voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;

int threshold = 2000;
const int hysteresis = 100;
unsigned long blockStartMicros = 0;

long lastMsg = 0;
unsigned long duration = 0;
float distance1, distance2, temp, hum, blockTimeSec;

bool shouldSaveConfig = false;
bool beamBlocked = false;

char ssid[32];
char pass[32];
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* dist1Topic = "esp32/distance1";
const char* dist2Topic = "esp32/distance2";
const char* volTopic = "esp32/motor/voltage";
const char* vrmsTopic = "esp32/motor/vrms";
const char* ampRmsTopic = "esp32/motor/ampRms";
const char* wattTopic = "esp32/motor/watt";
const char* tempTopic = "esp32/temperature";
const char* humTopic = "esp32/humidity";
const char* ldrTopic = "esp32/ldr";
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

float getVPP(){
  float result;
  int readValue;
  int maxValue = 0;
  int minValue = 4096;

  uint32_t start_time = millis();
  while ((millis()-start_time) < 1000){
    readValue = analogRead(sensorIn);

    if (readVAlue > maxValue){
      maxValue = readValue;
    }
    if (readValue < minValue) {
      minValue = readValue;
    }
  }

  result = ((maxValue - minValue) * 3.3)/4096.0;

  return result;
}

void readLDR(){
   ldrValue = analogRead(ldrPin);
   Serial.print("LDR: ");
   Serial.println(ldrValue);

   if (!beamBlocked && ldrValue < threshold){
     beamBlocked = true;
     blockStartMicros = micros();
     Serial.println("Measuring the bag length);
   }
   if (beamBlocked && ldrValue > threshold + hysteresis){
     beamBlocked = false;
     unsigned long blockTime = micros() - blockStartMicros;
     blockTimeSec = blockTime / 1000000.0;
     Serial.print("block time: ");
     Serial.print(blockTimeSec);
     Serial.println("s");
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

  analogReadResolution(12);
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

    voltage = getVPP();
    VRMS = (Voltage/20)*0.707;
    AmpsRMS = ((VRMS*1000)/mVperAmp) - 0.3;
    watt = (AmpsRMS*240/1.2);
    sendJsonMessage("ACS712 Sensor", "voltage", voltage, volTopic);
    sendJsonMessage("ACS712 Sensor", "VRMS", VRMS, vrmsTopic);
    sendJsonMessage("ACS712 Sensor", "AmpsRMS", AmpsRMS, ampRmsTopic);
    sendJsonMessage("ACS721 Sensor", "Wattage", watt, wattTopic);

    readLDR();
    sendJsonMessage("LDR", "blockTime", blockTimeSec, ldrTopic);

//    measureHumTemp();
//    sendJsonMessage("DHT11 Sensor:", "temperature", temp, tempTopic);
//    sendJsonMessage("DHT11 Sensor:", "humidity", hum, humTopic);
  }
} 
