/*
 * sensirionTask.cpp
 *
 *  Created on: Jan 5, 2022
 *      Author: dig
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "SCD30.h"
#include "cgiScripts.h"
#include "log.h"
#include "sensirionTask.h"
#include "settings.h"
#include "udpClient.h"
#include "wifiConnect.h"

#include <cstring>
#include <math.h>

#define UDPTXPORT 5050
#define OLDUDPTXPORT 5001
#define MAXRETRIES 5
#define SCD30_TIMEOUT 600

// #define SIMULATE

static const char *TAG = "sensirionTask";

extern int scriptState;
static SCD30 airSensor;
static bool calvaluesReceived;

float getTemperature(void) { return lastVal.temperature; }
extern bool connected;

bool sensirionError;

esp_err_t initSCD30(void) {
	int retries = 0;
	int retriestotal = 0;
	retries = MAXRETRIES;
	esp_err_t err;

	vTaskDelay(2000 / portTICK_PERIOD_MS); // boot up time = 2 seconds

	do {
		err = airSensor.setMeasurementInterval(MEASINTERVAL);
		retriestotal++;
	} while (err != ESP_OK && (retries-- > 0));
	vTaskDelay(100 / portTICK_PERIOD_MS); // wait for SCD30 to be ready
	if (err == ESP_OK) {
		retries = MAXRETRIES;
		do {
			err = airSensor.setAutoSelfCalibration(false); // disable ASC
			retriestotal++;
		} while (err != ESP_OK && (retries-- > 0));
	}

	if (err == ESP_OK) {
		retries = MAXRETRIES;
		do {
			err = airSensor.beginMeasuring(0);
			retriestotal++;
		} while (err != ESP_OK && (retries-- > 0));
	}
	ESP_LOGI(TAG, "retriestotal: %d", retriestotal);
	if (err == ESP_OK)
		ESP_LOGI(TAG, "initialized");
	else
		ESP_LOGE(TAG, "Error initializing!");
	return err;
}

void sensirionTask(void *pvParameter) {
	int dummy = (int)pvParameter;
	i2c_port_t I2CmasterPort = (i2c_port_t)dummy;
	time_t now = 0;
	struct tm timeinfo;
	int lastminute = -1;
	char str[50];

	int sensirionTimeoutTimer = SCD30_TIMEOUT;

	time(&now);

	ESP_LOGI(TAG, "Starting SCD30 task on I2C port %d", I2CmasterPort);


	while ((airSensor.begin(I2CmasterPort, false, false) != ESP_OK) && (sensirionTimeoutTimer-- > 0)) {
		airSensor.reset();

		ESP_LOGE(TAG, "Air sensor not detected");
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}

	while (initSCD30() != ESP_OK) {
		sensirionError = true;
		vTaskDelay(200 / portTICK_PERIOD_MS);
		ESP_LOGE(TAG, "Error init Air sensor");
	}
	sensirionError = false;

	sensirionTimeoutTimer = SCD30_TIMEOUT;
	// testLog();
	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);

		if (sensirionError)
			sensirionTimeoutTimer = 1; //
		if (sensirionTimeoutTimer-- == 0) {
			ESP_LOGE(TAG, "Air sensor timeout");

			airSensor.reset();
			if (initSCD30() != ESP_OK)
				sensirionError = true;
			else
				sensirionError = false;

			sensirionTimeoutTimer = SCD30_TIMEOUT;
			while ((airSensor.begin(I2CmasterPort, false, false) != ESP_OK) && (sensirionTimeoutTimer-- > 0))
				;

			vTaskDelay(200 / portTICK_PERIOD_MS);
		}
		if (airSensor.readMeasurement() == ESP_OK) {
			sensirionTimeoutTimer = SCD30_TIMEOUT;
		
			lastVal.co2 = airSensor.getCO2();
			lastVal.temperature = airSensor.getTemperature() - userSettings.temperatureOffset;
			lastVal.hum = airSensor.getHumidity();

			if (lastVal.co2 > 400) { // first measurement invalid, reject
				lastVal.timeStamp = timeStamp;
			}
			int rssi = getRssi();
			sprintf(str, "%s,%2.0f,%2.2f,%3.1f,%d", userSettings.moduleName, lastVal.co2, lastVal.temperature, lastVal.hum, rssi);
			UDPsendMssg(UDPTXPORT, str, strlen(str));
			ESP_LOGI(TAG, "%s", str);

#ifdef TURBO_MODE
			addToLog(lastVal); // add to cyclic log buffer
#else
			time(&now);
			localtime_r(&now, &timeinfo);
			if (lastminute != timeinfo.tm_min) {
				addToLog(lastVal);			  // add to cyclic log buffer
				lastminute = timeinfo.tm_min; // every minute
			}
#endif
		}
		if (calvaluesReceived) {
			calvaluesReceived = false;
			ESP_LOGI(TAG, "calvalues set: %f, %f", calValues.temperature, calValues.CO2);
			if (calValues.CO2 != NOCAL) { // then real CO2 received
				airSensor.setForcedRecalibrationFactor(calValues.CO2);
				calValues.CO2 = NOCAL;
			}

			ESP_LOGI(TAG, "calvalues set: %f, %f", calValues.temperature, calValues.CO2);
		}
	} // end while(1)
} // end sensirionTask

// CGI stuff

int printLog(log_t *logToPrint, char *pBuffer) {
	int len;
	len = sprintf(pBuffer, "%lu,", logToPrint->timeStamp);
	len += sprintf(pBuffer + len, "%3.0f,", logToPrint->co2);
	len += sprintf(pBuffer + len, "%3.2f,", logToPrint->temperature);
	len += sprintf(pBuffer + len, "%3.2f\n", logToPrint->hum);
	return len;
}

// prints last measurement values
int getRTMeasValuesScript(char *pBuffer, int count) {
	int len = 0;

	switch (scriptState) {
	case 0:
		scriptState++;
		len = printLog(&lastVal, pBuffer);
		return len;
		break;
	default:
		break;
	}
	return 0;
}

int getInfoValuesScript(char *pBuffer, int count) {
	int len = 0;
	switch (scriptState) {
	case 0:
		scriptState++;
		len = sprintf(pBuffer, "%s\n", "Naam,Waarde");
		len += sprintf(pBuffer + len, "%s,%3.0f\n", "CO2", lastVal.co2);
		len += sprintf(pBuffer + len, "%s,%3.2f\n", "temperatuur", lastVal.temperature);
		len += sprintf(pBuffer + len, "%s,%3.0f\n", "Vochtigheid", lastVal.hum);
		len += sprintf(pBuffer + len, "%s,%1.2f\n", "temperatuur offset", userSettings.temperatureOffset);
		len += sprintf(pBuffer + len, "%s,%d\n", "RSSI", getRssi());
		return len;
		break;
	default:
		break;
	}
	return 0;
}

// only build javascript table

int getCalValuesScript(char *pBuffer, int count) {
	int len = 0;
	switch (scriptState) {
	case 0:
		scriptState++;
		len = sprintf(pBuffer, "%s\n", "Meting,Referentie,Stel in");
		len += sprintf(pBuffer + len, "%s\n", "CO2\n temperatuur");
		return len;
		break;
	default:
		break;
	}
	return 0;
}

int getSensorNameScript(char *pBuffer, int count) {
	int len = 0;
	switch (scriptState) {
	case 0:
		scriptState++;
		len = sprintf(pBuffer, "Actueel,Nieuw\n");
		len += sprintf(pBuffer + len, "%s\n", userSettings.moduleName);
		return len;
		break;
	default:
		break;
	}
	return 0;
}

int saveSettingsScript(char *pBuffer, int count) {
	saveSettings();
	return 0;
}

int cancelSettingsScript(char *pBuffer, int count) {
	loadSettings();
	return 0;
}

calValues_t calValues = {NOCAL, NOCAL};

const CGIdesc_t calibrateDescriptors[] = {
	{"dummy", NULL, FLT, 1}, // ??
	{"temperatuur", &calValues.temperature, FLT, 1},
	{"CO2", &calValues.CO2, FLT, 1},
};


void parseCGIWriteData(char *buf, int received) {

	bool save = false;

	if (sscanf(buf, "setName:moduleName=%s", userSettings.moduleName) == 1) {
		ESP_LOGI(TAG, "Hostname set to %s", userSettings.moduleName);
		save = true;
	}

	if (strncmp(buf, "setCal:", 7) == 0) { // calvalues are written , in sensirionTasks write these to SCD30
		if (readActionScript(&buf[7], calibrateDescriptors, NR_CALDESCRIPTORS)) {
			calvaluesReceived = true;
			ESP_LOGI(TAG, "calvalues received: %f, %f", calValues.CO2, calValues.temperature);

			if (calValues.temperature != NOCAL) { // then real temperature received
				float measuredTemp = lastVal.temperature + userSettings.temperatureOffset;
				userSettings.temperatureOffset = measuredTemp - calValues.temperature;
				save = true;
			}
		}
	}
	if (save)
		saveSettings();
}
