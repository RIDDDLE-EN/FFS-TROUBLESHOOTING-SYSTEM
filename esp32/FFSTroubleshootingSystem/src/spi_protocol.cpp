#include "spi_protocol.h"
#include "config.h"
#include <driver/spi_slave.h>
#include <HX711.h>

extern HX711 scale;	// Defined in sensors.cpp

#pragma pack(push, 1)
struct SPIFrame {
	uint8_t command[8];
	RawSensorData data;
	uint8_t padding[256 - 8 - sizeof(RawSensorData) -2];
	uint16_t crc;
};
#pragma pack(pop)

static_assert(sizeof(SPIFrame) == 256, "Frame must be 256 bytes");

DMA_ATTR uint8_t txBuf[256];
DMA_ATTR uint8_t rxBuf[256];

static bool dataReady = false;
static bool cmdPending = false;

static uint16_t crc16(const uint8_t *data, size_t len) {
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i] << 8;
		for (uint8_t j = 0; j < 8; j++) {
			if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
			else crc <<= 1;
		}
	}
	return crc;
}

static void IRAM_ATTR spi_post_setup_cb(spi_slave_transaction_t *trans) {
	digitalWrite(DATA_READY_PIN, LOW);
}

static void IRAM_ATTR spi_post_trans_cb(spi_slave_transaction_t * trans) {
	dataReady = false;
	if (rxBuf[0] != CMD_NONE) cmdPending = true;
}

void spiInit() {
	pinMode(DATA_READY_PIN, OUTPUT);
	digitalWrite(DATA_READY_PIN, LOW);

	spi_bus_config_t buscfg = {
		.mosi_io_num = SPI_MOSI,
		.miso_io_num = SPI_MISO,
		.sclk_io_num = SPI_SCK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 256,
	};

	spi_slave_interface_config_t slvcfg = {
		.spics_io_num = SPI_CS,
		.flags = 0,
		.queue_size = 3,
		.mode = 0,
		.post_setup_cb = spi_post_setup_cb,
		.post_trans_cb = spi_post_trans_cb,
	};

	spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);

	memset(txBuf, 0, 256);
	memset(rxBuf, 0, 256);

	spi_slave_transaction_t trans = {
		.length = 256 * 8,
		.tx_buffer = txBuf,
		.rx_buffer = rxBuf,
	};

	spi_slave_queue_trans(VSPI_HOST, &trans, portMAX_DELAY);

	Serialprintln("[SPI] Ready");
}

void spiUpdateData(const RawSensorData &data) {
	memcpy(txBuf + 8, &data, sizeof(RawSensorData));
	uint16_t crc = crc16(txtBuf, 254);
	memcpy(txBuf + 254, &crc, 2);
	dataReady = true;
}

void spiSignalReady() {
	if (dataReady) digitalWrite(DATA_READY_PIN, HIGH);
}

void spiProcessCommands() {
	if (!cmdPending) return;

	uint8_t cmd = rxBuf[0];

	switch (cmd) {
		case CMS_SET_LC_FAC: {
			float factor;
			memcpy(&factor, &rxBuf[4], sizeof(float));
			setLoadCellFactor(factor);
			Serial.printf("[SPI] Set LC Factor: %.2f\n", factor);
			break;
		}
		case CMD_SET_TC_OFF: {
			float offset;
			memcpy(&offset, rxBuf[4], sizeof(float));
			setThermoOffset(offset);
			Serial.printf("[SPI] Set TC Offset: %.2f\n", offset);
			break;
		}
		case CMD_TARE_LC:
			Serial.println("[SPI] Tare LC");
			scale.tare(20);
			break;
	}

	cmdPending = false;
	memset(rxBuf, 0, 256);

	spi_slave_transaction_t trans = {
		.length = 256 * 8,
		.tx_buffer = txBuf,
		.rx_buffer = rxBuf,
	};
	spi_slave_queue_trans(VSPI_HOST, &trans, portMAX_DELAY);
}
