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
*/

#ifndef __SCD30_H__
#define __SCD30_H__

#include <stdint.h>
#include <stdbool.h>


#include "esp_log.h"
#include "driver/i2c.h"

#define	I2C_TIMEOUT_MS 100

// Uncomment the next #define if using an Teensy >= 3 or Teensy LC and want to use the dedicated I2C-Library for it
// Then you also have to include <i2c_t3.h> on your application instead of <Wire.h>

// #define USE_TEENSY3_I2C_LIB

//#include "Arduino.h"
//#ifdef USE_TEENSY3_I2C_LIB
//#include <i2c_t3.h>
//#else
//#include <Wire.h>
//#endif

//The default I2C address for the SCD30 is 0x61.
#define SCD30_ADDRESS 0x61

//Available commands

#define COMMAND_CONTINUOUS_MEASUREMENT 0x0010
#define COMMAND_SET_MEASUREMENT_INTERVAL 0x4600
#define COMMAND_GET_DATA_READY 0x0202
#define COMMAND_READ_MEASUREMENT 0x0300
#define COMMAND_AUTOMATIC_SELF_CALIBRATION 0x5306
#define COMMAND_SET_FORCED_RECALIBRATION_FACTOR 0x5204
#define COMMAND_SET_TEMPERATURE_OFFSET 0x5403
#define COMMAND_SET_ALTITUDE_COMPENSATION 0x5102
#define COMMAND_RESET 0xD304 // Soft reset
#define COMMAND_STOP_MEAS 0x0104
#define COMMAND_READ_FW_VER 0xD100


#define SIMULATEINTERVAL 50	//

typedef union
{
	uint8_t array[4];
	float value;
} ByteToFl; // paulvha

class SCD30
{
public:
	SCD30(void);

	esp_err_t begin(i2c_port_t masterPort, bool autoCalibrate = false, bool measBegin = true); //By default use Wire port


//	void enableDebugging(Stream &debugPort = Serial); //Turn on debug printing. If user doesn't specify then Serial will be used.

	esp_err_t beginMeasuring(uint16_t pressureOffset);
	esp_err_t beginMeasuring(void);
	esp_err_t StopMeasurement(void); // paulvha

	esp_err_t setAmbientPressure(uint16_t pressure_mbar);

	esp_err_t getSettingValue(uint16_t registerAddress, uint16_t *val);
	esp_err_t getFirmwareVersion(uint16_t *val) { return (getSettingValue(COMMAND_READ_FW_VER, val)); }
	uint16_t getCO2(void);
	float getHumidity(void);
	float getTemperature(void);

	uint16_t getMeasurementInterval(void);
	bool getMeasurementInterval(uint16_t *val) { return (getSettingValue(COMMAND_SET_MEASUREMENT_INTERVAL, val)); }
	esp_err_t setMeasurementInterval(uint16_t interval);

	uint16_t getAltitudeCompensation(void);
	bool getAltitudeCompensation(uint16_t *val) { return (getSettingValue(COMMAND_SET_ALTITUDE_COMPENSATION, val)); }
	esp_err_t setAltitudeCompensation(uint16_t altitude);

	bool getAutoSelfCalibration(void);
	esp_err_t setAutoSelfCalibration(bool enable);

	bool getForcedRecalibration(uint16_t *val) { return (getSettingValue(COMMAND_SET_FORCED_RECALIBRATION_FACTOR, val)); }
	esp_err_t setForcedRecalibrationFactor(uint16_t concentration);

	float getTemperatureOffset(void);
	bool getTemperatureOffset(uint16_t *val) { return (getSettingValue(COMMAND_SET_TEMPERATURE_OFFSET, val)); }
	esp_err_t setTemperatureOffset(float tempOffset);

	bool dataAvailable();
	esp_err_t readMeasurement();

	esp_err_t reset();

	esp_err_t sendCommand(uint16_t command, uint16_t arguments);
	esp_err_t sendCommand(uint16_t command);

	esp_err_t readRegister(uint16_t registerAddress, uint16_t *data );

	uint8_t computeCRC8(uint8_t data[], uint8_t len);

private:

	i2c_port_t _i2cPort;			//The generic connection to user's chosen I2C hardware

	//Global main datums
	float co2 = 0;
	float temperature = 0;
	float humidity = 0;

	//These track the staleness of the current data
	//This allows us to avoid calling readMeasurement() every time individual datums are requested
	bool co2HasBeenReported = true;
	bool humidityHasBeenReported = true;
	bool temperatureHasBeenReported = true;

	bool simulate = false;
	int simulateTmr;

	//Debug
//	Stream *_debugPort;			 //The stream to send debug messages to if enabled. Usually Serial.
//	boolean _printDebug = false; //Flag to print debugging variables
};
#endif
