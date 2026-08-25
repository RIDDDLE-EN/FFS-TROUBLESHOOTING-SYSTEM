#pragma once
#include "bag.h"
#include "loadcell.h"
#include "calibration.h"

void spiCommsInit();
void spiCommTask(void *parameter);
