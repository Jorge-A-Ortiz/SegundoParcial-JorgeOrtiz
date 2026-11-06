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
    bool buttonRaw;
    bool buttonStable;
    uint32_t buttonSince;
} monitor_t;

bool Monitor_PollButton(monitor_t *monitor, uint32_t now, bool pressed);
bool Monitor_SetThreshold(monitor_t *monitor, unsigned threshold);
void Monitor_SetRun(monitor_t *monitor, bool run);
void Monitor_Init(monitor_t *monitor);
void Monitor_UpdateSample(monitor_t *monitor, uint16_t adc);

uint8_t Monitor_Percent(uint16_t adc);

#endif
