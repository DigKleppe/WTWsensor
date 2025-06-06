/*
 This is a library written for the SCD30
 SparkFun sells these at its website: www.sparkfun.com
 Do you like this library? Help support SparkFun. Buy a board!
 https://www.sparkfun.com/products/14751

 Written by Nathan Seidle @ SparkFun Electronics, May 22nd, 2018

 Updated February 1st 2021 to include some of the features of paulvha's version of the library
 (while maintaining backward-compatibility):
 https://github.com/paulvha/scd30
 Thank you Paul!

 The SCD30 measures CO2 with accuracy of +/- 30ppm.

 This library handles the initialization of the SCD30 and outputs
 CO2 levels, relative humidty, and temperature.

 https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library

 Development environment specifics:
 Arduino IDE 1.8.13

 SparkFun code, firmware, and software is released under the MIT License.
 Please see LICENSE.md for more details.

 adapted for non-arduino ESP32 idf 5
 */

#define TAG "SDC30"

#include "SCD30.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include <driver/gpio.h>

#define POWERSWITCHPIN                                                                                                                               \
	GPIO_NUM_19				 // to switch power to Sensor off when not responding
							 // VDD connected to PFET
#define READYPIN GPIO_NUM_18 // to check if SCD30 is ready

esp_err_t swapBytes(uint8_t *src, int nrBytes) {
	uint8_t bytes[8];
	if (nrBytes < 8) {
		memcpy(bytes, src, nrBytes);
		for (int n = nrBytes - 1; n >= 0; n--)
			*src++ = bytes[n];
	} else
		return ESP_ERR_INVALID_ARG;

	return ESP_OK;
}

SCD30::SCD30(void) {
	// Constructor
}

esp_err_t SCD30::begin(i2c_port_t masterPort, bool autoCalibrate, bool measBegin) {
	esp_err_t err;

	_i2cPort = masterPort; // Grab which port the user wants us to use
	gpio_set_direction(POWERSWITCHPIN, GPIO_MODE_OUTPUT);
	gpio_set_level(POWERSWITCHPIN, 0);			   // active low
	gpio_set_direction(READYPIN, GPIO_MODE_INPUT); // set ready pin as input

	vTaskDelay(100); // wait for power

	simulate = false;
	uint16_t fwVer;

	err = getFirmwareVersion(&fwVer); // Read the firmware version. Return false if the CRC check fails.

	if (err != ESP_OK) {
		simulate = true;
		simulateTmr = SIMULATEINTERVAL;
		ESP_LOGE(TAG, "Error reading firmware version");
		ESP_LOGE(TAG, "Simulating");
		return err;
	}
	ESP_LOGI(TAG, "SCD30 firmware version: %x", fwVer);

	if (measBegin == false) // Exit now if measBegin is false
		return ESP_OK;

	// Check for device to respond correctly
	if (beginMeasuring() == ESP_OK) // Start continuous measurements
	{
		err = setMeasurementInterval(2);			  // 2 seconds between measurements
		err |= setAutoSelfCalibration(autoCalibrate); // Enable auto-self-calibration
	}

	if (err != ESP_OK)
		ESP_LOGE(TAG, "Error intializing (%s)", esp_err_to_name(err));

	return (err);
}

////Calling this function with nothing sets the debug port to Serial
////You can also call it with other streams like Serial1, SerialUSB, etc.
// void SCD30::enableDebugging(Stream &debugPort)
//{
//   _debugPort = &debugPort;
//   _printDebug = true;
// }

// Returns the latest available CO2 level
// If the current level has already been reported, trigger a new read
uint16_t SCD30::getCO2(void) {
	if (co2HasBeenReported == true) // Trigger a new read
		readMeasurement();			// Pull in new co2, humidity, and temp into global vars

	co2HasBeenReported = true;

	return (uint16_t)co2; // Cut off decimal as co2 is 0 to 10,000
}

// Returns the latest available humidity
// If the current level has already been reported, trigger a new read
float SCD30::getHumidity(void) {
	if (humidityHasBeenReported == true) // Trigger a new read
		readMeasurement();				 // Pull in new co2, humidity, and temp into global vars

	humidityHasBeenReported = true;

	return humidity;
}

// Returns the latest available temperature
// If the current level has already been reported, trigger a new read
float SCD30::getTemperature(void) {
	if (temperatureHasBeenReported == true) // Trigger a new read
		readMeasurement();					// Pull in new co2, humidity, and temp into global vars

	temperatureHasBeenReported = true;

	return temperature;
}

// Enables or disables the ASC
esp_err_t SCD30::setAutoSelfCalibration(bool enable) {
	if (enable)
		return sendCommand(COMMAND_AUTOMATIC_SELF_CALIBRATION, 1); // Activate continuous ASC
	else
		return sendCommand(COMMAND_AUTOMATIC_SELF_CALIBRATION, 0); // Deactivate continuous ASC
}

// Set the forced recalibration factor. See 1.3.7.
// The reference CO2 concentration has to be within the range 400 ppm ≤ cref(CO2) ≤ 2000 ppm.
esp_err_t SCD30::setForcedRecalibrationFactor(uint16_t concentration) {
	if (concentration < 400 || concentration > 2000) {
		return ESP_ERR_INVALID_ARG; // Error check.
	}
	return sendCommand(COMMAND_SET_FORCED_RECALIBRATION_FACTOR, concentration);
}

// Get the temperature offset. See 1.3.8.
float SCD30::getTemperatureOffset(void) {
	uint16_t response;
	readRegister(COMMAND_SET_TEMPERATURE_OFFSET, &response);

	union {
		int16_t signed16;
		uint16_t unsigned16;
	} signedUnsigned; // Avoid any ambiguity casting int16_t to uint16_t
	signedUnsigned.signed16 = response;

	return (((float)signedUnsigned.signed16) / 100.0);
}

// Set the temperature offset to remove module heating from temp reading
esp_err_t SCD30::setTemperatureOffset(float tempOffset) {
	// Temp offset is only positive. See: https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library/issues/27#issuecomment-971986826
	//"The SCD30 offset temperature is obtained by subtracting the reference temperature from the SCD30 output temperature"
	// https://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/9.5_CO2/Sensirion_CO2_Sensors_SCD30_Low_Power_Mode.pdf

	if (tempOffset < 0.0)
		return ESP_ERR_INVALID_ARG;

	uint16_t value = tempOffset * 100;

	return sendCommand(COMMAND_SET_TEMPERATURE_OFFSET, value);
}

// Get the altitude compenstation. See 1.3.9.
uint16_t SCD30::getAltitudeCompensation(void) {
	uint16_t response;
	readRegister(COMMAND_SET_ALTITUDE_COMPENSATION, &response);
	return response;
}

// Set the altitude compenstation. See 1.3.9.
esp_err_t SCD30::setAltitudeCompensation(uint16_t altitude) { return sendCommand(COMMAND_SET_ALTITUDE_COMPENSATION, altitude); }

// Set the pressure compenstation. This is passed during measurement startup.
// mbar can be 700 to 1200
esp_err_t SCD30::setAmbientPressure(uint16_t pressure_mbar) {
	if (pressure_mbar < 700 || pressure_mbar > 1200) {
		return ESP_ERR_INVALID_ARG;
	}
	return sendCommand(COMMAND_CONTINUOUS_MEASUREMENT, pressure_mbar);
}

// SCD30 soft reset
esp_err_t SCD30::reset() {
	gpio_set_level(POWERSWITCHPIN, 1); // switch off power, active low
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	gpio_set_level(POWERSWITCHPIN, 0); // switch on power
	vTaskDelay(100 / portTICK_PERIOD_MS);
	return sendCommand(COMMAND_RESET);
}

// Get the current ASC setting
bool SCD30::getAutoSelfCalibration() {
	uint16_t response;
	readRegister(COMMAND_AUTOMATIC_SELF_CALIBRATION, &response);
	if (response == 1) {
		return true;
	} else {
		return false;
	}
}

// Begins continuous measurements
// Continuous measurement status is saved in non-volatile memory. When the sensor
// is powered down while continuous measurement mode is active SCD30 will measure
// continuously after repowering without sending the measurement command.

esp_err_t SCD30::beginMeasuring(uint16_t pressureOffset) { return (sendCommand(COMMAND_CONTINUOUS_MEASUREMENT, pressureOffset)); }

// Overload - no pressureOffset
esp_err_t SCD30::beginMeasuring(void) { return (beginMeasuring(0)); }

// Stop continuous measurement
esp_err_t SCD30::StopMeasurement(void) { return (sendCommand(COMMAND_STOP_MEAS)); }

// Sets interval between measurements
// 2 seconds to 1800 seconds (30 minutes)
esp_err_t SCD30::setMeasurementInterval(uint16_t interval) { return sendCommand(COMMAND_SET_MEASUREMENT_INTERVAL, interval); }

// Gets interval between measurements
// 2 seconds to 1800 seconds (30 minutes)
uint16_t SCD30::getMeasurementInterval(void) {
	uint16_t interval = 0;
	getSettingValue(COMMAND_SET_MEASUREMENT_INTERVAL, &interval);
	return (interval);
}

// Returns true when data is available
bool SCD30::dataAvailable() {
	uint16_t response = 0;

	if (gpio_get_level(READYPIN) == 1) { // If the SCD30 is not ready, return false
	//	ESP_LOGI(TAG, "SCD30 ready");
		return true;
	}
	// readRegister(COMMAND_GET_DATA_READY, &response);

	// if (response == 1)
	// 	return (true);
	return (false);
}

// Get 18 bytes from SCD30
// Updates global variables with floats
// Returns true if success
esp_err_t SCD30::readMeasurement() {
	// Verify we have data from the sensor
	esp_err_t err;

	if (simulate) {
		if (simulateTmr-- == 0) {
			simulateTmr = SIMULATEINTERVAL;
			// Now copy the uint32s into their associated floats
			co2 = 456;
			temperature = 23.4;
			humidity = 56.7;
			// Mark our global variables as fresh
			co2HasBeenReported = false;
			humidityHasBeenReported = false;
			temperatureHasBeenReported = false;
			return (ESP_OK); // Success! New data simulator values in globals.
		}
		return ESP_ERR_NOT_FINISHED;
	}

	if (dataAvailable() == false)
		return (ESP_ERR_NOT_FINISHED);

	uint8_t bytesToCrc[2];	
	uint8_t receivedBytes[18] = {0};
	ByteToFl tempCO2;
	tempCO2.value = 0;
	ByteToFl tempHumidity;
	tempHumidity.value = 0;
	ByteToFl tempTemperature;
	tempTemperature.value = 0;
	uint8_t write_buf[2] = {COMMAND_READ_MEASUREMENT >> 8, COMMAND_READ_MEASUREMENT & 0xF};

	err = i2c_master_write_to_device(_i2cPort, SCD30_ADDRESS, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (err == ESP_OK) {
		vTaskDelay(3 / portTICK_PERIOD_MS);
		err = i2c_master_read_from_device(_i2cPort, SCD30_ADDRESS, receivedBytes, sizeof(receivedBytes), I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
	}
	if (err == ESP_OK) {
			for (uint8_t x = 0; x < sizeof(receivedBytes); x++) {
			uint8_t incoming = receivedBytes[x];

			switch (x) {
			case 0:
			case 1:
			case 3:
			case 4:
				tempCO2.array[x < 3 ? 3 - x : 4 - x] = incoming;
				bytesToCrc[x % 3] = incoming;
				break;
			case 6:
			case 7:
			case 9:
			case 10:
				tempTemperature.array[x < 9 ? 9 - x : 10 - x] = incoming;
				bytesToCrc[x % 3] = incoming;
				break;
			case 12:
			case 13:
			case 15:
			case 16:
				tempHumidity.array[x < 15 ? 15 - x : 16 - x] = incoming;
				bytesToCrc[x % 3] = incoming;
				break;
			default:
				// Validate CRC
				uint8_t foundCrc = computeCRC8(bytesToCrc, 2);
				if (foundCrc != incoming) {
					err = ESP_ERR_INVALID_CRC;
				}
				break;
			}
		}
	}
	if (err != ESP_OK) {
		if (err == ESP_ERR_INVALID_CRC) {
			ESP_LOGE(TAG, "CRC failed! Received:");
			for (int i = 0; i < sizeof(receivedBytes); i++) {
				ESP_LOGE(TAG, "Byte %d: %02X", i, receivedBytes[i]);
			};
			ESP_LOGE(TAG, "Expected CRC: %02X, Found CRC: %02X", computeCRC8(bytesToCrc, 2), receivedBytes[17]);
		} else {
			ESP_LOGE(TAG, "i2c_master_read_from_SCD30 failed (%s)!", esp_err_to_name(err));
			// If the read failed, we should not update the global variables
			// and return the error immediately.
			// This prevents stale data from being used in the globals.
		}
		return err;
	}

	// Now copy the uint32s into their associated floats
	co2 = tempCO2.value;
	temperature = tempTemperature.value;
	humidity = tempHumidity.value;

	// Mark our global variables as fresh
	co2HasBeenReported = false;
	humidityHasBeenReported = false;
	temperatureHasBeenReported = false;

	return (ESP_OK); // Success! New data available in globals.
}

// Gets a setting by reading the appropriate register.
// Returns true if the CRC is valid.
esp_err_t SCD30::getSettingValue(uint16_t registerAddress, uint16_t *val) {

	int retries = 3;
	bool ok = false;
	uint8_t receivedBytes[3];
	esp_err_t err;

	if (simulate)
		return ESP_OK;

	swapBytes((uint8_t *)&registerAddress, sizeof(registerAddress));

	do {
		ESP_LOGE(TAG, "i2c_master_write_to_SCD30");

		err = i2c_master_write_to_device(_i2cPort, SCD30_ADDRESS, (uint8_t *)&registerAddress, sizeof(registerAddress),
										 I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "i2c_master_write_to_SCD30 failed (%s)!", esp_err_to_name(err));
			vTaskDelay(3 / portTICK_PERIOD_MS);
		}
		vTaskDelay(3 / portTICK_PERIOD_MS);
		if (!err) {
			err = i2c_master_read_from_device(_i2cPort, SCD30_ADDRESS, receivedBytes, sizeof(receivedBytes), I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "i2c_master_read_from_SCD30 failed (%s)!", esp_err_to_name(err));
				vTaskDelay(3 / portTICK_PERIOD_MS);
			} else
				ok = true;
		}
		if (retries-- == 0)
			return ESP_ERR_TIMEOUT;
	} while (!ok);

	uint8_t crc = receivedBytes[2];
	*val = (uint16_t)receivedBytes[0] << 8 | receivedBytes[1];

	uint8_t expectedCRC = computeCRC8(receivedBytes, 2);
	if (crc == expectedCRC) // Return true if CRC check is OK
		return (ESP_OK);
	else
		ESP_LOGE(TAG, "CRC failed!");

	return (ESP_ERR_INVALID_CRC);
}

// Gets two bytes from SCD30
esp_err_t SCD30::readRegister(uint16_t registerAddress, uint16_t *data) {
	uint16_t receivedBytes;
	esp_err_t err;

	swapBytes((uint8_t *)&registerAddress, sizeof(registerAddress));
	err = i2c_master_write_to_device(_i2cPort, SCD30_ADDRESS, (uint8_t *)&registerAddress, sizeof(registerAddress),
									 I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (err == ESP_OK) {
		vTaskDelay(3 / portTICK_PERIOD_MS);
		err = i2c_master_read_from_device(_i2cPort, SCD30_ADDRESS, (uint8_t *)&receivedBytes, sizeof(receivedBytes),
										  I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
		if (err == ESP_OK) {
			swapBytes((uint8_t *)&receivedBytes, sizeof(receivedBytes));
			*data = receivedBytes;
		} else {
			ESP_LOGE(TAG, "Read register Read failed (%s)!", esp_err_to_name(err));
		}
	} else {
		// ESP_LOGE(TAG, "Read register Write failed (%s)!", esp_err_to_name(err));
	}
	return err;
}

// Sends a command along with arguments and CRC
esp_err_t SCD30::sendCommand(uint16_t command, uint16_t arguments) {

	if (simulate)
		return ESP_OK;

	uint8_t data[5];
	data[0] = command >> 8;
	data[1] = command & 0xFF;
	data[2] = arguments >> 8;
	data[3] = arguments & 0xFF;
	data[4] = computeCRC8(&data[2], 2); // Calc CRC on the arguments only, not the command
	return i2c_master_write_to_device(_i2cPort, SCD30_ADDRESS, data, sizeof(data), I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Sends just a command, no arguments, no CRC
esp_err_t SCD30::sendCommand(uint16_t command) {
	if (simulate)
		return ESP_OK;

	swapBytes((uint8_t *)&command, sizeof(command));
	return i2c_master_write_to_device(_i2cPort, SCD30_ADDRESS, (uint8_t *)&command, sizeof(command), I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Given an array and a number of bytes, this calculate CRC8 for those bytes
// CRC is only calc'd on the data portion (two bytes) of the four bytes being sent
// From: http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
// Tested with: http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
// x^8+x^5+x^4+1 = 0x31
uint8_t SCD30::computeCRC8(uint8_t data[], uint8_t len) {
	uint8_t crc = 0xFF; // Init with 0xFF

	for (uint8_t x = 0; x < len; x++) {
		crc ^= data[x]; // XOR-in the next input byte

		for (uint8_t i = 0; i < 8; i++) {
			if ((crc & 0x80) != 0)
				crc = (uint8_t)((crc << 1) ^ 0x31);
			else
				crc <<= 1;
		}
	}

	return crc; // No output reflection
}
