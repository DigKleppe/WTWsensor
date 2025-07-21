/*
 * sensirionTask.h
 *
 *  Created on: Apr 18, 2022
 *      Author: dig
 */

#ifndef MAIN_INCLUDE_SENSIRIONTASK_H_
#define MAIN_INCLUDE_SENSIRIONTASK_H_

#include "cgiScripts.h"

//#define TURBO_MODE				// use turbo mode for faster measurements

#ifdef TURBO_MODE
#define MEASINTERVAL			 	2 // TEST  20  // interval for sensiron sensor in seconds
#else
#define MEASINTERVAL			 	10 // interval for sensiron sensor in seconds
#endif

#define LOGINTERVAL					60

#define AVERAGES 					30	

#define NR_CALDESCRIPTORS 			4
#define NOCAL 						99999

typedef struct {
	float CO2;
	float temperature;
	float hum;
} calValues_t;

typedef struct {
	float co2;
	float temperature;
	float hum;
	int rssi;
} sensorMssg_t;


extern calValues_t calValues;
extern bool sensirionError;
extern bool calvaluesReceived;
extern int moduleNr;  // set by solderlink 
void getAvgMeasValues ( sensorMssg_t * dest);

extern const CGIdesc_t calibrateDescriptors[NR_CALDESCRIPTORS];

float getTemperature (void);

int getRTMeasValuesScript(char *pBuffer, int count) ;
int getNewMeasValuesScript(char *pBuffer, int count);
int getLogScript(char *pBuffer, int count);
int getInfoValuesScript (char *pBuffer, int count);
int getCalValuesScript (char *pBuffer, int count);
int getSensorNameScript(char *pBuffer, int count);
int saveSettingsScript (char *pBuffer, int count);
int cancelSettingsScript (char *pBuffer, int count);
int calibrateRespScript(char *pBuffer, int count);

void sensirionTask(void *pvParameter);


#endif /* MAIN_INCLUDE_SENSIRIONTASK_H_ */
