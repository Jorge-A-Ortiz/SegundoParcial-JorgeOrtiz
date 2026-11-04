#include "board.h"
#include "app.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_ctimer.h"
#include <stdbool.h>
#include <stdint.h>

static volatile uint32_t s_milliseconds;

static void TimerCallback(uint32_t flags)
{
    if ((flags & kCTIMER_Match0Flag) != 0U)
    {
        ++s_milliseconds;
    }
}

static ctimer_callback_t s_timerCallbacks[] = {TimerCallback};

static void InitializeTimer(void)
{
    ctimer_config_t timerConfig;
    CTIMER_GetDefaultConfig(&timerConfig);
    timerConfig.prescale = 11U;
    CTIMER_Init(CTIMER0, &timerConfig);

    ctimer_match_config_t matchConfig = {0};
    matchConfig.matchValue = 1000U;
    matchConfig.enableCounterReset = true;
    matchConfig.enableInterrupt = true;

    CTIMER_RegisterCallBack(CTIMER0, s_timerCallbacks, kCTIMER_SingleCallback);
    CTIMER_SetupMatch(CTIMER0, kCTIMER_Match_0, &matchConfig);
    NVIC_SetPriority(CTIMER0_IRQn, 1U);
    EnableIRQ(CTIMER0_IRQn);
    CTIMER_StartTimer(CTIMER0);
}

int main(void)
{
    BOARD_InitHardware();
    InitializeTimer();

    uint32_t lastActivityChange = 0U;
    bool activityOn = false;

    while (1)
    {
        uint32_t now = s_milliseconds;

        while ((uint32_t)(now - lastActivityChange) >= 500U)
        {
            lastActivityChange += 500U;
            activityOn = !activityOn;
            GPIO_PinWrite(GPIO3, 13U, activityOn ? 1U : 0U);
        }
    }
}
