#ifndef CONFIG_H
#define CONFIG_H

#include <Preferences.h>

class Config {
public:
    Config();
    ~Config();
    void saveWiFiCredentials(const char* ssid, const char* pass);
    void loadWiFiCredentials();
    const char* getSSID() const { return ssid_; }
    const char* getPass() const { return pass_; }
    bool shouldSaveConfig() const { return shouldSaveConfig_; }
    void setShouldSaveConfig(bool value) { shouldSaveConfig_ = value; }

private:
    Preferences preferences_;
    bool shouldSaveConfig_;
    char ssid_[32];
    char pass_[32];
};

#endif // CONFIG_H