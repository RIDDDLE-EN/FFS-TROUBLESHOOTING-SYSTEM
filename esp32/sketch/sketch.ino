#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <DHT.h>

#define DHT22_PIN 21
#define ledPin 4

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHT22_PIN, DHT22);

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
char msg[50];
long lastMsg = 0;
int value;
float temperature, humidity;

void setup(){
  Serial.begin(115200);

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

void callback(char* topic, char* payload, unsigned int length){
  Serial.print("Callback - ");
  Serial.print("Message:");

  String messageTemp;
  
  for (int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }

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
  Serial.println("Connecting to MQTT Broker...");

  while (!mqttClient.connected()){
    Serial.println("Reconnecting to MQTT Broker...");

    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), "testUser0592", "test0592")){
      Serial.println("Connected. ");
      mqttClient.subscribe("esp32/test");
    }
  }
}

void loop(){
  if (!mqttClient.connected()) reconnect();
  mqttClient.loop();

  long now = millis();
  
  if (now - lastMsg > 5000) {
    lastMsg = now;

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    mqttClient.publish("esp32/temperature", tempString);

    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    Serial.print("humidity: ");
    Serial.println(humString);
    mqttClient.publish("esp32/humidity", humString);
  }
}
