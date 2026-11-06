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

bool Monitor_PollButton(monitor_t *monitor, uint32_t now, bool pressed)
{
    if (pressed != monitor->buttonRaw)
    {
        monitor->buttonRaw = pressed;
        monitor->buttonSince = now;
    }

    if (pressed == monitor->buttonStable || (uint32_t)(now - monitor->buttonSince) < 30U)
    {
        return false;
    }

    monitor->buttonStable = pressed;
    if (!pressed)
    {
        return false;
    }

    monitor->run = !monitor->run;
    monitor->alarmOn = monitor->run && monitor->sampleValid && monitor->pct >= monitor->threshold;
    return true;
}

bool Monitor_SetThreshold(monitor_t *monitor, unsigned threshold)
{
    if (threshold < 10U || threshold > 95U)
    {
        return false;
    }

    monitor->threshold = (uint8_t)threshold;
    monitor->alarmOn = monitor->run && monitor->sampleValid && monitor->pct >= monitor->threshold;
    return true;
}
