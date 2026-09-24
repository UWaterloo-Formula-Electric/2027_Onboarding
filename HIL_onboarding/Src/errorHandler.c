/**
  *****************************************************************************
  * @file    errorHandler.c
  * @brief   Fatal error handler
  * @details Prints the failing file and line straight to the debug UART and
  * stops. The UART is re-initialised first because the print task may have
  * been holding it when the error happened, and interrupts are disabled so
  * nothing else can run.
  *****************************************************************************
  */

#include "errorHandler.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp.h"

#define ERROR_UART_TIMEOUT_MS 1000

void _handleError(char *file, int line)
{
    char lineNumber[12];

    taskDISABLE_INTERRUPTS();

    HAL_GPIO_WritePin(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_PIN_SET);

    HAL_UART_DeInit(&DEBUG_UART_HANDLE);
    HAL_UART_Init(&DEBUG_UART_HANDLE);

    snprintf(lineNumber, sizeof(lineNumber), " line %d\n", line);

    HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)"Error!: File ",
                      strlen("Error!: File "), ERROR_UART_TIMEOUT_MS);
    HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)file,
                      strlen(file), ERROR_UART_TIMEOUT_MS);
    HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)lineNumber,
                      strlen(lineNumber), ERROR_UART_TIMEOUT_MS);

    while (1) {
    }
}
