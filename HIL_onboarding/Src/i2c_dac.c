/**
  ******************************************************************************
  * @file    i2C_dac.c
  * @brief   MCP4728 I2C DAC driver starter.
  * @details Complete the frame construction and the bus transfer using
  *          MCP4728 datasheet sections 5.6.1 through 5.6.4.
  ******************************************************************************
  */

#include "i2C_dac.h"

#include "i2cBus.h"

#define I2C_DAC_CODE_MAX 0x0FFFU
#define I2C_DAC_BUS I2C_BUS_1
#define I2C_DAC_ADDRESS7 0x60U

typedef struct {
    uint8_t bytes[I2C_DAC_FRAME_MAX_LENGTH];
    uint16_t length;
} I2cDacFrame_t;

static HAL_StatusTypeDef buildOutputFrame(uint16_t code, I2cDacFrame_t *frame)
{
    if (code > I2C_DAC_CODE_MAX || frame == NULL) {
        return HAL_ERROR;
    }

    frame->length = 0;

    // TODO: Construct the MCP4728 command frame, including its channel-selection bits.
    return HAL_ERROR;
}

static HAL_StatusTypeDef transmitFrame(const I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U) {
        return HAL_ERROR;
    }

    // TODO: Send frame->bytes through i2cBus on I2C_DAC_BUS to I2C_DAC_ADDRESS7.
    return HAL_ERROR;
}

static HAL_StatusTypeDef activateDAC(void)
{
    // TODO: To activate the DAC, we need to activate a certain pin, but which one?
    return HAL_ERROR;
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
