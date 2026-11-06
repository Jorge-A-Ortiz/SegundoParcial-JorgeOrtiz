#include "board.h"
#include "app.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_ctimer.h"
#include "fsl_lpuart.h"
#include "fsl_lpadc.h"
#include "monitor.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define TX_SIZE 256U
#define RX_SIZE 256U
#define LINE_SIZE 32U
#define RX_BUDGET 32U
#define STATUS_SIZE 96U

static volatile uint32_t s_milliseconds;
static uint32_t s_lastAdcStart;
static bool s_adcPending;
static monitor_t s_monitor;
static volatile uint8_t s_txBuffer[TX_SIZE];
static volatile uint16_t s_txHead;
static volatile uint16_t s_txTail;
static volatile uint8_t s_rxBuffer[RX_SIZE];
static volatile uint16_t s_rxHead;
static volatile uint16_t s_rxTail;
static volatile bool s_rxFault;
static char s_line[LINE_SIZE + 1U];
static uint8_t s_lineLength;
static bool s_discardLine;
static bool s_pendingCr;

void LPUART0_IRQHandler(void)
{
    uint32_t flags = LPUART_GetStatusFlags(LPUART0);
    uint32_t errors = kLPUART_RxOverrunFlag | kLPUART_FramingErrorFlag |
                      kLPUART_ParityErrorFlag | kLPUART_NoiseErrorFlag;
    if ((flags & errors) != 0U)
    {
        (void)LPUART_ClearStatusFlags(LPUART0, flags & errors);
        s_rxFault = true;
    }
    for (unsigned int i = 0U; i < 16U; ++i)
    {
        if ((LPUART_GetStatusFlags(LPUART0) & kLPUART_RxDataRegFullFlag) == 0U)
        {
            break;
        }
        uint8_t byte = LPUART_ReadByte(LPUART0);
        uint16_t next = (uint16_t)((s_rxHead + 1U) % RX_SIZE);
        if (next == s_rxTail)
        {
            s_rxFault = true;
        }
        else if (!s_rxFault)
        {
            s_rxBuffer[s_rxHead] = byte;
            s_rxHead = next;
        }
    }

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

static size_t TxFree(void)
{
    return (s_txTail + TX_SIZE - s_txHead - 1U) % TX_SIZE;
}

static bool QueueLine(const char *line)
{
    size_t length = strlen(line);
    uint16_t head = s_txHead;
    size_t freeBytes = TxFree();
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

static bool QueueStatus(uint32_t now)
{
    char line[STATUS_SIZE];
    int length = snprintf(line, sizeof(line),
        "t=%lu;ADC=%u;PCT=%u;THR=%u;MODE=%s;ALARM=%u\r\n",
        (unsigned long)now, (unsigned)s_monitor.adc, (unsigned)s_monitor.pct,
        (unsigned)s_monitor.threshold, s_monitor.run ? "RUN" : "PAUSE",
        (unsigned)s_monitor.alarmOn);
    return length > 0 && (size_t)length < sizeof(line) && QueueLine(line);
}

static bool HandleThreshold(const char *line)
{
    if (strncmp(line, "THR ", 4U) != 0 || line[4] == '\0')
    {
        return false;
    }

    unsigned value = 0U;
    for (const char *digit = line + 4; *digit != '\0'; ++digit)
    {
        if (*digit < '0' || *digit > '9')
        {
            return false;
        }
        value = value * 10U + (unsigned)(*digit - '0');
        if (value > 95U)
        {
            return false;
        }
    }

    if (!Monitor_SetThreshold(&s_monitor, value))
    {
        return false;
    }

    GPIO_PinWrite(GPIO3, 12U, s_monitor.alarmOn ? 1U : 0U);
    char reply[16];
    (void)snprintf(reply, sizeof(reply), "OK THR=%u\r\n", value);
    (void)QueueLine(reply);
    return true;
}

static bool HandleMode(const char *line)
{
    bool run = strcmp(line, "START") == 0;
    if (!run && strcmp(line, "STOP") != 0)
    {
        return false;
    }

    Monitor_SetRun(&s_monitor, run);
    GPIO_PinWrite(GPIO3, 12U, s_monitor.alarmOn ? 1U : 0U);
    (void)QueueLine(run ? "OK RUN\r\n" : "OK PAUSE\r\n");
    return true;
}

static void ProcessRxByte(uint8_t byte, uint32_t now)
{
    if (byte == '\n')
    {
        if (s_discardLine || s_lineLength > 0U)
        {
            s_line[s_lineLength] = '\0';
            if (!s_discardLine && strcmp(s_line, "STATUS") == 0)
            {
                (void)QueueStatus(now);
            }
            else if (s_discardLine || (!HandleMode(s_line) && !HandleThreshold(s_line)))
            {
                (void)QueueLine("ERR\r\n");
            }
        }
        s_lineLength = 0U;
        s_discardLine = false;
        s_pendingCr = false;
        return;
    }
    if (s_discardLine)
    {
        return;
    }
    if (s_pendingCr)
    {
        s_pendingCr = false;
        s_discardLine = true;
        return;
    }
    if (byte == '\r')
    {
        s_pendingCr = true;
        return;
    }
    if (s_lineLength >= LINE_SIZE || byte < 32U || byte > 126U)
    {
        s_lineLength = 0U;
        s_discardLine = true;
        return;
    }
    s_line[s_lineLength++] = (char)byte;
}

static void PollRx(uint32_t now)
{
    uint32_t irqState = DisableGlobalIRQ();
    bool fault = s_rxFault;
    if (fault)
    {
        s_rxTail = s_rxHead;
        s_rxFault = false;
    }
    EnableGlobalIRQ(irqState);
    if (fault)
    {
        s_lineLength = 0U;
        s_pendingCr = false;
        s_discardLine = true;
    }

    for (uint8_t count = 0U; count < RX_BUDGET && TxFree() >= STATUS_SIZE; ++count)
    {
        uint16_t tail = s_rxTail;
        if (tail == s_rxHead)
        {
            break;
        }
        uint8_t byte = s_rxBuffer[tail];
        s_rxTail = (uint16_t)((tail + 1U) % RX_SIZE);
        ProcessRxByte(byte, now);
    }
}

static void InitializeAdc(void)
{
    lpadc_config_t config;
    LPADC_GetDefaultConfig(&config);
    config.powerLevelMode = kLPADC_PowerLevelAlt4;
    config.referenceVoltageSource = kLPADC_ReferenceVoltageAlt3;
    config.enableAnalogPreliminary = true;
    LPADC_Init(ADC1, &config);
    LPADC_DoOffsetCalibration(ADC1);
    LPADC_DoAutoCalibration(ADC1);

    lpadc_conv_command_config_t command;
    LPADC_GetDefaultConvCommandConfig(&command);
    command.channelNumber = 8U;
    command.conversionResolutionMode = kLPADC_ConversionResolutionStandard;
    command.sampleTimeMode = kLPADC_SampleTimeADCK131;
    LPADC_SetConvCommandConfig(ADC1, 1U, &command);

    lpadc_conv_trigger_config_t trigger;
    LPADC_GetDefaultConvTriggerConfig(&trigger);
    trigger.targetCommandId = 1U;
    trigger.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(ADC1, 0U, &trigger);
}

static void PollAdc(uint32_t now)
{
    if ((uint32_t)(now - s_lastAdcStart) >= 100U)
    {
        s_lastAdcStart += 100U;
        if (!s_adcPending)
        {
            s_adcPending = true;
            GPIO_PortToggle(GPIO3, 1UL << 0U);
            LPADC_DoSoftwareTrigger(ADC1, 1U);
        }
    }
    if (s_adcPending)
    {
        lpadc_conv_result_t result;
        if (LPADC_GetConvResult(ADC1, &result))
        {
            s_adcPending = false;
            Monitor_UpdateSample(&s_monitor, (uint16_t)((result.convValue >> 3U) & MONITOR_ADC_MAX));
            GPIO_PinWrite(GPIO3, 12U, s_monitor.alarmOn ? 1U : 0U);
        }
    }
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
    config.enableRx = true;
    if (LPUART_Init(LPUART0, &config, 12000000U) != kStatus_Success)
    {
        return false;
    }
    LPUART_EnableInterrupts(LPUART0, kLPUART_RxDataRegFullInterruptEnable |
        kLPUART_RxOverrunInterruptEnable | kLPUART_FramingErrorInterruptEnable |
        kLPUART_ParityErrorInterruptEnable | kLPUART_NoiseErrorInterruptEnable);
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
    Monitor_Init(&s_monitor);
    InitializeTimer();
    InitializeAdc();
    bool uartReady = InitializeUart();
    uint32_t lastTelemetry = 0U;

    uint32_t lastActivityChange = 0U;
    bool activityOn = false;

    while (1)
    {
        uint32_t now = s_milliseconds;
        PollAdc(now);
        if (Monitor_PollButton(&s_monitor, now, GPIO_PinRead(GPIO0, 6U) == 0U))
        {
            GPIO_PinWrite(GPIO3, 12U, s_monitor.alarmOn ? 1U : 0U);
        }

        if (uartReady)
        {
            if ((uint32_t)(now - lastTelemetry) >= 1000U && QueueStatus(now))
            {
                lastTelemetry += 1000U;
            }
            PollRx(now);
        }

        while ((uint32_t)(now - lastActivityChange) >= 500U)
        {
            lastActivityChange += 500U;
            activityOn = !activityOn;
            GPIO_PinWrite(GPIO3, 13U, activityOn ? 1U : 0U);
        }
    }
}
