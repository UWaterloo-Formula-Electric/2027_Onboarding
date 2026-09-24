/**
  ******************************************************************************
  * @file    i2C_dac.c
  * @brief   MCP4728 I2C DAC driver starter.
  * @details Three TODOs to complete, in the order i2cDacSetCode calls them:
  *          buildOutputFrame, transmitFrame, then activateDAC. Use MCP4728
  *          datasheet sections 5.6.1 through 5.6.4.
  *          i2cDacProbe works before any of them do - run it from the CLI to
  *          confirm the part answers on the bus before debugging your frame.
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
// edited the ??? to 0U (all other multi-write functions being at Channel A)
#define MCP4728_DAC1 0U
#define MCP4728_DAC0 0U

// byte slot
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
    // check if inputs match a 12-bit DAC or return error
    if (code > I2C_DAC_CODE_MAX || frame == NULL) {
        return HAL_ERROR;
    }

    // start with empty message
    frame->length = 0U;

    // TODO: Construct an MCP4728 Multi-Write frame from the named fields and code.
    // Look at the top for a bunch of defined variables.
    //Analog voltage is mapped to code by taking the equation V_out = (code / 4096) * V_ref.
    //D11:D0 is essentially your code. That is what you are setting.
    //The I2C address is not part of this frame, transmitFrame sends it separately.

    // Byte 0: command + channel select + UDAC
    frame->bytes[0] = (uint8_t)((MCP4728_C2   << 7U) |
                                (MCP4728_C1   << 6U) |
                                (MCP4728_C0   << 5U) |
                                (MCP4728_W1   << 4U) |
                                (MCP4728_W0   << 3U) |
                                (MCP4728_DAC1 << 2U) |
                                (MCP4728_DAC0 << 1U) |
                                (MCP4728_UDAC << 0U));

    // Byte 1: config bits + top 4 bits of the code (D11:D8)
    frame->bytes[1] = (uint8_t)((MCP4728_VREF << 7U) |
                                (MCP4728_PD1  << 6U) |
                                (MCP4728_PD0  << 5U) |
                                (MCP4728_GX   << 4U) |
                                ((code >> 8U) & 0x0FU));

    // Byte 2: low 8 bits of the code (D7:D0)
    frame->bytes[2] = (uint8_t)(code & 0xFFU);

    frame->length = 3U;

    return HAL_OK;
}

static HAL_StatusTypeDef transmitFrame(const I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U) {
        return HAL_ERROR;
    }

    // TODO: Send frame->bytes to the DAC at I2C_DAC_ADDRESS7 on I2C_DAC_BUS.
    //use hi2c1 to send the frame
    //TIP: look up HAL_I2C_Master_Transmit. It sends the address byte itself and
    //wants the 7 bit address shifted left by one. Use I2C_DAC_TIMEOUT_MS
    return HAL_I2C_Master_Transmit(
            I2C_DAC_ADDRESS7 << 1U,
            (uint8_t *)frame->bytes, 
            frame->length, 
            I2C_DAC_TIMEOUT_MS);

    return HAL_ERROR;
}

static HAL_StatusTypeDef activateDAC(void)
{
    // TODO: To activate the DAC, we need to strobe PF10. First you need to go into
    // CubeMX and check the GPIO for PF10 is enabled as an output.
    //TIP: look up HAL_GPIO_WritePin, and see T_LDAC_MS above for the hold time.
    //LDAC is active low, so work out which level does what, and what state the
    //pin should be left in for the next write.

    // assert (LDAC is active low)
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);
    
    // wait (hold for one tick)
    vTaskDelay(pdMS_TO_TICKS(T_LDAC_MS));

    // release
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);

    return HAL_OK;
}

HAL_StatusTypeDef i2cDacInit(void)
{
    // CubeMX drives PF10 low at reset, and a low LDAC makes the DAC latch every
    // write at its last acknowledge, which defeats the UDAC bit the frames set.
    // Park it high so the first transfer defers like every later one.
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
