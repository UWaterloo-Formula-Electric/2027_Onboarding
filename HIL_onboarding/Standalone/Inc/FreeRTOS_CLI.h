/**
  *****************************************************************************
  * @file    FreeRTOS_CLI.h
  * @brief   Command line interpreter, standalone replacement for
  *          common/Src/FreeRTOS_CLI.c's header
  * @details A from-scratch implementation of the FreeRTOS-Plus-CLI interface
  * that Src/hilCli.c already builds against. The public names keep
  * FreeRTOS-Plus-CLI's Hungarian notation so the board code compiles
  * unchanged and so a later swap back to the real component is a no-op; the
  * implementation internals follow this repo's style instead.
  *
  * Registration uses a fixed array rather than upstream's pvPortMalloc'd
  * linked list, so registering a command cannot fail on a fragmented heap.
  *****************************************************************************
  */

#ifndef FREERTOS_CLI_H
#define FREERTOS_CLI_H

#include <stddef.h>

#include "FreeRTOS.h"

// Size of the buffer a command handler writes its output into
#ifndef configCOMMAND_INT_MAX_OUTPUT_SIZE
#define configCOMMAND_INT_MAX_OUTPUT_SIZE 1024
#endif

#define CLI_MAX_COMMANDS 24

typedef BaseType_t (*pdCOMMAND_LINE_CALLBACK)(char *writeBuffer,
                                              size_t writeBufferLength,
                                              const char *commandString);

typedef struct xCOMMAND_LINE_INPUT {
    const char *pcCommand;
    const char *pcHelpString;
    const pdCOMMAND_LINE_CALLBACK pxCommandInterpreter;
    int8_t cExpectedNumberOfParameters;
} CLI_Command_Definition_t;

// Returns pdPASS, or pdFAIL if the definition is malformed or the table is full
BaseType_t FreeRTOS_CLIRegisterCommand(const CLI_Command_Definition_t *commandToRegister);

// Returns pdTRUE if the same commandString must be passed in again for more output
BaseType_t FreeRTOS_CLIProcessCommand(const char *commandString, char *writeBuffer,
                                      size_t writeBufferLength);

// Returns a pointer into commandString, not a copy, or NULL if absent
const char *FreeRTOS_CLIGetParameter(const char *commandString, UBaseType_t wantedParameter,
                                     BaseType_t *parameterStringLength);

#endif /* FREERTOS_CLI_H */
