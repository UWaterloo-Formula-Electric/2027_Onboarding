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
#define MCP4728_DAC1_A 0U 
#define MCP4728_DAC0_A 0U
#define MCP4728_DAC1_B 0U 
#define MCP4728_DAC0_B 1U
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


    frame->bytes[0] = I2C_DAC_ADDRESS7;
    frame->bytes[1] = (MCP4728_C2 << 7) | (MCP4728_C1 << 6) | (MCP4728_C0 << 5) | (MCP4728_W1 << 4) | 
                    (MCP4728_W0 << 3) | (MCP4728_DAC1_A << 2) | (MCP4728_DAC0_A << 1) | MCP4728_UDAC;
    frame->bytes[2] = (MCP4728_VREF << 7) | (MCP4728_PD1 << 6) | (MCP4728_PD0 << 5) | (MCP4728_GX << 4)   | (code >> 8);
    frame->bytes[3] = code;
    frame->bytes[4] = (MCP4728_C2 << 7) | (MCP4728_C1 << 6) | (MCP4728_C0 << 5) | (MCP4728_W1 << 4) | 
                    (MCP4728_W0 << 3) | (MCP4728_DAC1_B << 2) | (MCP4728_DAC0_B << 1) | MCP4728_UDAC;
    frame->bytes[5] = (MCP4728_VREF << 7) | (MCP4728_PD1 << 6) | (MCP4728_PD0 << 5) | (MCP4728_GX << 4)   | (code >> 8);
    frame->bytes[6] = code;
    frame->length = 7U;
    return HAL_OK;
}

static HAL_StatusTypeDef transmitFrame(I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U) {
        return HAL_ERROR;
    }
    return i2cWrite(I2C_DAC_BUS, I2C_DAC_ADDRESS7 << 1U, &frame->bytes[1], frame->length-1);
}

static HAL_StatusTypeDef activateDAC(void)
{
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);
    return HAL_OK;
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
