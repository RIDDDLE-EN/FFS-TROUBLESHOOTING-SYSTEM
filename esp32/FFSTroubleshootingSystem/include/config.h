#pragma once

// Environmental 
#define DHT_PIN		4

// Motor monitoring (feeding motors 1 and 2)
#define ACS712_1	32
#define ACS712_2	33
#define ENCODER1_A	27
#define ENCODER1_B	14
#define ENCODER2_A	12
#define ENCODER2_B	13

// Weight and fill control
#define HX_DT		16
#define HX_SCK		17

// Roll centering 
#define ULTRA1_TRIG	26
#define ULTRA2_ECHO	25
#define ULTRA2_TRIG	0
#define ULTRA2_ECHO	2

// Bag length measurement
#define LDR_PIN		35
#define LASER_PIN	15

// Temperature system
#define TC_PLUS		34
#define TC_CONTINUITY	36

// I2C Bus (vibration(ADX324) + IR Temp(MLX90614))
#define I2C_SDA		21
#define I2C_SCL		22

// SPI communication to Raspberry
// Hardware SPI pins (VSPI on ESP32)
#define SPI_MISO	18
#define SPI_MOSI	23
#define SPI_SCK		18
#define SPI_CS		5

// Data-ready handshaking
#define DATA_READY_PIN	33

// 
// Constants
//

#define SPI_FREQUENCY	10000000
#define SPI_BUFFER_SIZE	256

#define ENCODER_PPR	20

#define MAIN_LOOP_HZ	100
#define MAIN_LOOP_US	(1000000 / MAIN_LOOP_HZ)
