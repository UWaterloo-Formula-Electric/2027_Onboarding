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

#define MCP4728_A2 0U
#define MCP4728_A1 0U
#define MCP4728_A0 0U
#define I2C_DAC_ADDRESS7 (0x60U | (MCP4728_A2 << 2U) | (MCP4728_A1 << 1U) | MCP4728_A0)

#define MCP4728_C2 0U
#define MCP4728_C1 1U
#define MCP4728_C0 0U
#define MCP4728_W1 0U
#define MCP4728_W0 0U
#define MCP4728_DAC1 ???
#define MCP4728_DAC0 ???
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

    // TODO: Construct an MCP4728 Multi-Write frame from the named fields and code. 
    // Look at the top for a bunch of defined variables.
    //Analog voltage is mapped to code by taking the equation V_out = (code / 4096) * V_ref. V_ref is 2.048V. 
    //D11:D0 is essentially your code. That is what you are setting.

    frame->bytes[0];
    frame->bytes[1];
    frame->bytes[2];
    frame->bytes[3];
    frame->bytes[4];
    frame->bytes[5];
    frame->bytes[6];
    frame->length = 7U;
    return HAL_ERROR;
}

static HAL_StatusTypeDef transmitFrame(const I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U) {
        return HAL_ERROR;
    }

    // TODO: Send frame->bytes through i2cBus on I2C_DAC_BUS to I2C_DAC_ADDRESS7.
    //use hi2c1 to send the frame
    //TIP: look up HAL_I2C_Master_Transmit.
    return HAL_ERROR;
}

static HAL_StatusTypeDef activateDAC(void)
{
    // TODO: To activate the DAC, we need to turn off PF10. First you need to go into CubeMX and enable the GPIO for PF10 though.
    //TIP: look up HAL_GPIO_WritePin.
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
