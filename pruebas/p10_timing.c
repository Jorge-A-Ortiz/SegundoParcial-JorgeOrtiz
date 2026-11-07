#include "p10_timing.h"
#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "fsl_reset.h"
#include <stdio.h>

#define WINDOW_US 30000000U
#define RESPONSE_COUNT 150U

typedef struct
{
    uint32_t count;
    uint32_t last;
    uint32_t min;
    uint32_t max;
} interval_stats_t;

static uint32_t s_firstRx;
static volatile uint32_t s_count;
static volatile uint32_t s_rx[RESPONSE_COUNT];
static volatile uint32_t s_latency[RESPONSE_COUNT];
static interval_stats_t s_events[3];

void P10_Init(void)
{
    CLOCK_AttachClk(kFRO12M_to_CTIMER1);
    CLOCK_SetClockDiv(kCLOCK_DivCTIMER1, 1U);
    RESET_ReleasePeripheralReset(kCTIMER1_RST_SHIFT_RSTn);
    ctimer_config_t config;
    CTIMER_GetDefaultConfig(&config);
    config.prescale = 11U;
    CTIMER_Init(CTIMER1, &config);
    CTIMER_StartTimer(CTIMER1);
}

uint32_t P10_Now(void)
{
    return CTIMER_GetTimerCountValue(CTIMER1);
}

void P10_Start(uint32_t rxTime)
{
    if (s_firstRx == 0U)
    {
        s_firstRx = rxTime;
    }
}

void P10_Complete(uint32_t rxTime)
{
    uint32_t count = s_count;
    if (count < RESPONSE_COUNT)
    {
        s_latency[count] = P10_Now() - rxTime;
        s_rx[count] = rxTime;
        s_count = count + 1U;
    }
}

void P10_Event(p10_event_t event)
{
    uint32_t now = P10_Now();
    if (s_firstRx == 0U || now - s_firstRx >= WINDOW_US)
    {
        return;
    }

    interval_stats_t *stats = &s_events[event];
    if (stats->count != 0U)
    {
        uint32_t delta = now - stats->last;
        if (stats->count == 1U || delta < stats->min)
        {
            stats->min = delta;
        }
        if (delta > stats->max)
        {
            stats->max = delta;
        }
    }
    stats->last = now;
    ++stats->count;
}

void P10_Report(bool (*queueLine)(const char *line))
{
    static const char *const names[] = {"ADC", "LED", "TEL"};
    static uint32_t next;
    static uint32_t count;
    if (s_firstRx == 0U || P10_Now() - s_firstRx < WINDOW_US + 500000U)
    {
        return;
    }
    char line[96];
    int len;
    if (next == 0U)
    {
        count = s_count;
        len = snprintf(line, sizeof(line), "P10 BEGIN respuestas=%lu\r\n", (unsigned long)count);
    }
    else if (next <= count)
    {
        len = snprintf(line, sizeof(line), "P10 RX=%lu lat_us=%lu\r\n",
                       (unsigned long)(s_rx[next - 1U] - s_firstRx),
                       (unsigned long)s_latency[next - 1U]);
    }
    else if (next <= count + 3U)
    {
        uint32_t idx = next - count - 1U;
        const interval_stats_t *stats = &s_events[idx];
        len = snprintf(line, sizeof(line), "P10 %s eventos=%lu min_us=%lu max_us=%lu\r\n",
                       names[idx], (unsigned long)stats->count,
                       (unsigned long)stats->min, (unsigned long)stats->max);
    }
    else if (next == count + 4U)
    {
        len = snprintf(line, sizeof(line), "P10 END\r\n");
    }
    else
    {
        return;
    }
    if (len > 0 && (size_t)len < sizeof(line) && queueLine(line))
    {
        ++next;
    }
}
