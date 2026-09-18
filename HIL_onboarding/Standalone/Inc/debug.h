/**
  *****************************************************************************
  * @file    debug.h
  * @brief   Print macros and debug UART plumbing, standalone replacement for
  *          common/Inc/debug.h
  * @details Provides the same API the board code already calls: the print
  * macros, debugInit, uartStartReceiving, and the printTask/cliTask bodies
  * CubeMX declares extern in Core/Src/freertos.c. See Standalone/README.md.
  *
  * DEBUG_PRINT and ERROR_PRINT expand to the same call, exactly as upstream -
  * the severity split is a convention, not a compiler-enforced one.
  *****************************************************************************
  */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#include "bsp.h"
#include "errorHandler.h"

// common/Inc/generalErrorHandler.h's macro, which Src/errorHandler.c backs
#define handleError() _handleError(__FILE__, __LINE__)

// Creates the print stream and registers the built in CLI commands. The UART
// itself belongs to uartBus.h, which userInit brings up separately.
HAL_StatusTypeDef debugInit(void);

void printTask(void const *argument);
void cliTask(void const *argument);

void debugPrint(const char *format, ...);
void debugPrintIsr(const char *format, ...);

#ifdef DEBUG_ON
#define DEBUG_PRINT(...) debugPrint(__VA_ARGS__)
#define DEBUG_PRINT_ISR(...) debugPrintIsr(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#define DEBUG_PRINT_ISR(...) ((void)0)
#endif

#ifdef ERROR_PRINT_ON
#define ERROR_PRINT(...) debugPrint(__VA_ARGS__)
#define ERROR_PRINT_ISR(...) debugPrintIsr(__VA_ARGS__)
#else
#define ERROR_PRINT(...) ((void)0)
#define ERROR_PRINT_ISR(...) ((void)0)
#endif

#ifdef CONSOLE_PRINT_ON
#define CONSOLE_PRINT(...) debugPrint(__VA_ARGS__)
#else
#define CONSOLE_PRINT(...) ((void)0)
#endif

// Overwrites writeBuffer on every call, so a handler that needs more than one
// line must return pdTRUE to be re-invoked. Relies on writeBuffer and
// writeBufferLength being the CLI handler's own parameter names, as upstream.
#define COMMAND_OUTPUT(...) snprintf(writeBuffer, writeBufferLength, __VA_ARGS__)

#endif /* DEBUG_H */
