#ifndef SENSORS_H
#define SENSORS_H

#include <DHT.h>
#include <UltrasonicHCSR04.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

struct SensorData {
    float distance1;
    float distance2;
    float voltage;
    float vrms;
    float ampsRMS;
    float watt;
    float temperature;
    float humidity;
    int ldrValue;
    float blockTimeSec;
};

class Sensors {
public:
    Sensors();
    void begin();
    SensorData readAll();
    void sendJsonMessage(PubSubClient& client, const char* sensorId, const char* label, float value, const char* pubTopic);

private:
    DHT dht11_;
    UltrasonicHCSR04 sonar1_;
    UltrasonicHCSR04 sonar2_;
    int ldrValue_;
    bool beamBlocked_;
    unsigned long blockStartMicros_;
    float getVPP();
    void readLDR();
};

#endif // SENSORS_H