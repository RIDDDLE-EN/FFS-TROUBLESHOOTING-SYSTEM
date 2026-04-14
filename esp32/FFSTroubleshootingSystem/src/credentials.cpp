#include "credentials.h"
#include <Preferences.h>
#include <WiFiManager.h>

static Preferences prefs;

//NVS Namespacs and keys
static constexpr const char* NVS_NS		="creds";
static constexpr const char* KEY_SSID		="ssid";
static constexpr const char* KEY_PASS		="pass";
static constexpr const char* KEY_CLIENT_ID	="clientId";
static constexpr const char* KEY_CLIENT_PSK	="clientPsk";

//
bool loadCredentials(Creds &c) {
	if (!prefs.begin(NVS_NS, true)) return false;
	c.SSID		=prefs.getString(KEY_SSID, 	"");
	c.PASS		=prefs.getString(KEY_PASS, 	"");
	c.CLIENT_ID	=prefs.getString(KEY_CLIENT_ID,	"");
	c.CLIENT_PSK	=prefs.getString(KEY_CLIENT_PSK,"");
	prefs.end();

	return c.SSID.length() > 0;
}

void saveCredentials(const Creds &c) {
	if (!prefs.begin(NVS_NS, false)) {
		Serial.println("[Prefs] ERROR: Failed to open NVS for writing.");
		return;
	}
	prefs.putString(KEY_SSID,	c.SSID);
	prefs.putString(KEY_PASS,	c.PASS);
	prefs.putString(KEY_CLIENT_ID,	c.CLIENT_ID);
	prefs.putString(KEY_CLIENT_PSK,	c.CLIENT_PSK);
	prefs.end();
	Serial.println("[Prefs] Credentials saved to NVS.");
}

Creds runConfigPortal() {
	static bool shouldSave = false;

	WiFiManager wm;
	
	WiFiManagerParameter p_client_id("clientId", "MQTT Client ID", "", 32);

	const char* mqttPassHtml = 
		"<input id='mpw' name='mpw' type='password' maxlength='32' placeholder='MQTT Password' />";
	WiFiManagerParameter p_client_psk("mpw", "MQTT Password", "", 32, mqttPassHtml);

	wm.addParameter(&p_client_id);
	wm.addParameter(&p_client_psk);
	wm.setSaveConfigCallback([]() { shouldSave = true; });

	Serial.println("[WiFi] Launching config portal: ESP32_CONFIG");
	wm.startConfigPortal("ESP32_CONFIG");

	if (shouldSave) {
		Creds c;
		c.SSID		= wm.getWiFiSSID();
		c.PASS		= wm.getWiFiPass();
		c.CLIENT_ID	= String(p_client_id.getValue());
		c.CLIENT_PSK	= String(p_client_psk.getValue());
		saveCredentials(c);
		Serial.println("[WiFi] Config saved. Restarting...");
		ESP.restart();
	}

	//Reached only if portal closed without saving - caller uses existing creds.
	return Creds{};
}

void printCreds(const Creds &c) {
	Serial.printf("[Creds] SSID: %s | Client ID: %s\n", 
			c.SSID.c_str(), c.CLIENT_ID.c_str());
}
