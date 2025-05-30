/*
 * averager.h
 *
 * calculates moving average , without highest and lowest value
 *
 *  Created on: Feb 8, 2018
 *      Author: dig
 */

#ifndef MAIN_INCLUDE_AVERAGER_H_
#define MAIN_INCLUDE_AVERAGER_H_

#include <stdint.h>

class Averager {
public:
  Averager(uint32_t averages = 5);
  int32_t write(int32_t value);
  int32_t getHighest();
  int32_t getLowest();
  float average();
  void *setAverages(uint32_t);
  void clear();


private:
  int32_t *pBuffer;
  uint32_t bufSize;
  uint32_t bufValues;
  uint32_t bufWriteIndex;
  int32_t highest;
  int32_t lowest;
};

#endif
