#include "board.h"
#include "app.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_ctimer.h"
#include "fsl_lpuart.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define TX_SIZE 256U

static volatile uint32_t s_milliseconds;
static volatile uint8_t s_txBuffer[TX_SIZE];
static volatile uint16_t s_txHead;
static volatile uint16_t s_txTail;

void LPUART0_IRQHandler(void)
{
    if ((LPUART_GetEnabledInterrupts(LPUART0) & kLPUART_TxDataRegEmptyInterruptEnable) != 0U &&
        (LPUART_GetStatusFlags(LPUART0) & kLPUART_TxDataRegEmptyFlag) != 0U)
    {
        if (s_txTail != s_txHead)
        {
            LPUART_WriteByte(LPUART0, s_txBuffer[s_txTail]);
            s_txTail = (uint16_t)((s_txTail + 1U) % TX_SIZE);
        }
        else
        {
            LPUART_DisableInterrupts(LPUART0, kLPUART_TxDataRegEmptyInterruptEnable);
        }
    }
    SDK_ISR_EXIT_BARRIER;
}

static bool QueueLine(const char *line)
{
    size_t length = strlen(line);
    uint16_t head = s_txHead;
    size_t freeBytes = (s_txTail + TX_SIZE - head - 1U) % TX_SIZE;
    if (length > freeBytes)
    {
        return false;
    }
    for (size_t i = 0U; i < length; ++i)
    {
        s_txBuffer[head] = (uint8_t)line[i];
        head = (uint16_t)((head + 1U) % TX_SIZE);
    }
    __DMB();
    s_txHead = head;
    LPUART_EnableInterrupts(LPUART0, kLPUART_TxDataRegEmptyInterruptEnable);
    return true;
}

static bool InitializeUart(void)
{
    lpuart_config_t config;
    LPUART_GetDefaultConfig(&config);
    config.baudRate_Bps = 115200U;
    config.parityMode = kLPUART_ParityDisabled;
    config.dataBitsCount = kLPUART_EightDataBits;
    config.stopBitCount = kLPUART_OneStopBit;
    config.enableTx = true;
    config.enableRx = false;
    if (LPUART_Init(LPUART0, &config, 12000000U) != kStatus_Success)
    {
        return false;
    }
    NVIC_SetPriority(LPUART0_IRQn, 2U);
    EnableIRQ(LPUART0_IRQn);
    return true;
}

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
    if (InitializeUart())
    {
        (void)QueueLine("UART lista\r\n");
    }

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
