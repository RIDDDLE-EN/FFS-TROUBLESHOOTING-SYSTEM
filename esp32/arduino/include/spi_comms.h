#pragma once
#include "bag.h"
#include "loadcell.h"
#include "calibration.h"

void spiCommsInit(BagModule &b, CalibrationModule &c, LoadCellModule &l);
void spiCommTask(void *parameter);
