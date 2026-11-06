#include "monitor.h"

uint8_t Monitor_Percent(uint16_t adc)
{
    if (adc > MONITOR_ADC_MAX)
    {
        adc = MONITOR_ADC_MAX;
    }
    return (uint8_t)(((uint32_t)adc * 100U + 2047U) / 4095U);
}

void Monitor_Init(monitor_t *monitor)
{
    *monitor = (monitor_t){.threshold = 70U, .run = true};
}

void Monitor_UpdateSample(monitor_t *monitor, uint16_t adc)
{
    monitor->adc = adc;
    monitor->pct = Monitor_Percent(adc);
    monitor->sampleValid = true;

    if (!monitor->run || monitor->pct < monitor->threshold - 5U)
    {
        monitor->alarmOn = false;
    }
    else if (monitor->pct >= monitor->threshold)
    {
        monitor->alarmOn = true;
    }
}
