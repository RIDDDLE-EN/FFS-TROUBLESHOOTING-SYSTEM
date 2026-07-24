#pragma once

void loggerInit();
void logMessage(const char *fmt, ...);
bool logDequeue(char *buf);
int  logPending();
#define LOG(fmt, ...)	logMessage(fmt, ##__VA_ARGS__)
