#include <MyLogin.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <DHT.h>
#include <math.h>

#define DHT11_PIN 21
#define ledPin 4
#define trigPin 15
#define echoPin 4

const char* ssid = STASSID;
const char* password = STAPSK;
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht11(DHT11_PIN, DHT11);

long lastMsg = 0;
unsigned long duration = 0;
float distanceCm = 0.0;
float temperature = NAN;
float humidity = NAN;

void setup(){
  Serial.begin(115200);
  delay(100);

  pinMode(ledPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  //testing to see if code has uploaded
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);

  dht11.begin();
  setup_wifi();
  
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
}

void setup_wifi(){
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
}

void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");

  String messageTemp;
  
  for (int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "esp32/test") {
    Serial.print("Changing Output to ");

    if (messageTemp == "on"){
      Serial.println("led on");
      digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off"){
      Serial.println("led off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect(){
  if (mqttClient.connected()) return;
  Serial.println("Connecting to MQTT Broker...");

  while (!mqttClient.connected()){
    Serial.println("Reconnecting to MQTT Broker...");

    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), CLIENTID, CLIENTPSK)){
      Serial.println("Connected. ");
      mqttClient.subscribe("esp32/test");
    }
  }
}

void measureUltrasonic(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000UL);

  if (duration == 0) {
    distanceCm = -1.0;
    Serial.println("Ultrasonic: timeout (no echo).");
  }
  else {
    distanceCm = (float)duration/58.0;
    Serial.print("Ultrasonic duration (us): ");
    Serial.print(duration);
    Serial.print(" distance(cm): ");
    Serial.println(distanceCm, 2);
  }
}

void loop(){
  if (!mqttClient.connected()) reconnect();
  mqttClient.loop();

  long now = millis();
  
  if (now - lastMsg > 5000UL) {
    lastMsg = now;

    measureUltrasonic();

    char distString[16];
    if (distanceCm < 0) {
      snprintf(distString, sizeof(distString), "timeout");
      Serial.println("Publishing distance: timeout");
      if (mqttClient.connected()) mqttClient.publish("esp32/distance", distString);
    } else {
      dtostrf(distanceCm, 4, 2, distString);
      Serial.print("Publishing distance: ");
      Serial.println(distString);
      if (mqttClient.connected()){
        bool ok = mqttClient.publish("esp32/distance", distString);
        Serial.print("Publish distance ");
        Serial.println(ok ? "ok" : "FAILED");
      }
    }

    temperature = dht11.readTemperature();
    humidity = dht11.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("DHT read failed (NaN) - skipping publish.");
    } else {
      char tempString[16];
      char humString[16];
      dtostrf(temperature, 4, 2, tempString);
      dtostrf(humidity, 4, 2, humString);

      Serial.print("Publishing temp: ");
      Serial.println(tempString);
      Serial.print("Publishing hum: ");
      Serial.println(humString);

      if (mqttClient.connected()) {
        bool okT = mqttClient.publish("esp32/temperature", tempString);
        bool okH = mqttClient.publish("esp32/humidity", humString);
        Serial.print("Publish temp ");
        Serial.println(okT ? "OK" : "FAILED");
        Serial.print("Publish hum ");
        Serial.println(okH ? "OK" : "FAILED");
      }
    }
  }

  delay(10);
}
