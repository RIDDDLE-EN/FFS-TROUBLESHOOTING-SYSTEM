#include <WiFiManager.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <DHT.h>
#include <Ultrasonic.h>

#define DHT_PIN       4
#define SONAR_NUM     2
#define MAX_DISTANCE  200
#define BUTTON        13
#define LED_PIN       5
#define ACS712_1      32
#define ACS712_2      33
#define LDR_PIN       35
#define HX_DT         18
#define HX_SCK        19
#define SW_420        34

Preferences prefs;
WiFiManager wm;
WiFiManagerParameter p_client_id("mid", "MQTT Client ID", "", 32);
const char *mqttPassHtml = "<input id = 'mpw' name'mpw' type='password' maxlength='32' placeholder='MQTT Password' />";
WiFiManagerParameter p_client_psk("mpq", "MQTT Password", "", 32, mqttPassHtml);
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
const char* vibTopic = "esp32/vibration";
const char* subTopic = "esp32/test";

struct Creds { String SSID, PASS, CLIENT_ID, CLIENT_PSK; };

Creds c;


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
      digitalWrite(LED_PIN, HIGH);
    } else if (message == "off") {
      Serial.print("led Off");
      digitalWrite(LED_PIN, LOW);
    }
  }
}

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveCredentials(const Creds &c) {

  if (prefs.begin("Creds", false)) {
    prefs.putString("ssid", c.SSID);
    prefs.putString("password", c.PASS);
    prefs.putString("clientId", c.CLIENT_ID);
    prefs.putString("clientPSK", c.CLIENT_PSK);
    prefs.end();
    Serial.println("Credentials saved to NVS.");
  } else {
    Serial.println("Error: Could not open Preferences.");
  }
}

bool loadCredentials(Creds &c) {
  if (prefs.begin("Creds", true)) {
    c.SSID = prefs.getString("ssid", "");
    c.PASS = prefs.getString("password", "");
    c.CLIENT_ID = prefs.getString("clientID", "");
    c.CLIENT_PSK = prefs.getString("clientPSK", "");
    prefs.end();

    size_t len = c.SSID.length();

    return (len > 0);
  }
  return false;
}

void setup_wifi(const Creds &c) {
  Serial.println("connecting ...");
  WiFi.begin(c.SSID, c.PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
}

void espTest() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
}


void dhtSensor(){
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
  
  sendJsonMessage("DHT11 Sensor:", "temperature", temp, tempTopic);
  sendJsonMessage("DHT11 Sensor:", "humidity", hum, humTopic);
}

void currentSensor(char* sensorNo, uint8_t sensorPin){
  int readValue;
  int maxValue = 0;
  int minValue = 4096;

  uint32_t start_time = millis();
  while ((millis()-start_time) < 1000){
    readValue = analogRead(sensorPin);

    if (readValue > maxValue){
      maxValue = readValue;
    }
    if (readValue < minValue) {
      minValue = readValue;
    }
  }

  voltage = ((maxValue - minValue) * 3.3)/4096.0;
  VRMS = (voltage/20)*0.707;
  AmpsRMS = ((VRMS*1000)/mVperAmp) - 0.3;
  watt = (AmpsRMS*240/1.2);
  sendJsonMessage(sensorNo, "voltage", voltage, volTopic);
  sendJsonMessage(sensorNo, "VRMS", VRMS, vrmsTopic);
  sendJsonMessage(sensorNo, "AmpsRMS", AmpsRMS, ampRmsTopic);
  sendJsonMessage(sensorNo, "Wattage", watt, wattTopic);
}

void ldrSensor(){
   ldrValue = analogRead(LDR_PIN);
   if (!beamBlocked && ldrValue < threshold){
     beamBlocked = true;
     blockStartMicros = micros();
   }
   if (beamBlocked && ldrValue > threshold + hysteresis){
     beamBlocked = false;
     unsigned long blockTime = micros() - blockStartMicros;
     blockTimeSec = blockTime / 1000000.0;
   }

   sendJsonMessage("LDR", "blockTime", blockTimeSec, ldrTopic);
}

void vibrationSensor(){
  int vibration = digitalRead(SW_420);
  if (vibration) {
    Serial.println("Detected vibration...");
  } else {
    Serial.println("...");
  }
  sendJsonMessage("SW-420", "vibration", vibration, vibTopic);
}

void ultrasonicSensor(){
  distance1 = sonar[0].read();
  sendJsonMessage("Ultrasonic Sensor1:", "distance1", distance1, dist1Topic);
  distance2 = sonar[1].read();
  sendJsonMessage("Ultrasonic Sensor2:", "distance2", distance2, dist2Topic);
}

void configPortal() {
  wm.addParameter(&p_client_id);
  wm.addParameter(&p_client_psk);
  wm.setSaveConfigCallback(saveConfigCallback);

  if (digitalRead(BUTTON) == LOW) {
    Serial.println("Button pressed, starting WiFiManager...");
    wm.startConfigPortal("ESP32_CONFIG");

    if (shouldSaveConfig) {
        Creds c = readParams();
        saveCredentials(c);
        Serial.println("Credentials saved");
        ESP.restart();
    } else {
      setup_wifi(c);
    }
  }
}

Creds readParams() {
  Creds c;
  c.SSID = wm.getWiFiSSID().c_str();
  c.PASS = wm.getWiFiPass().c_str();
  c.CLIENT_ID = String(p_client_id.getValue());
  c.CLIENT_PSK = String(p_client_psk.getValue());
  return c;
}

void setup() {
  Serial.begin(115200);
  dht11.begin();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SW_420, INPUT);

  analogReadResolution(12);
  
  configPortal();
  espTest();
}

void reconnect(const Creds &c) {
  if (mqttClient.connected()) return;
  Serial.print("MQTT state: ");
  Serial.println(mqttClient.state());
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), c.CLIENT_ID.c_str(), c.CLIENT_PSK.c_str())) {
      Serial.println("Connected. ");
      mqttClient.subscribe(subTopic);
    }
    delay(2000);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) setup_wifi(c);
  if (!mqttClient.connected()) reconnect(c);
  mqttClient.loop();
  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ultrasonicSensor();
    currentSensor("ACS712 1", ACS712_1);
//    currentSensor("ACS712 2", ACS712_2);
    ldrSensor();
    dhtSensor();
    vibrationSensor();
  }
}
