#include "sensors.h"
#include "spi_comms"
#include <Wire.h>
#include <HX711.h>
#include <MPU6050.h>
#include "freertos/queue.h"

static HX711 			scale;
static MPU6050			mpu;

static CalibrationModoule	calibration;
static LoadCellModule 		loadcell;
static MotorModule		motor;
static EnvironmentModule	environment;
static VibrationModule		vibration;
static BagModule 		bag;
static ThermocoupleModule	thermo;
static RollModule		roll;

static volatile uint32_t 	totalBagsProduced = 0;
static QueueHandle_t		rawSensorQueue;
static QueueHandle_t		diagnosticsQueue;

BagData g_bagData = {0.0f, 0};
QueueHandle_t queueProcessedToSPI = nullptr;

void sensorsInit() {
	Wire.begin(I2C_SDA, I2C_SCL);
	mpu.initialize();

	calibration.init(scale, HX_DT, HX_SCK);
	loadcell.init(scale, calibration);
	vibration.init(mpu);

	bag.init(LASER_PIN, LDR_PIN);
	roll.init(TRIG1, ECHO1, TRIG2, ECHO2);
	motor.init();
	environment.init();

	spiCommsInit();

	queueProcessedToSPI = xQueueCreate(5, sizeof(InterpretedSensorData));

	xTaskCreatePinnedToCore(
		dataProcessingTask,
		"SensorDSP:,
		8192,
		nullptr,
		5,
		nullptr,
		1
	);
}

void dataProcessingTask(void *pvParameters) {
	InterpretedSensorData output = {};

	TickType_t xLastWakeTime = xTaksGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(20);

	while (1) {
		// Environment
		EnvironmentData envData;
		environment.read(endData);
		output.ambient_temp = envData.temperature;
		output.ambient_hum  = envData.humidity;
		output.env_sensor_ok = envData.valid;
		output.env_status    = EnvironmentModule::evaluateEnvironment(envData);

		// Motors
		MotorData motData;
		motor.update(motData);
		output.current1 = motData.current1;
		output.rpm1 	= motData.rpm1;
		output.motor1_running = motData.motor1_running;

		output.current2 = motData.current2;
		output.rpm2 	= motData.rpm2;
		output.motor2_running = motData.motor2_running;

		// Load cell
		LoadCellData lcData;
		loadCell.updateAndGetFiltered(lcData);
		output.weight_grams = lcData.weight_grams;
		output.loadcell_ok  = lcData.loadcell_ok;

		// Bag Detection
		bag.processBag(g_bagData, motData.rpm1);
		output.bag_length_cm = g_bagData.baglength;
		output.bags_counted  = g_bagData.bagsProduced;

		// Thermocouple
		ThermoData tcData;
		thermo.read_tc(tcData, TC_PIN, calibration);
		output.seal_temp = tcData.seal_temp;
		output.tc_connected = tcData.tc_connected;
		output.thermocople_ok = tcData.thermocouple_ok;

		// Roll Centering
		RollData rollData;
		roll.calculateOffset(rollData);
		output.roll_center_offset_cm = rollData.roll_center_offset_cm;
		output.roll_centered = rollData.roll_centered;
		output.ultrasonic_ok = rollData.ultrasonic_ok;

		// Vibration
		int16_t ax, ay az;
		mpu.getAcceleration(&ax, &ay, &az);
		JawDiagnostics jawDiag;

		if (vibration.processImpact(ax, ay, az, jawDiag)) {
			output.impact_amplitude = jawDiag.impact_amplitude;
			output.knife_frequency  = jawDiag.knife_frequency;
			output.is_updated	= jawDiag.is_updated;
		} else {
			output.is_updated = false;
		}

		xQueueOverwrite(queueProcessedToSPI, &output);
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
	}
}
