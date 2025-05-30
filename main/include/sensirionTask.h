/*
 * sensirionTask.h
 *
 *  Created on: Apr 18, 2022
 *      Author: dig
 */

#ifndef MAIN_INCLUDE_SENSIRIONTASK_H_
#define MAIN_INCLUDE_SENSIRIONTASK_H_

#include "cgiScripts.h"

#define MEASINTERVAL			 	20  // interval for sensiron sensor in seconds
#define LOGINTERVAL					60

#define NR_CALDESCRIPTORS 			3
#define NOCAL 						99999

typedef struct {
	float temperature;
	float RH;
	float CO2;
} calValues_t;

extern calValues_t calValues;

extern const CGIdesc_t calibrateDescriptors[NR_CALDESCRIPTORS];


float getTemperature (void);

int getRTMeasValuesScript(char *pBuffer, int count) ;
int getNewMeasValuesScript(char *pBuffer, int count);
int getLogScript(char *pBuffer, int count);
int getInfoValuesScript (char *pBuffer, int count);
int getCalValuesScript (char *pBuffer, int count);
int saveSettingsScript (char *pBuffer, int count);
int cancelSettingsScript (char *pBuffer, int count);
int calibrateRespScript(char *pBuffer, int count);

void sensirionTask(void *pvParameter);


#endif /* MAIN_INCLUDE_SENSIRIONTASK_H_ */
