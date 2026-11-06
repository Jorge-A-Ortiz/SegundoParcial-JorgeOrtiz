#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#define MONITOR_ADC_BITS 12U
#define MONITOR_ADC_MAX 4095U

typedef struct
{
    uint16_t adc;
    uint8_t pct;
    uint8_t threshold;
    bool run;
    bool sampleValid;
    bool alarmOn;
} monitor_t;

void Monitor_Init(monitor_t *monitor);
void Monitor_UpdateSample(monitor_t *monitor, uint16_t adc);

uint8_t Monitor_Percent(uint16_t adc);

#endif
