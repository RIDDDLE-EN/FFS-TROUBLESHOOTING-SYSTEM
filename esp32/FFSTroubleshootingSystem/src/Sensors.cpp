#include "Sensors.h"
#include <Arduino.h>

Sensors::Sensors() : dht11_(21, DHT11), sonar1_(5, 18), sonar2_(19, 23), // Note: pins adjusted, original had 21 for both
                     ldrValue_(0), beamBlocked_(false), blockStartMicros_(0) {}

void Sensors::begin() {
    dht11_.begin();
    analogReadResolution(12);
}

SensorData Sensors::readAll() {
    SensorData data;
    data.distance1 = sonar1_.measureDistanceCM();
    data.distance2 = sonar2_.measureDistanceCM();

    data.voltage = getVPP();
    data.vrms = (data.voltage / 2.0) * 0.707; // Adjusted divisor
    data.ampsRMS = ((data.vrms * 1000) / 185) - 0.3; // mVperAmp
    data.watt = data.ampsRMS * 240 / 1.2; // Assuming voltage

    readLDR();
    data.ldrValue = ldrValue_;
    data.blockTimeSec = beamBlocked_ ? (micros() - blockStartMicros_) / 1000000.0 : 0;

    data.humidity = dht11_.readHumidity();
    data.temperature = dht11_.readTemperature();

    return data;
}

float Sensors::getVPP() {
    int maxValue = 0;
    int minValue = 4096;
    uint32_t start_time = millis();
    while ((millis() - start_time) < 1000) {
        int readValue = analogRead(34); // sensorIn
        if (readValue > maxValue) maxValue = readValue;
        if (readValue < minValue) minValue = readValue;
    }
    return ((maxValue - minValue) * 3.3) / 4096.0;
}

void Sensors::readLDR() {
    ldrValue_ = analogRead(35); // ldrPin
    if (!beamBlocked_ && ldrValue_ < 2000) { // threshold
        beamBlocked_ = true;
        blockStartMicros_ = micros();
    }
    if (beamBlocked_ && ldrValue_ > 2100) { // threshold + hysteresis
        beamBlocked_ = false;
    }
}

void Sensors::sendJsonMessage(PubSubClient& client, const char* sensorId, const char* label, float value, const char* pubTopic) {
    StaticJsonDocument<200> doc;
    doc["sensor_id"] = sensorId;
    doc[label] = value;
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(pubTopic, buffer);
    delay(100); // Reduced delay
}