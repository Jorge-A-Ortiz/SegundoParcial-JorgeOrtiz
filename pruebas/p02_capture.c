#include "p02_capture.h"
#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "fsl_inputmux.h"
#include "fsl_port.h"
#include "fsl_reset.h"
#include <stdio.h>

#define WINDOW_US 10000000U
#define REPORT_US 11000000U

typedef struct
{
    uint32_t edges;
    uint32_t last;
    uint32_t min;
    uint32_t max;
} capture_stats_t;

static volatile capture_stats_t s_adc;
static volatile capture_stats_t s_led;

static void Record(volatile capture_stats_t *stats, ctimer_capture_channel_t channel)
{
    uint32_t stamp = CTIMER_GetCaptureValue(CTIMER1, channel);
    if (stamp >= WINDOW_US)
    {
        return;
    }

    if (stats->edges != 0U)
    {
        uint32_t interval = stamp - stats->last;
        if (stats->edges == 1U || interval < stats->min)
        {
            stats->min = interval;
        }
        if (interval > stats->max)
        {
            stats->max = interval;
        }
    }
    stats->last = stamp;
    ++stats->edges;
}

static void Capture(uint32_t flags)
{
    if ((flags & kCTIMER_Capture0Flag) != 0U)
    {
        Record(&s_adc, kCTIMER_Capture_0);
    }
    if ((flags & kCTIMER_Capture1Flag) != 0U)
    {
        Record(&s_led, kCTIMER_Capture_1);
    }
}

static ctimer_callback_t s_callbacks[] = {Capture};

void P02_Init(void)
{
    const port_pin_config_t input = {
        .mux = kPORT_MuxAlt4,
        .pullSelect = kPORT_PullDown,
        .inputBuffer = kPORT_InputBufferEnable,
    };
    PORT_SetPinConfig(PORT3, 14U, &input);
    PORT_SetPinConfig(PORT3, 15U, &input);

    CLOCK_EnableClock(kCLOCK_GateINPUTMUX0);
    RESET_PeripheralReset(kINPUTMUX0_RST_SHIFT_RSTn);
    INPUTMUX_Init(INPUTMUX0);
    INPUTMUX_AttachSignal(INPUTMUX0, 0U, kINPUTMUX_CtimerInp6ToTimer1Captsel);
    INPUTMUX_AttachSignal(INPUTMUX0, 1U, kINPUTMUX_CtimerInp7ToTimer1Captsel);

    CLOCK_AttachClk(kFRO12M_to_CTIMER1);
    CLOCK_SetClockDiv(kCLOCK_DivCTIMER1, 1U);
    RESET_ReleasePeripheralReset(kCTIMER1_RST_SHIFT_RSTn);
    ctimer_config_t config;
    CTIMER_GetDefaultConfig(&config);
    config.prescale = 11U;
    CTIMER_Init(CTIMER1, &config);
    CTIMER_RegisterCallBack(CTIMER1, s_callbacks, kCTIMER_SingleCallback);
    NVIC_SetPriority(CTIMER1_IRQn, 2U);
    CTIMER_SetupCapture(CTIMER1, kCTIMER_Capture_0, kCTIMER_Capture_BothEdge, true);
    CTIMER_SetupCapture(CTIMER1, kCTIMER_Capture_1, kCTIMER_Capture_BothEdge, true);
    CTIMER_StartTimer(CTIMER1);
}

void P02_Report(bool (*queueLine)(const char *line))
{
    static unsigned next;
    if (next >= 2U || CTIMER_GetTimerCountValue(CTIMER1) < REPORT_US)
    {
        return;
    }

    if (next == 0U)
    {
        DisableIRQ(CTIMER1_IRQn);
    }
    const volatile capture_stats_t *stats = next == 0U ? &s_adc : &s_led;
    char line[96];
    int len = snprintf(line, sizeof(line), "P02 %s ventana_us=%lu flancos=%lu min_us=%lu max_us=%lu\r\n",
                       next == 0U ? "ADC" : "LED", (unsigned long)WINDOW_US,
                       (unsigned long)stats->edges, (unsigned long)stats->min,
                       (unsigned long)stats->max);
    if (len > 0 && (size_t)len < sizeof(line) && queueLine(line))
    {
        ++next;
    }
}
