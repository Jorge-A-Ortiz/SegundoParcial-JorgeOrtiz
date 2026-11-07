#ifndef P10_TIMING_H
#define P10_TIMING_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { P10_ADC, P10_LED, P10_TELEMETRY } p10_event_t;

void P10_Init(void);
uint32_t P10_Now(void);
void P10_Start(uint32_t rxTime);
void P10_Complete(uint32_t rxTime);
void P10_Event(p10_event_t event);
void P10_Report(bool (*queueLine)(const char *line));

#endif
