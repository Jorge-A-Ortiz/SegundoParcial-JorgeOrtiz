#include "monitor.h"

uint8_t Monitor_Percent(uint16_t adc)
{
    if (adc > MONITOR_ADC_MAX)
    {
        adc = MONITOR_ADC_MAX;
    }
    return (uint8_t)(((uint32_t)adc * 100U + 2047U) / 4095U);
}
