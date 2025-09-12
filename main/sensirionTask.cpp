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
#include "averager.h"
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
#define SCD30_TIMEOUT 2500 // * 10ms

// #define SIMULATE

static const char *TAG = "sensirionTask";

extern int scriptState;
static SCD30 airSensor;
bool calvaluesReceived;
static log_t avgVal; // avgeraged values
static log_t lastVal;

static int timeOuts;
static int retriestotal;
static int resets;

Averager co2Averager;
Averager tempAverager;
Averager humAverager;

float getTemperature(void) { return lastVal.temperature; }
const char *getFirmWareVersion();

bool sensirionError;

esp_err_t initSCD30(void) {
	int retries = 0;

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

// sends averaged values at  11,21,31 for module 1  12, 22 for module 2 etc
void updTransmitTask(void *pvParameter) {

	time_t now;
	struct tm timeinfo;
	char str[80];
	bool isSend = false;

	while (connectStatus != IP_RECEIVED) {
		vTaskDelay(100);
	}

	vTaskDelay(5000 / portTICK_PERIOD_MS); // wait to start for samples to collect

	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
		if ((timeinfo.tm_sec % 10) == moduleNr) {
			if (!isSend) {
				isSend = true;
				sprintf(str, "%s,%2.0f,%2.2f,%3.1f,%d,%lu\n\r", userSettings.moduleName, avgVal.co2,
						avgVal.temperature - userSettings.temperatureOffset, avgVal.hum - userSettings.RHoffset, getRssi(), (unsigned long)timeStamp);
				UDPsendMssg(UDPTXPORT, str, strlen(str));
				//	ESP_LOGI(TAG, "UDP send %s %d", str, timeinfo.tm_sec);
			}
		} else
			isSend = false;
	}
}

void sensirionTask(void *pvParameter) {
	int dummy = (int)pvParameter;
	i2c_port_t I2CmasterPort = (i2c_port_t)dummy;
	time_t now = 0;
	struct tm timeinfo;
	int lastminute = -1;

	char str[80];

	int sensirionTimeoutTimer = SCD30_TIMEOUT;
	ESP_LOGI(TAG, "Starting SCD30 task on I2C port %d", I2CmasterPort);

	co2Averager.setAverages(AVERAGES);
	tempAverager.setAverages(AVERAGES);
	humAverager.setAverages(AVERAGES);

	while ((airSensor.begin(I2CmasterPort, false, false) != ESP_OK) && (sensirionTimeoutTimer-- > 0)) {
		airSensor.reset();
		resets++;
		sensirionError = true;
		ESP_LOGE(TAG, "Air sensor not detected");

		int rssi = getRssi();
		sprintf(str, "%s,-999,-999,-999,%d", userSettings.moduleName, rssi);
		UDPsendMssg(UDPTXPORT, str, strlen(str));
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

	while (initSCD30() != ESP_OK) {
		sensirionError = true;
		vTaskDelay(200 / portTICK_PERIOD_MS);
		ESP_LOGE(TAG, "Error init Air sensor");
	}
	sensirionError = false;
	sensirionTimeoutTimer = SCD30_TIMEOUT;

	xTaskCreate(updTransmitTask, "udptx", 4 * 1024, NULL, 0, NULL);
	// testLog();
	while (1) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);

		if (sensirionError)
			sensirionTimeoutTimer = 1; //
		if (sensirionTimeoutTimer-- == 0) {
			ESP_LOGE(TAG, "Air sensor timeout");
			resets++;
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
			lastVal.temperature = airSensor.getTemperature(); //- userSettings.temperatureOffset;
			lastVal.hum = airSensor.getHumidity();			  //-userSettings.CO2offset;
			if (lastVal.co2 > 350) {						  // first measurement invalid, reject
				co2Averager.write(lastVal.co2 * 1000.0);
				tempAverager.write(lastVal.temperature * 1000.0);
				humAverager.write(lastVal.hum * 1000.0);
			}

#ifdef TURBO_MODE
			addToLog(avgVal); // add to cyclic log buffer
#else
			if (lastminute != timeinfo.tm_min) {
				avgVal.co2 = co2Averager.average() / 1000.0;
				avgVal.temperature = tempAverager.average() / 1000.0;
				avgVal.hum = humAverager.average() / 1000.0;
				avgVal.timeStamp = timeStamp;
				addToLog(avgVal);			  // add to cyclic log buffer
				lastminute = timeinfo.tm_min; // every minute
			 	ESP_LOGI(TAG, "CO2 value: %f ",avgVal.co2);

				// if (avgVal.co2 < 405) {		  //
				// 	ESP_LOGI(TAG, "CO2 value too low, Calibrate");
				// 	airSensor.setForcedRecalibrationFactor(410);
				// }
			}
#endif
		}
		if (calvaluesReceived) {
			calvaluesReceived = false;
			if (calValues.CO2 != NOCAL) { // then real CO2 received
				airSensor.setForcedRecalibrationFactor(calValues.CO2);
				calValues.CO2 = NOCAL;
			}
		}
	} // end while(1)
} // end sensirionTask

void getAvgMeasValues(sensorMssg_t *dest) {
	dest->co2 = avgVal.co2;
	dest->hum = avgVal.hum;
	dest->temperature = avgVal.temperature;
}
// CGI stuff

int printLog(log_t *logToPrint, char *pBuffer) {
	int len;
	len = sprintf(pBuffer, "%lu,", logToPrint->timeStamp);
	len += sprintf(pBuffer + len, "%3.0f,", logToPrint->co2);
	len += sprintf(pBuffer + len, "%3.2f,", logToPrint->temperature - userSettings.temperatureOffset);
	len += sprintf(pBuffer + len, "%3.2f\n", logToPrint->hum - userSettings.RHoffset);
	return len;
}

// prints last measurement values
int getRTMeasValuesScript(char *pBuffer, int count) {
	int len = 0;

	switch (scriptState) {
	case 0:
		scriptState++;
		len = printLog(&avgVal, pBuffer);
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
		len += sprintf(pBuffer + len, "%s,%s\n", "Sensornaam", userSettings.moduleName);
		len += sprintf(pBuffer + len, "%s,%3.0f\n", "CO2", avgVal.co2);
		len += sprintf(pBuffer + len, "%s,%3.2f\n", "temperatuur", avgVal.temperature - userSettings.temperatureOffset);
		len += sprintf(pBuffer + len, "%s,%3.1f\n", "Vochtigheid", avgVal.hum - userSettings.RHoffset);
		return len;
	case 1:
		scriptState++;
		len = sprintf(pBuffer, "%s,%1.2f\n", "temperatuur offset", userSettings.temperatureOffset);
		len += sprintf(pBuffer + len, "%s,%1.2f\n", "RH offset", userSettings.RHoffset);
		len += sprintf(pBuffer + len, "%s,%d\n", "RSSI", getRssi());
		len += sprintf(pBuffer + len, "%s,%s\n", "firmwareversie", getFirmWareVersion());
		len += sprintf(pBuffer + len, "%s,%s\n", "SPIFFS versie", wifiSettings.SPIFFSversion);
		return len;
		break;
	case 2:
		scriptState++;
		len = sprintf(pBuffer, "%s,%d\n", "timeouts", timeOuts);
		len += sprintf(pBuffer + len, "%s,%d\n", "retriestotal", retriestotal);
		len += sprintf(pBuffer + len, "%s,%d\n", "Sensor resets", resets);
		len += sprintf(pBuffer + len, "%s,%lu\n", "timeStamp", (unsigned long)timeStamp);
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
		len += sprintf(pBuffer + len, "%s\n", "CO2\n temperatuur\n RH");
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
		//	len = sprintf(pBuffer, "Actueel,Nieuw\n");
		//	len = sprintf(pBuffer, "Sensor naam\n");
		len = sprintf(pBuffer, "%s\n", userSettings.moduleName);
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

calValues_t calValues = {NOCAL, NOCAL, NOCAL};

const CGIdesc_t calibrateDescriptors[] = {
	{"dummy", NULL, FLT, 1}, // ??
	{"temperatuur", &calValues.temperature, FLT, 1},
	{"RH", &calValues.hum, FLT, 1},
	{"CO2", &calValues.CO2, FLT, 1},
};

void parseCGIWriteData(char *buf, int received) {
	bool save = false;

	if (strncmp(buf, "forgetWifi", 10) == 0) {
		ESP_LOGI(TAG, "Wifisettings erased");
		strcpy(wifiSettings.SSID, "xx");
		strcpy(wifiSettings.pwd, "xx");
		saveSettings();
		esp_restart();
	}
	if (strncmp(buf, "setCal:", 7) == 0) { // calvalues are written , in sensirionTasks write these to SCD30
		if (readActionScript(&buf[7], calibrateDescriptors, NR_CALDESCRIPTORS)) {
			calvaluesReceived = true; // update CO2 synchronous above

			ESP_LOGI(TAG, "calvalues received: %f, %f, %f", calValues.CO2, calValues.temperature, calValues.hum);

			if (calValues.temperature != NOCAL) { // then real temperature received
				userSettings.temperatureOffset = avgVal.temperature - calValues.temperature;
				save = true;
			}
			if (calValues.hum != NOCAL) { // then real temperature received
				userSettings.RHoffset = avgVal.hum - calValues.hum;
				save = true;
			}
		}
	}
	if (save)
		saveSettings();
}
