/**
  ******************************************************************************
  * @file    i2C_dac.c
  * @brief   MCP4728 I2C DAC driver for outputs A and B.
  * @details Writes both input registers over I2C1, then pulses LDAC to update
  *          their outputs together. See MCP4728 datasheet section 5.6.2.
  ******************************************************************************
  */

#include "i2C_dac.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug.h"
#include "i2c.h"
#include "i2cBus.h"

#define I2C_DAC_TIMEOUT_MS 15

// Datasheet Table 1-2 asks for 210 ns minimum, and specifies no maximum, so
// one tick is the shortest thing the scheduler can express and is plenty
#define T_LDAC_MS 1

// I2C_DAC_CODE_MAX and I2C_DAC_FULL_SCALE_MV live in i2C_dac.h, since the CLI
// validates against them too
#define I2C_DAC_BUS I2C_BUS_1

#define MCP4728_A2 0U
#define MCP4728_A1 0U
#define MCP4728_A0 0U
#define I2C_DAC_ADDRESS7 (0x60U | (MCP4728_A2 << 2U) | (MCP4728_A1 << 1U) | MCP4728_A0)

#define MCP4728_C2 0U
#define MCP4728_C1 1U
#define MCP4728_C0 0U
#define MCP4728_W1 0U
#define MCP4728_W0 0U
#define MCP4728_CHANNEL_A 0U
#define MCP4728_CHANNEL_B 1U
#define MCP4728_UDAC 1U
#define MCP4728_VREF 0U
#define MCP4728_PD1 0U
#define MCP4728_PD0 0U
#define MCP4728_GX 0U

typedef struct {
    uint8_t bytes[I2C_DAC_FRAME_MAX_LENGTH];
    uint16_t length;
} I2cDacFrame_t;

static HAL_StatusTypeDef buildOutputFrame(uint16_t code, I2cDacFrame_t *frame)
{
    if (code > I2C_DAC_CODE_MAX || frame == NULL) {
        return HAL_ERROR;
    }

    frame->length = 0U;

    // Each channel uses command, configuration/high nibble, then low byte.
    // HAL sends the address separately: two channels need six payload bytes.
    const uint8_t command = (MCP4728_C2 << 7U) | (MCP4728_C1 << 6U) |
                            (MCP4728_C0 << 5U) | (MCP4728_W1 << 4U) |
                            (MCP4728_W0 << 3U) | MCP4728_UDAC;
    const uint8_t high = (MCP4728_VREF << 7U) | (MCP4728_PD1 << 6U) |
                         (MCP4728_PD0 << 5U) | (MCP4728_GX << 4U) |
                         ((code >> 8U) & 0x0FU);
    const uint8_t low = code & 0xFFU;

    frame->bytes[0] = command | (MCP4728_CHANNEL_A << 1U);
    frame->bytes[1] = high;
    frame->bytes[2] = low;
    frame->bytes[3] = command | (MCP4728_CHANNEL_B << 1U);
    frame->bytes[4] = high;
    frame->bytes[5] = low;
    frame->length = 6U;
    return HAL_OK;
}

static HAL_StatusTypeDef transmitFrame(const I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U ||
        frame->length > I2C_DAC_FRAME_MAX_LENGTH) {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(&hi2c1, I2C_DAC_ADDRESS7 << 1U,
                                   (uint8_t *)frame->bytes, frame->length,
                                   I2C_DAC_TIMEOUT_MS);
}

static HAL_StatusTypeDef activateDAC(void)
{
    // UDAC keeps the writes pending until this active-low strobe. Round up
    // to at least one scheduler tick so the pulse cannot collapse to zero.
    TickType_t holdTicks = pdMS_TO_TICKS(T_LDAC_MS);
    if (holdTicks == 0U) {
        holdTicks = 1U;
    }

    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);
    vTaskDelay(holdTicks);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);

    return HAL_OK;
}

HAL_StatusTypeDef i2cDacInit(void)
{
    // Keep LDAC inactive so both channel writes defer until activateDAC.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);

    return HAL_OK;
}

HAL_StatusTypeDef i2cDacProbe(void)
{
    // i2cBus wants the 7 bit address already shifted up for the R/W bit
    return i2cIsDeviceReady(I2C_DAC_BUS, I2C_DAC_ADDRESS7 << 1U);
}

HAL_StatusTypeDef i2cDacSetMillivolts(uint16_t millivolts)
{
    uint32_t code;

    if (millivolts >= I2C_DAC_FULL_SCALE_MV) {
        return HAL_ERROR;
    }

    code = ((uint32_t)millivolts * I2C_DAC_CODE_COUNT) / I2C_DAC_FULL_SCALE_MV;

    return i2cDacSetCode((uint16_t)code);
}

HAL_StatusTypeDef i2cDacSetCode(uint16_t code)
{
    I2cDacFrame_t frame;

    if (buildOutputFrame(code, &frame) != HAL_OK) {
        return HAL_ERROR;
    }

    if (transmitFrame(&frame) != HAL_OK) {
        return HAL_ERROR;
    }

    if (activateDAC() != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}
