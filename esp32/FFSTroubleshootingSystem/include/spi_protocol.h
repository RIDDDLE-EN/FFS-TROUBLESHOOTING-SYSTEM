#pragma once
#include "sensors.h"

// 
// Frame: [ COMMAND (8B) | RAW_SENSOR_DATA (244B) | CRC16 (2B) | PAD (2B) ] = 256B
// Commands (Pi to ESP32)
// 	0x00 = Read only
// 	0x01 = Set load cell factor (float in bytes 4-7)
// 	0x02 = Set thermocouple offset (float in bytes 4-7)
// 	0x03 = Tare load cell

#define CMD_NONE 	0x00
#define CMD_SET_LC_FAC	0x01
#define CMD_SET_TC_OFF	0x02
#define CMD_TARE_LC	0x03

void spiInit();
void spiUpdateData(const RawSensorData &data);
void spiSignalReady();
void spiProcessCommands();
