#include <WiFi.h>

const char* ssid = Wokwi-GUEST";
const char* password = "";

unsigned long wifi_setup;

WiFiClient espClient;

void setup(){
  Serial.begin(115200);

  setup_wifi();
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

void loop(){

}
