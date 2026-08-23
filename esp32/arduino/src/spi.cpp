#include "spi_comms.h"
#include "config.h"
#include "logger.h"
#include "sensors.h"

#include <string.h>
#include <Arduino.h>
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_attr.h"

static WORD_ALIGNED_ATTR uint8_t s_txBuf[SPI_PACKET_SIZE];
static WORD_ALIGNED_ATTR uint8_t s_rxBuf[SPI_PACKET_SIZE];
static bool s_calibrationActive = false;

static uint8_t calculateCRC8(const uint8_t *data, size_t len) {
	uint8_t crc = 0x00;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i]; 
		for (int b = 0; b < 8 ; b++) {
			crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
		}
	}
	return crc;
}

static void sendResponseFrame(uint8_t msgType, const void *payload, uint8_t payloadLen) {
	memset(s_txBuf, 0, SPI_PACKET_SIZE);
	s_txBuf[0] = SPI_START_FROM_ESP;
	s_txBuf[1] = msgType;
	s_txBuf[2] = payloadLen;

	if (payload && payloadLen > 0) {
		uint8_t maxSafeLen = (payloadLen > (SPI_PACKET_SIZE - 4)) ? (SPI_PACKET_SIZE -4) : payloadLen;
		memcpy(&s_txBuf[3], payload, maxSafeLen);
	}
	s_txBuf[SPI_PACKET_SIZE - 1] = calculateCRC8(s_txBuf, SPI_PACKET_SIZE - 1);
}

static bool isFrameValid(const uint8_t *buf) {
	if (buf[0] != SPI_START_FROM_PI) return false;
	return buf[SPI_PACKET_SIZE -1] == calculateCRC8(buf, SPI_PACKET_SIZE - 1);
}

void spiCommsInit() {
	gpio_set_direction((gpio_num_t)SPI_DATA_READY, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)SPI_DATA_READY, 0);

	spi_bus_config_t busCfg = {};
	busCfg.mosi_io_num	= SPI_MOSI;
	busCfg.miso_io_num	= SPI_MISO;
	busCfg.sclk_io_num	= SPI_CLK;
	busCfg.quadwp_io_num 	= -1;
	busCfg.quadhd_io_num	= -1;

	spi_slave_interface_config_t slvCfg = {};
	slvCfg.spics_io_num	= SPI_CS;
	slvCfg.queue_size	= 3;
	slvCfg.mode		= 0;

	if (spi_slave_initialize(SPI3_HOST, &busCfg, &slvCfg, SPI_DMA_CH_AUTO) != ESP_OK)
		LOG("[SPI] Core bus configuration initialization FAILED!");
}

void spiCommTask(void *parameter) {
	InterpretedSensorData localDataFrame = {};
	char logBuffer[LOG_MAX_MSG_LEN] = {};
	bool outboundDataPending = false;
	bool responsePending = false;

	while (true) {
		if (!responsePending) {
			bool hasLog = logDequeue(logBuffer);
			bool hasSensor = (xQueuePeek(queueProcessedToSPI, &localDataFrame, 0) == pdTRUE);

			if (hasLog) {
				sendResponseFrame(MSG_LOG, logBuffer, (uint8_t)strnlen(logBuffer, LOG_MAX_MSG_LEN -1) + 1);
			} else if (hasSensor && outboundDataPending) {
				xQueueReceive(queueProcessedToSPI, &localDataFrame, 0);
				sendResponseFrame(MSG_SENSOR_DATA, &localDataFrame, sizeof(InterpretedSensorData));
				outboundDataPending = false;
				gpio_set_level((gpio_num_t)SPI_DATA_READY, 0);
			} else {
				sendResponseFrame(MSG_IDLE, nullptr, 0);
			}
		}

		spi_slave_transaction_t transaction = {};
		transaction.length	= SPI_PACKET_SIZE * 8;
		transaction.tx_buffer	= s_txBuf;
		transaction.rx_buffer	= s_rxBuf;

		spi_slave_queue_trans(SPI3_HOST, &transaction, portMAX_DELAY);
		spi_slave_transaction_t *completedTrans = nullptr;
		spi_slave_get_trans_result(SPI3_HOST, &completedTrans, portMAX_DELAY);

		responsePending = false;

		if (isFrameValid(s_rxBuf)) {
			uint8_t masterCmd	= s_rxBuf[1];
			uint8_t len		= s_rxBuf[2];
			const uint8_t *payloadPtr = &s_rxBuf[3];
			responsePending = true;

			switch (masterCmd) {
				case CMD_PING:
					sendResponseFrame(MSG_PONG, nullptr, 0);
					break;
				case CMD_READ_SENSORS:
					if (xQueueReceive(queueProcessedToSPI, &localDataFrame, 0) == pdTRUE) {
						sendResponseFrame(MSG_SENSOR_DATA, &localDataFrame, sizeof(InterpretedSensorData));
					} else {
						sendResponseFrame(MSG_IDLE, nullptr, 0);
					}
					break;
				case CMD_READ_LOG:
					if (logDequeue(logBuffer)) {
						sendResponseFrame(MSG_LOG, logBuffer, (uint8_t)strnlen(logBuffer, LOG_MAX_MSG_LEN -1) + 1);
					} else {
						sendResponseFrame(MSG_IDLE, nullptr, 0);
					}
					break;
				
				case CMD_CAL_START:
					s_calibrationActive = true;
					setLoadCellFactor(0.0f);
					tareLoadCell();
					vTaskDelay(pdMS_TO_TICKS(100));
					{
						int32_t rawWeightAvg = readRawWeightAverage(10);
						sendResponseFrame(MSG_CAL_RAW_WEIGHT, &rawWeightAvg, sizeof(rawWeightAvg));
					}
					break;
				case CMD_CAL_FACTOR:
					if(len >= sizeof(float) && s_calibrationActive) {
						float scaleFactor = 0.0f;
						memcpy(&scaleFactor, payloadPtr, sizeof(float));
						if (scaleFactor != 0.0f) {
							setLoadCellFactor(scaleFactor);
							s_calibrationActive = false;
							sendResponseFrame(MSG_ACK, nullptr, 0);
						} else {
							sendResponseFrame(MSG_NACK, nullptr, 0);
						}
					} else {
						sendResponseFrame(MSG_NACK, nullptr, 0);
					}
					break;
				case CMD_TARE:
					tareLoadCell();
					sendResponseFrame(MSG_ACK, nullptr, 0);
					break;

				case CMD_RESET_BAGS:
					resetBagCounter();
					sendResponseFrame(MSG_ACK, nullptr, 0);
					break;

				case CMD_SET_THERMO_OFFSET:
					if (len >= sizeof(float)) {
						float offsetValue = 0.0f;
						memcpy(&offsetValue, payloadPtr, sizeof(float));
						setThermoOffset(offsetValue);
						sendResponseFrame(MSG_ACK, nullptr, 0);
					} else {
						sendResponseFrame(MSG_NACK, nullptr, 0);
					}
					break;
				default:
					sendResponseFrame(MSG_NACK, nullptr, 0);
					break;
			}
		}

		if (uxQueueMessagesWaiting(queueProcessedToSPI) > 0) {
			outboundDataPending = true;
			gpio_set_level((gpio_num_t)SPI_DATA_READY, 1);
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}


