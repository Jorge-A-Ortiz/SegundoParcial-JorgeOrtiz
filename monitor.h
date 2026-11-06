#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>

#define MONITOR_ADC_BITS 12U
#define MONITOR_ADC_MAX 4095U

uint8_t Monitor_Percent(uint16_t adc);

#endif
