/**
  *****************************************************************************
  * @file    uartBus.c
  * @brief   Transfer wrappers for the debug UART
  * @details UART4 on PH13/PH14 is the debug link. This follows what
  * common/Src/debug.c does on the other boards, because it is known good on
  * this hardware:
  *
  * Transmit is a plain blocking HAL_UART_Transmit. DMA is deliberately not
  * used. A one-shot DMA transmit only reports completion through the UART's TC
  * interrupt, so any hiccup there leaves gState stuck at BUSY_TX and every
  * later write returns HAL_BUSY - output stops dead after a few characters.
  * The data also has to stay put for the whole transfer, which a completion
  * semaphore can get wrong if a timed-out transfer completes late. At 115200
  * baud a 64 byte chunk blocks printTask for about 5 ms, which is why printTask
  * is the lowest priority task in the system.
  *
  * Receive is a single byte circular DMA. It never needs re-arming, and
  * HAL_UART_RxCpltCallback fires per character straight into receiveStream, so
  * the CLI can block rather than poll.
  *
  * Everything in this file that touches HAL_UART_* belongs here - debug.c and
  * the CLI go through uartBus.h.
  *****************************************************************************
  */

#include "uartBus.h"

#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"

#include "bsp.h"
#include "debug.h"

// Matches common/Inc/debug.h's UART_PRINT_TIMEOUT
#define UART_TRANSMIT_TIMEOUT_MS 100
#define UART_TRANSMIT_MUTEX_TIMEOUT_MS 100

#define UART_RECEIVE_STREAM_LENGTH 128

static uint8_t receiveByte;
static StreamBufferHandle_t receiveStream;
static SemaphoreHandle_t transmitMutex;

HAL_StatusTypeDef uartBusInit(void)
{
    receiveStream = xStreamBufferCreate(UART_RECEIVE_STREAM_LENGTH, 1);
    transmitMutex = xSemaphoreCreateMutex();

    if (receiveStream == NULL || transmitMutex == NULL) {
        return HAL_ERROR;
    }

    if (DEBUG_UART_HANDLE.hdmarx == NULL) {
        ERROR_PRINT("Failed to init uartBus, no RX DMA linked to the debug UART\n");
        return HAL_ERROR;
    }

    // Both of these break receive silently, so say so rather than just going
    // deaf. Warnings, not failures - transmit still works, and a board that
    // boots and prints the reason beats one that sits in Error_Handler.

    // A one byte receive in Normal mode delivers exactly one character and then
    // stops, leaving the CLI deaf after the first keystroke
    if (DEBUG_UART_HANDLE.hdmarx->Init.Mode != DMA_CIRCULAR) {
        ERROR_PRINT("RX DMA is not Circular, set it to Circular in CubeMX\n");
    }

    // With the FIFO on, the controller holds bytes until the threshold is
    // reached before bursting them to memory. The transfer below is one byte,
    // so a threshold of four words can never be met and nothing is ever
    // written. bmu, vcu and pdu all run this transfer in Direct mode.
    if (DEBUG_UART_HANDLE.hdmarx->Init.FIFOMode != DMA_FIFOMODE_DISABLE) {
        ERROR_PRINT("RX DMA has the FIFO enabled, set Direct mode in CubeMX "
                    "or no character will ever arrive\n");
    }

    // Deliberately NOT enabling UART4_IRQn. HAL_UART_Receive_DMA always arms
    // PEIE and EIE, so with the NVIC line enabled every framing or noise error
    // on the RX pin becomes an interrupt. HAL services those through its abort
    // path, which takes __HAL_LOCK on the handle - a steady stream of them
    // either starves the tasks or leaves the handle locked, after which every
    // HAL_UART_Transmit returns HAL_BUSY and the console goes silent for good.
    // vcu and pdu run this exact peripheral and stream with the line disabled;
    // the flags just sit unread and the circular DMA keeps going regardless.

    // Drop anything the line collected before we were listening, otherwise the
    // first read can come back as an overrun
    __HAL_UART_FLUSH_DRREGISTER(&DEBUG_UART_HANDLE);

    return HAL_UART_Receive_DMA(&DEBUG_UART_HANDLE, &receiveByte, 1);
}

/* Transmit ----------------------------------------------------------------- */

void uartBusWritePolled(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0) {
        return;
    }

    HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)data, length,
                      UART_TRANSMIT_TIMEOUT_MS);
}

HAL_StatusTypeDef uartBusWrite(const uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status;

    if (data == NULL || length == 0) {
        return HAL_ERROR;
    }

    if (transmitMutex == NULL || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        uartBusWritePolled(data, length);
        return HAL_OK;
    }

    if (xSemaphoreTake(transmitMutex,
                       pdMS_TO_TICKS(UART_TRANSMIT_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return HAL_TIMEOUT;
    }

    status = HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)data, length,
                               UART_TRANSMIT_TIMEOUT_MS);

    xSemaphoreGive(transmitMutex);
    return status;
}

/* Receive ------------------------------------------------------------------ */

BaseType_t uartBusReadByte(uint8_t *byte, TickType_t ticksToWait)
{
    if (byte == NULL || receiveStream == NULL) {
        return pdFALSE;
    }

    return (xStreamBufferReceive(receiveStream, byte, 1, ticksToWait) == 1) ? pdTRUE : pdFALSE;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (uart->Instance != DEBUG_UART_HANDLE.Instance) {
        return;
    }

    // The DMA is circular over a single byte, so it has already restarted
    xStreamBufferSendFromISR(receiveStream, &receiveByte, 1, &higherPriorityTaskWoken);

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

