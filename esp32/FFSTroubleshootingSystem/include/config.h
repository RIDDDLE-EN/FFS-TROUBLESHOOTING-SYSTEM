#pragma once

//Pin Definitions
#define DHT_PIN 	4
#define BUTTON 		13
#define LED_PIN 	5
#define ACS712_1 	32
#define ACS712_2 	33
#define LDR_PIN 	35
#define SW_420 		34

//HX711 Load Cell
#define HX_DT			18
#define HX_SCK			19
#define LOADCELL_MAX_KG		20.0f
#define LOADCELL_CALIB_KG	5.0f
#define LOADCELL_SAMPLES	10

//Ultrasonic sensors (trig, echo)
#define SONAR_NUM	2
#define SONAR1_TRIG	25
#define SONAR1_ECHO 	26
#define SONAR2_TRIG	27
#define SONAR2_ECHO 	14

//MQTT Broker
#define MQTT_SERVER	"broker.hivemq.com"
#define MQTT_PORT	1883

//MQTT TOPICS
#define TOPIC_DIST1	"esp32/distance1"
#define TOPIC_DIST2	"esp32/distance2"
#define TOPIC_VOLTAGE	"esp32/motor/voltage"
#define TOPIC_VRMS	"esp32/motor/vrms"
#define TOPIC_AMP_RMS	"esp32/motor/ampRms"
#define TOPIC_WATT	"esp32/motor/watt"
#define TOPIC_TEMP	"esp32/temperature"
#define TOPIC_HUM	"esp32/humidity"
#define TOPIC_LDR	"esp32/ldr"
#define TOPIC_WEIGHT	"esp32/loadcell/weight"
#define TOPIC_VIB	"esp32/vibration"
#define TOPIC_SUB	"esp32/test"

//LDR Beam-Break
#define LDR_THRESHOLD	2000
#define LDR_HYSTERESIS	100

//Loop Timing
#define SENSOR_INTERVAL_MS	2000
