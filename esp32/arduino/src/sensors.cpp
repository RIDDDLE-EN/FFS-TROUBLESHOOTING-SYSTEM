#include "sensors.h"
#include "config.h"
#include "logger.h"
#include "loadcell.h"
#include "motors.h"
#include "environment.h"
#include "vibration.h"
#include "freertos/queue.h"

static LoadCellModule 		loadcell;
static MotorModule		motor;
static EnvironmentModule	environment;
static VibrationModule		vibration;
static MPU6050			mpu;

static volatile uint32_t 	totalBagsProduced = 0;
static QueueHandle_t		rawSensorQueue;
static QueueHandle_t		diagnosticsQueue;

void sensorInit() {
	LOG("[SENSORS] Initializing Subsystems...");

	Wire.begin(MPU_SDA, MPU_SCL);
	mpu.initialize();
	vibration.init(mpu);
	loadCell.init();
	motors.init();
	environment.init();

	rawSensorQueue = xQueueCreate(1, sizeof(MotorData));
	diagnositcsQueue = xQueueCreate(5, sizeof(JawDiagnostics));
}

void resetBagCounter() {
	totalBagsProduced = 0;
	LOG("[METRICS] Bag counter reset to 0");
}

void sensorReadTask(void *pvParameters) {
	TickType_t xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(50);

	while (true) {
		float weight = loadCell.updateAndGetFiltered();
		MotorData mData = motors.update();
		EnvironmentData envData = environment.read();

		xQueueOverwrite(rawSensorQueue, &mData);

		vTaskDelayUntil(&xLastWakeTime, xFrequency);
	}
}

void vibrationAnalysisTask(void *pvParameters) {
	TickType_t xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(1000 / SAMPLE_RATE);

	int16_t ax, ay, az;
	jawDiagnostics jawData;

	while (true) {
		mpu.getAcceleration(&ax, &ay, &az);

		if (vibration.processImpact(ax, ay, az, jawData)) {
			xQueueSend(diagnosticsQueue, &jawData, 0);
		}

		vTaskDelayUntil(&xLastWakeTime, xFrequency);
	}
}

void dataProcessingTask(void *pvParameters) {
	JawDiagnostics lastesDiagnostics;
	MotorData latestMotors;

	while (true) {
		if (xQueueReceive(diagnosticsQueue &latestDiagnostics, 
