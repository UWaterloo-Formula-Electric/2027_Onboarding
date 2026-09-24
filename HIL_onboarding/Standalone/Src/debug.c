/**
  *****************************************************************************
  * @file    debug.c
  * @brief   Logging and the CLI task, standalone replacement for
  *          common/Src/debug.c
  * @details A stream buffer decouples logging from the wire. Anything that
  * prints formats into it and returns immediately; printTask drains it and
  * hands whole chunks to uartBus, which blocks until they are on the wire.
  * That keeps DEBUG_PRINT cheap enough to call from a control loop, and
  * confines the waiting to the lowest priority task in the system.
  *
  * cliTask blocks on a received character, echoes it, and runs a completed
  * line through the interpreter. It starts with the other tasks, so the CLI
  * is live as soon as the board boots and prints the command list on entry.
  *
  * Before the scheduler starts there is no printTask to drain the buffer, so
  * prints from userInit go straight out over the UART instead.
  *
  * This board has no CAN, so the UART-over-CAN tunnel and heartbeat commands
  * that Inc/bsp.h's DISABLE_CAN_FEATURES compiles out upstream do not exist.
  *****************************************************************************
  */

#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"

#include "FreeRTOS_CLI.h"
#include "uartBus.h"

#define PRINT_STREAM_LENGTH 1024
#define FORMAT_BUFFER_LENGTH 256
#define TRANSMIT_CHUNK_LENGTH 64
#define PRINT_MUTEX_TIMEOUT_MS 10

#define CLI_INPUT_LENGTH 128
#define CLI_PROMPT "> "
#define ASCII_DELETE 0x7F

// How long CLI output waits for printTask to drain before giving up
#define CLI_OUTPUT_TIMEOUT_MS 100

#define CLI_BANNER \
    "\r\nHIL_onboarding standalone, built " __DATE__ " " __TIME__ "\r\n" \
    "Type help to list these again\r\n\r\n"

// FreeRTOS wants a stats counter roughly 10-100x the 1 kHz tick
#define STATS_TIMER_TARGET_HZ 50000U

static StreamBufferHandle_t printStream;
static SemaphoreHandle_t printMutex;
static char formatBuffer[FORMAT_BUFFER_LENGTH];

static HAL_StatusTypeDef registerBuiltinCommands(void);

/* Output ------------------------------------------------------------------- */

// Never blocks, so a log line from a control loop costs almost nothing. The
// trade is that it drops rather than waits when printStream is full.
static void queueBytes(const char *data, size_t length)
{
    if (data == NULL || length == 0) {
        return;
    }

    if (printStream == NULL || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        uartBusWritePolled((const uint8_t *)data, (uint16_t)length);
        return;
    }

    xStreamBufferSend(printStream, data, length, 0);
}

// CLI output waits for printTask to make room instead. A dropped log line is a
// nuisance, but a command's reply silently losing its tail is a bug - and the
// boot banner alone is more than printStream holds.
static void queueBytesBlocking(const char *data, size_t length)
{
    size_t sent = 0;

    if (data == NULL || length == 0) {
        return;
    }

    if (printStream == NULL || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        uartBusWritePolled((const uint8_t *)data, (uint16_t)length);
        return;
    }

    while (sent < length) {
        size_t written = xStreamBufferSend(printStream, &data[sent], length - sent,
                                           pdMS_TO_TICKS(CLI_OUTPUT_TIMEOUT_MS));

        if (written == 0) {
            return;
        }

        sent += written;
    }
}

static size_t clampFormatLength(int formatted, size_t bufferLength)
{
    if (formatted < 0) {
        return 0;
    }

    // vsnprintf returns what it would have written, which can exceed the buffer
    return ((size_t)formatted < bufferLength) ? (size_t)formatted : bufferLength - 1U;
}

void debugPrint(const char *format, ...)
{
    BaseType_t locked = pdFALSE;
    va_list args;
    int formatted;

    if (printMutex != NULL && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        if (xSemaphoreTake(printMutex, pdMS_TO_TICKS(PRINT_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            return;
        }

        locked = pdTRUE;
    }

    va_start(args, format);
    formatted = vsnprintf(formatBuffer, sizeof(formatBuffer), format, args);
    va_end(args);

    queueBytes(formatBuffer, clampFormatLength(formatted, sizeof(formatBuffer)));

    if (locked == pdTRUE) {
        xSemaphoreGive(printMutex);
    }
}

void debugPrintIsr(const char *format, ...)
{
    // Separate from formatBuffer so an interrupt cannot corrupt a task's format
    static char isrFormatBuffer[FORMAT_BUFFER_LENGTH];
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    va_list args;
    size_t length;
    int formatted;

    if (printStream == NULL) {
        return;
    }

    va_start(args, format);
    formatted = vsnprintf(isrFormatBuffer, sizeof(isrFormatBuffer), format, args);
    va_end(args);

    length = clampFormatLength(formatted, sizeof(isrFormatBuffer));

    if (length > 0) {
        xStreamBufferSendFromISR(printStream, isrFormatBuffer, length, &higherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

// Routes printf through the same stream, via Core/Src/syscalls.c's _write
int __io_putchar(int ch)
{
    char byte = (char)ch;

    queueBytes(&byte, 1);
    return ch;
}

void printTask(void const *argument)
{
    uint8_t chunk[TRANSMIT_CHUNK_LENGTH];

    (void)argument;

    while (1) {
        size_t length = xStreamBufferReceive(printStream, chunk, sizeof(chunk), portMAX_DELAY);

        if (length > 0) {
            uartBusWrite(chunk, (uint16_t)length);
        }
    }
}

/* CLI ---------------------------------------------------------------------- */

static void runCommandLine(const char *inputBuffer, char *outputBuffer,
                           size_t outputBufferLength)
{
    BaseType_t moreOutputPending;

    // A handler returning pdTRUE has more to say than one buffer holds
    do {
        moreOutputPending = FreeRTOS_CLIProcessCommand(inputBuffer, outputBuffer,
                                                       outputBufferLength);
        queueBytesBlocking(outputBuffer, strlen(outputBuffer));
    } while (moreOutputPending != pdFALSE);
}

void cliTask(void const *argument)
{
    static char inputBuffer[CLI_INPUT_LENGTH];
    static char outputBuffer[configCOMMAND_INT_MAX_OUTPUT_SIZE];
    size_t inputLength = 0;
    uint8_t received;

    (void)argument;

    // Every command is registered by now - userInit runs debugInit and
    // hilCliInit before the scheduler starts - so running help here lists the
    // real table rather than a copy of it that could drift out of date.
    queueBytesBlocking(CLI_BANNER, strlen(CLI_BANNER));
    runCommandLine("help", outputBuffer, sizeof(outputBuffer));
    queueBytesBlocking(CLI_PROMPT, strlen(CLI_PROMPT));

    while (1) {
        if (uartBusReadByte(&received, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (received == '\r' || received == '\n') {
            queueBytesBlocking("\r\n", 2);

            if (inputLength > 0) {
                inputBuffer[inputLength] = '\0';
                runCommandLine(inputBuffer, outputBuffer, sizeof(outputBuffer));
                inputLength = 0;
            }

            queueBytesBlocking(CLI_PROMPT, strlen(CLI_PROMPT));
        } else if (received == '\b' || received == ASCII_DELETE) {
            if (inputLength > 0) {
                inputLength--;
                queueBytesBlocking("\b \b", 3);
            }
        } else if (received >= ' ' && received < ASCII_DELETE) {
            if (inputLength < sizeof(inputBuffer) - 1U) {
                inputBuffer[inputLength++] = (char)received;
                queueBytesBlocking((const char *)&received, 1);
            }
        }
    }
}

/* Run time stats ----------------------------------------------------------- */

#ifdef STATS_TIM_HANDLE
void configureTimerForRunTimeStats(void)
{
    // TIM7 is on APB1, whose timer clock is 2x PCLK1 for every prescaler this
    // board uses. Only the ratio to the tick matters, the stats are percentages.
    uint32_t timerClock = HAL_RCC_GetPCLK1Freq() * 2U;
    uint32_t prescaler = timerClock / STATS_TIMER_TARGET_HZ;

    __HAL_TIM_SET_PRESCALER(&STATS_TIM_HANDLE, (prescaler > 0U) ? prescaler - 1U : 0U);
    __HAL_TIM_SET_COUNTER(&STATS_TIM_HANDLE, 0);

    if (HAL_TIM_Base_Start(&STATS_TIM_HANDLE) != HAL_OK) {
        ERROR_PRINT("Failed to start the run time stats timer\n");
    }
}

uint32_t getRunTimeCounterValue(void)
{
    // TIM7 is 16 bit with no update interrupt configured, so widen it here.
    // FreeRTOS calls this on every context switch, far more often than the
    // 1.3 s the counter takes to wrap, so no wrap can be missed. The statics
    // are safe because every caller is already in a critical section or an ISR.
    static uint16_t lastCount;
    static uint32_t elapsed;
    uint16_t count = (uint16_t)__HAL_TIM_GET_COUNTER(&STATS_TIM_HANDLE);

    elapsed += (uint16_t)(count - lastCount);
    lastCount = count;

    return elapsed;
}
#else
void configureTimerForRunTimeStats(void)
{
}

uint32_t getRunTimeCounterValue(void)
{
    return 0;
}
#endif /* STATS_TIM_HANDLE */

/* Built in CLI commands ---------------------------------------------------- */

static BaseType_t heapCommand(char *writeBuffer, size_t writeBufferLength,
                              const char *commandString)
{
    (void)commandString;

    COMMAND_OUTPUT("Heap free %lu bytes, minimum ever free %lu bytes\n",
                   (uint32_t)xPortGetFreeHeapSize(),
                   (uint32_t)xPortGetMinimumEverFreeHeapSize());
    return pdFALSE;
}

static const CLI_Command_Definition_t heapCommandDefinition =
{
    "heap",
    "heap:\r\n  Print free and minimum ever free heap\r\n",
    heapCommand,
    0 /* Number of parameters */
};

static BaseType_t taskListCommand(char *writeBuffer, size_t writeBufferLength,
                                  const char *commandString)
{
    (void)commandString;

    // vTaskList takes no length, so configCOMMAND_INT_MAX_OUTPUT_SIZE has to
    // stay well above the ~40 bytes per task it writes
    COMMAND_OUTPUT("Task\t\tState\tPri\tStack\tNum\r\n");
    vTaskList(writeBuffer + strlen(writeBuffer));
    return pdFALSE;
}

static const CLI_Command_Definition_t taskListCommandDefinition =
{
    "taskList",
    "taskList:\r\n  List every task with its state and minimum free stack\r\n",
    taskListCommand,
    0 /* Number of parameters */
};

static BaseType_t statsCommand(char *writeBuffer, size_t writeBufferLength,
                               const char *commandString)
{
    (void)commandString;

    COMMAND_OUTPUT("Task\t\tAbs time\tPercent\r\n");
    vTaskGetRunTimeStats(writeBuffer + strlen(writeBuffer));
    return pdFALSE;
}

static const CLI_Command_Definition_t statsCommandDefinition =
{
    "stats",
    "stats:\r\n  Print per task run time statistics\r\n",
    statsCommand,
    0 /* Number of parameters */
};

static BaseType_t resetCommand(char *writeBuffer, size_t writeBufferLength,
                               const char *commandString)
{
    static const char message[] = "Resetting\r\n";

    (void)writeBuffer;
    (void)writeBufferLength;
    (void)commandString;

    // Written straight to the UART rather than through COMMAND_OUTPUT, because
    // the reset lands before cliTask could queue the output, let alone before
    // printTask could drain it
    uartBusWritePolled((const uint8_t *)message, (uint16_t)(sizeof(message) - 1U));
    NVIC_SystemReset();

    return pdFALSE;
}

static const CLI_Command_Definition_t resetCommandDefinition =
{
    "reset",
    "reset:\r\n  Reset the microcontroller\r\n",
    resetCommand,
    0 /* Number of parameters */
};

static BaseType_t versionCommand(char *writeBuffer, size_t writeBufferLength,
                                 const char *commandString)
{
    (void)commandString;

    COMMAND_OUTPUT("HIL_onboarding standalone build, " __DATE__ " " __TIME__ "\n");
    return pdFALSE;
}

static const CLI_Command_Definition_t versionCommandDefinition =
{
    "version",
    "version:\r\n  Print the firmware build identifier\r\n",
    versionCommand,
    0 /* Number of parameters */
};

static HAL_StatusTypeDef registerBuiltinCommands(void)
{
    static const CLI_Command_Definition_t *const builtins[] = {
        &heapCommandDefinition,
        &taskListCommandDefinition,
        &statsCommandDefinition,
        &resetCommandDefinition,
        &versionCommandDefinition
    };
    size_t index;

    for (index = 0; index < (sizeof(builtins) / sizeof(builtins[0])); index++) {
        if (FreeRTOS_CLIRegisterCommand(builtins[index]) != pdPASS) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

/* Init --------------------------------------------------------------------- */

HAL_StatusTypeDef debugInit(void)
{
    printStream = xStreamBufferCreate(PRINT_STREAM_LENGTH, 1);
    printMutex = xSemaphoreCreateMutex();

    if (printStream == NULL || printMutex == NULL) {
        return HAL_ERROR;
    }

    return registerBuiltinCommands();
}
