#include "Config.h"
#include <cstring>

Config::Config() : shouldSaveConfig_(false) {
    preferences_.begin("wifi", false);
    loadWiFiCredentials();
}

Config::~Config() {
    preferences_.end();
}

void Config::saveWiFiCredentials(const char* ssid, const char* pass) {
    preferences_.putString("ssid", ssid);
    preferences_.putString("pass", pass);
    loadWiFiCredentials(); // Update local copies
}

void Config::loadWiFiCredentials() {
    String ssidStr = preferences_.getString("ssid", "");
    String passStr = preferences_.getString("pass", "");
    strcpy(ssid_, ssidStr.c_str());
    strcpy(pass_, passStr.c_str());
}