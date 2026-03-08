#include <WiFi.h>
#include <PubSubClient.h>

#define ledPin 4

const char* ssid = Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup(){
  Serial.begin(115200);

  setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.serCallback(callback);
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

}
