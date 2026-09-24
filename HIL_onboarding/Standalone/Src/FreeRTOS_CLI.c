/**
  *****************************************************************************
  * @file    FreeRTOS_CLI.c
  * @brief   Command line interpreter, standalone replacement for
  *          common/Src/FreeRTOS_CLI.c
  * @details Command lookup, parameter extraction and the built in help
  * command. See FreeRTOS_CLI.h for why the public names are not camelCase.
  *****************************************************************************
  */

#include "FreeRTOS_CLI.h"

#include <stdio.h>
#include <string.h>

#define CLI_UNRECOGNISED_MESSAGE \
    "Command not recognised. Enter 'help' to see a list of available commands.\r\n"
#define CLI_BAD_PARAMETERS_MESSAGE \
    "Incorrect command parameter(s). Enter 'help' to see a list of available commands.\r\n"

static BaseType_t helpCommand(char *writeBuffer, size_t writeBufferLength,
                              const char *commandString);

static const CLI_Command_Definition_t helpCommandDefinition =
{
    "help",
    "help:\r\n  Lists all the registered commands\r\n",
    helpCommand,
    0 /* Number of parameters */
};

static const CLI_Command_Definition_t *registeredCommands[CLI_MAX_COMMANDS];
static UBaseType_t registeredCommandCount;

static UBaseType_t countParameters(const char *commandString)
{
    UBaseType_t parameters = 0;
    BaseType_t inWord = pdFALSE;

    // The command name itself is the first word, so it is not counted
    while (*commandString != '\0' && *commandString != ' ') {
        commandString++;
    }

    while (*commandString != '\0') {
        if (*commandString == ' ') {
            inWord = pdFALSE;
        } else if (inWord == pdFALSE) {
            inWord = pdTRUE;
            parameters++;
        }

        commandString++;
    }

    return parameters;
}

static BaseType_t helpCommand(char *writeBuffer, size_t writeBufferLength,
                              const char *commandString)
{
    static UBaseType_t nextCommand = 0;

    (void)commandString;

    if (nextCommand < registeredCommandCount) {
        snprintf(writeBuffer, writeBufferLength, "%s", registeredCommands[nextCommand]->pcHelpString);
        nextCommand++;
    }

    if (nextCommand >= registeredCommandCount) {
        nextCommand = 0;
        return pdFALSE;
    }

    return pdTRUE;
}

BaseType_t FreeRTOS_CLIRegisterCommand(const CLI_Command_Definition_t *commandToRegister)
{
    if (commandToRegister == NULL || commandToRegister->pcCommand == NULL ||
        commandToRegister->pcHelpString == NULL ||
        commandToRegister->pxCommandInterpreter == NULL) {
        return pdFAIL;
    }

    // help is always first in the table, so it is also first in its own output
    if (registeredCommandCount == 0) {
        registeredCommands[registeredCommandCount++] = &helpCommandDefinition;
    }

    if (registeredCommandCount >= CLI_MAX_COMMANDS) {
        return pdFAIL;
    }

    registeredCommands[registeredCommandCount++] = commandToRegister;
    return pdPASS;
}

const char *FreeRTOS_CLIGetParameter(const char *commandString, UBaseType_t wantedParameter,
                                     BaseType_t *parameterStringLength)
{
    const char *parameter = NULL;
    UBaseType_t parametersFound = 0;

    if (parameterStringLength != NULL) {
        *parameterStringLength = 0;
    }

    if (commandString == NULL || wantedParameter == 0) {
        return NULL;
    }

    while (parametersFound < wantedParameter) {
        while (*commandString != '\0' && *commandString != ' ') {
            commandString++;
        }

        while (*commandString == ' ') {
            commandString++;
        }

        if (*commandString == '\0') {
            break;
        }

        parametersFound++;

        if (parametersFound == wantedParameter) {
            const char *end = commandString;

            while (*end != '\0' && *end != ' ') {
                end++;
            }

            parameter = commandString;

            if (parameterStringLength != NULL) {
                *parameterStringLength = (BaseType_t)(end - commandString);
            }
        }
    }

    return parameter;
}

BaseType_t FreeRTOS_CLIProcessCommand(const char *commandString, char *writeBuffer,
                                      size_t writeBufferLength)
{
    // Held across calls so a handler returning pdTRUE keeps the same command
    static const CLI_Command_Definition_t *activeCommand = NULL;
    BaseType_t moreOutputPending;

    if (commandString == NULL || writeBuffer == NULL || writeBufferLength == 0) {
        return pdFALSE;
    }

    if (activeCommand == NULL) {
        const CLI_Command_Definition_t *matched = NULL;
        UBaseType_t index;

        for (index = 0; index < registeredCommandCount; index++) {
            size_t commandLength = strlen(registeredCommands[index]->pcCommand);

            if (strncmp(registeredCommands[index]->pcCommand, commandString, commandLength) == 0 &&
                (commandString[commandLength] == ' ' || commandString[commandLength] == '\0')) {
                matched = registeredCommands[index];
                break;
            }
        }

        if (matched == NULL) {
            snprintf(writeBuffer, writeBufferLength, "%s", CLI_UNRECOGNISED_MESSAGE);
            return pdFALSE;
        }

        // A negative count means the command takes a variable number
        if (matched->cExpectedNumberOfParameters >= 0 &&
            countParameters(commandString) != (UBaseType_t)matched->cExpectedNumberOfParameters) {
            snprintf(writeBuffer, writeBufferLength, "%s", CLI_BAD_PARAMETERS_MESSAGE);
            return pdFALSE;
        }

        activeCommand = matched;
    }

    writeBuffer[0] = '\0';
    moreOutputPending = activeCommand->pxCommandInterpreter(writeBuffer, writeBufferLength,
                                                            commandString);

    if (moreOutputPending == pdFALSE) {
        activeCommand = NULL;
    }

    return moreOutputPending;
}
