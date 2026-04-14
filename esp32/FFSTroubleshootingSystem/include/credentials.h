#pragma once
#include <Arduino.h>

struct Creds { String SSID, PASS, CLIENT_ID, CLIENT_PSK; };

//Load credentials from NVS. Returns true if SSID is present.
bool loadCredentials(Creds &c);

//Persist credentials to NVS
void saveCredentials(const Creds &c);

//Launch the WiFiManager config portal. Savs + restarts if user submits,
//otherwise returns empty Creds (caller should fall back to loaded creds).
Creds runConfigPortal();

void printCreds(const Creds &c);
