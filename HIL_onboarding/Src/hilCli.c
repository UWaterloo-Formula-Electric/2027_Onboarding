/**
  *****************************************************************************
  * @file    hilCli.c
  * @brief   HIL's own CLI commands
  * @details Raw bus pokes for bench bring-up: scan an I2C bus, and read or
  * write a single device register. These are deliberately device agnostic, so
  * a chip can be exercised before any driver exists for it. A driver for a
  * specific part should add its own higher level commands here, next to these.
  *
  * The other boards keep their CLI in controlStateMachine_mock.c because it
  * hangs off their state machine mock. HIL has no state machine, hence the
  * different file name.
  *****************************************************************************
  */

#include "hilCli.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_CLI.h"
#include "debug.h"
#include "i2cBus.h"
#include "i2C_dac.h"

// Valid 7 bit range, excluding the reserved blocks at each end
#define I2C_SCAN_FIRST_ADDRESS 0x08
#define I2C_SCAN_LAST_ADDRESS 0x77

// 16 steps across the range, coarse enough to watch on a scope or the VCU's ADC
#define I2C_DAC_RAMP_STEP 0x100U
#define I2C_DAC_RAMP_MAX_STEP_MS 5000U

static HAL_StatusTypeDef getBusFromParam(const char *param, I2cBus_t *bus)
{
    unsigned int busNumber = 0;

    if (param == NULL || sscanf(param, "%u", &busNumber) != 1) {
        return HAL_ERROR;
    }

    if (busNumber < 1 || busNumber > NUM_I2C_BUSES) {
        return HAL_ERROR;
    }

    *bus = (I2cBus_t)(busNumber - 1);
    return HAL_OK;
}

static HAL_StatusTypeDef getHexFromParam(const char *param, unsigned int max, unsigned int *value)
{
    if (param == NULL || sscanf(param, "%x", value) != 1) {
        return HAL_ERROR;
    }

    return (*value <= max) ? HAL_OK : HAL_ERROR;
}

static BaseType_t i2cScanCommand(char *writeBuffer, size_t writeBufferLength,
                                 const char *commandString)
{
    static unsigned int address = I2C_SCAN_FIRST_ADDRESS;
    BaseType_t paramLen;
    I2cBus_t bus;

    if (getBusFromParam(FreeRTOS_CLIGetParameter(commandString, 1, &paramLen), &bus) != HAL_OK) {
        address = I2C_SCAN_FIRST_ADDRESS;
        COMMAND_OUTPUT("Bus must be between 1 and %d\n", NUM_I2C_BUSES);
        return pdFALSE;
    }

    // One address per invocation, since the CLI output buffer only holds one line
    while (address <= I2C_SCAN_LAST_ADDRESS) {
        unsigned int current = address++;

        if (i2cIsDeviceReady(bus, current << 1) == HAL_OK) {
            COMMAND_OUTPUT("Found device at 0x%02X\n", current);
            return pdTRUE;
        }
    }

    address = I2C_SCAN_FIRST_ADDRESS;
    COMMAND_OUTPUT("Scan complete\n");
    return pdFALSE;
}

static const CLI_Command_Definition_t i2cScanCommandDefinition =
{
    "i2cScan",
    "i2cScan <bus>:\r\n  Scan I2C <bus> (1-3) and list the 7 bit addresses that respond\r\n",
    i2cScanCommand,
    1 /* Number of parameters */
};

static HAL_StatusTypeDef getDecimalFromParam(const char *param, unsigned int max,
                                             unsigned int *value)
{
    if (param == NULL || sscanf(param, "%u", value) != 1) {
        return HAL_ERROR;
    }

    return (*value <= max) ? HAL_OK : HAL_ERROR;
}

static BaseType_t i2cDacProbeCommand(char *writeBuffer, size_t writeBufferLength,
                                     const char *commandString)
{
    (void)commandString;

    if (i2cDacProbe() != HAL_OK) {
        COMMAND_OUTPUT("No response from the DAC, check wiring, pull ups and bus number\n");
        return pdFALSE;
    }

    COMMAND_OUTPUT("DAC acknowledged\n");
    return pdFALSE;
}

static const CLI_Command_Definition_t i2cDacProbeCommandDefinition =
{
    "i2cDacProbe",
    "i2cDacProbe:\r\n  Check that the MCP4728 acknowledges on its configured bus\r\n",
    i2cDacProbeCommand,
    0 /* Number of parameters */
};

static BaseType_t i2cDacSetVoltageCommand(char *writeBuffer, size_t writeBufferLength,
                                          const char *commandString)
{
    BaseType_t paramLen;
    unsigned int millivolts;

    if (getDecimalFromParam(FreeRTOS_CLIGetParameter(commandString, 1, &paramLen),
                            I2C_DAC_FULL_SCALE_MV - 1U, &millivolts) != HAL_OK) {
        COMMAND_OUTPUT("Voltage must be in millivolts, 0 to %u\n", I2C_DAC_FULL_SCALE_MV - 1U);
        return pdFALSE;
    }

    if (i2cDacSetMillivolts((uint16_t)millivolts) != HAL_OK) {
        COMMAND_OUTPUT("DAC write failed\n");
        return pdFALSE;
    }

    COMMAND_OUTPUT("Set DAC output to %u mV\n", millivolts);
    return pdFALSE;
}

static const CLI_Command_Definition_t i2cDacSetVoltageCommandDefinition =
{
    "i2cDacSetVoltage",
    "i2cDacSetVoltage <mV>:\r\n"
    "  Set the MCP4728 output to a voltage in millivolts\r\n",
    i2cDacSetVoltageCommand,
    1 /* Number of parameters */
};

static BaseType_t i2cDacRampCommand(char *writeBuffer, size_t writeBufferLength,
                                    const char *commandString)
{
    // One step per invocation, so the sweep shows up a line at a time
    static unsigned int code = 0;
    BaseType_t paramLen;
    unsigned int stepMs;

    if (getDecimalFromParam(FreeRTOS_CLIGetParameter(commandString, 1, &paramLen),
                            I2C_DAC_RAMP_MAX_STEP_MS, &stepMs) != HAL_OK) {
        code = 0;
        COMMAND_OUTPUT("Step must be a dwell in ms, 0 to %u\n", I2C_DAC_RAMP_MAX_STEP_MS);
        return pdFALSE;
    }

    if (i2cDacSetCode((uint16_t)code) != HAL_OK) {
        code = 0;
        COMMAND_OUTPUT("DAC write failed\n");
        return pdFALSE;
    }

    vTaskDelay(pdMS_TO_TICKS(stepMs));

    COMMAND_OUTPUT("Code 0x%03X\n", code);
    code += I2C_DAC_RAMP_STEP;

    if (code > I2C_DAC_CODE_MAX) {
        code = 0;
        return pdFALSE;
    }

    return pdTRUE;
}

static const CLI_Command_Definition_t i2cDacRampCommandDefinition =
{
    "i2cDacRamp",
    "i2cDacRamp <ms>:\r\n"
    "  Sweep the MCP4728 from zero to full scale, dwelling <ms> at each step\r\n",
    i2cDacRampCommand,
    1 /* Number of parameters */
};

static BaseType_t i2cDacSetCodeCommand(char *writeBuffer, size_t writeBufferLength,
                                       const char *commandString)
{
    BaseType_t paramLen;
    unsigned int code;

    if (getHexFromParam(FreeRTOS_CLIGetParameter(commandString, 1, &paramLen),
                        I2C_DAC_CODE_MAX, &code) != HAL_OK) {
        COMMAND_OUTPUT("Code must be a 12 bit hex value, 0x000 to 0xFFF\n");
        return pdFALSE;
    }

    if (i2cDacSetCode((uint16_t)code) != HAL_OK) {
        COMMAND_OUTPUT("DAC write failed\n");
        return pdFALSE;
    }

    COMMAND_OUTPUT("Set DAC output to code 0x%03X\n", code);
    return pdFALSE;
}

static const CLI_Command_Definition_t i2cDacSetCodeCommandDefinition =
{
    "i2cDacSetCode",
    "i2cDacSetCode <code>:\r\n"
    "  Set the configured MCP4728 output to a 12 bit hexadecimal code\r\n",
    i2cDacSetCodeCommand,
    1 /* Number of parameters */
};

HAL_StatusTypeDef hilCliInit(void)
{
    static const CLI_Command_Definition_t *const commands[] = {
        &i2cScanCommandDefinition,
        &i2cDacProbeCommandDefinition,
        &i2cDacSetCodeCommandDefinition,
        &i2cDacSetVoltageCommandDefinition,
        &i2cDacRampCommandDefinition
    };
    size_t index;

    for (index = 0; index < (sizeof(commands) / sizeof(commands[0])); index++) {
        if (FreeRTOS_CLIRegisterCommand(commands[index]) != pdPASS) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}
