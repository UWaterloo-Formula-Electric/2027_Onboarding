/**
  *****************************************************************************
  * @file    uartBus.h
  * @brief   Transfer wrappers for the debug UART
  *****************************************************************************
  */

#ifndef UART_BUS_H
#define UART_BUS_H

#include "stm32f7xx_hal.h"

#include "FreeRTOS.h"

// Starts the receive DMA. Call before the scheduler starts.
HAL_StatusTypeDef uartBusInit(void);

// Blocking transmit, serialised against other callers. Not safe from
// interrupt context.
HAL_StatusTypeDef uartBusWrite(const uint8_t *data, uint16_t length);

// Blocking transmit with no locking, for fatal paths and anything running
// before the scheduler exists
void uartBusWritePolled(const uint8_t *data, uint16_t length);

// Waits up to ticksToWait for one received character. Returns pdTRUE if one
// was written to byte.
BaseType_t uartBusReadByte(uint8_t *byte, TickType_t ticksToWait);

#endif /* UART_BUS_H */
