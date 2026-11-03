/**
  ******************************************************************************
  * @file    i2c_dac.c
  * @brief   MCP4728 I2C DAC driver.
  ******************************************************************************
  */

#include "i2C_dac.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug.h"
#include "i2c.h"
#include "i2cBus.h"

#define I2C_DAC_TIMEOUT_MS 15U

// LDAC pulse duration. One RTOS tick is sufficient.
#define T_LDAC_MS 1U

#define I2C_DAC_BUS I2C_BUS_1

// MCP4728 I2C address pins: A2 = 0, A1 = 0, A0 = 0.
#define MCP4728_A2 0U
#define MCP4728_A1 0U
#define MCP4728_A0 0U

#define I2C_DAC_ADDRESS7 \
    (0x60U | (MCP4728_A2 << 2U) | (MCP4728_A1 << 1U) | MCP4728_A0)

// Multi-Write command bits.
#define MCP4728_C2 0U
#define MCP4728_C1 1U
#define MCP4728_C0 0U

#define MCP4728_W1 0U
#define MCP4728_W0 0U

// DAC channel selection.
#define MCP4728_CHANNEL_A 0U
#define MCP4728_CHANNEL_B 1U

// Defer output update until LDAC is pulled LOW.
#define MCP4728_UDAC 1U

// DAC configuration.
#define MCP4728_VREF 0U
#define MCP4728_PD1  0U
#define MCP4728_PD0  0U
#define MCP4728_GX   0U

typedef struct
{
    uint8_t bytes[I2C_DAC_FRAME_MAX_LENGTH];
    uint16_t length;
} I2cDacFrame_t;


/*
 * Builds the first byte of an MCP4728 Multi-Write operation.
 *
 * This byte contains:
 * C2 C1 C0 W1 W0 DAC1 DAC0 UDAC
 */
static uint8_t buildInitialCommandByte(uint8_t channel)
{
    return (uint8_t)(
        ((MCP4728_C2 & 0x01U) << 7U) |
        ((MCP4728_C1 & 0x01U) << 6U) |
        ((MCP4728_C0 & 0x01U) << 5U) |
        ((MCP4728_W1 & 0x01U) << 4U) |
        ((MCP4728_W0 & 0x01U) << 3U) |
        ((channel & 0x03U) << 1U) |
        (MCP4728_UDAC & 0x01U)
    );
}


/*
 * Builds the channel-selection byte for subsequent channels.
 *
 * In repeated Multi-Write sequences, the upper five bits are
 * don't-care bits. We set them to zero.
 *
 * For channel B this produces 0x03.
 */
static uint8_t buildRepeatedChannelByte(uint8_t channel)
{
    return (uint8_t)(
        ((channel & 0x03U) << 1U) |
        (MCP4728_UDAC & 0x01U)
    );
}


/*
 * Builds a Multi-Write frame to update DAC channels A and B
 * with the same 12-bit output code.
 *
 * Each channel requires three bytes:
 *   1. Channel selection / command
 *   2. VREF, power-down, gain and D11:D8
 *   3. D7:D0
 *
 * Total frame length: 6 bytes.
 */
static HAL_StatusTypeDef buildOutputFrame(
    uint16_t code,
    I2cDacFrame_t *frame)
{
    uint8_t configAndUpperCode;
    uint8_t lowerCode;

    if ((code > I2C_DAC_CODE_MAX) || (frame == NULL))
    {
        return HAL_ERROR;
    }

    frame->length = 0U;

    // Configuration bits followed by the upper four DAC data bits.
    configAndUpperCode = (uint8_t)(
        ((MCP4728_VREF & 0x01U) << 7U) |
        ((MCP4728_PD1  & 0x01U) << 6U) |
        ((MCP4728_PD0  & 0x01U) << 5U) |
        ((MCP4728_GX   & 0x01U) << 4U) |
        ((code >> 8U) & 0x0FU)
    );

    // Lower eight DAC data bits.
    lowerCode = (uint8_t)(code & 0xFFU);

    // Channel A: first Multi-Write sequence.
    frame->bytes[0] = buildInitialCommandByte(MCP4728_CHANNEL_A);
    frame->bytes[1] = configAndUpperCode;
    frame->bytes[2] = lowerCode;

    // Channel B: repeated Multi-Write sequence.
    frame->bytes[3] = buildRepeatedChannelByte(MCP4728_CHANNEL_B);
    frame->bytes[4] = configAndUpperCode;
    frame->bytes[5] = lowerCode;

    frame->length = 6U;

    return HAL_OK;
}


/*
 * Transmits the prepared frame over I2C1.
 */
static HAL_StatusTypeDef transmitFrame(const I2cDacFrame_t *frame)
{
    if ((frame == NULL) ||
        (frame->length == 0U) ||
        (frame->length > I2C_DAC_FRAME_MAX_LENGTH))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(
        &hi2c1,
        (uint16_t)(I2C_DAC_ADDRESS7 << 1U),
        (uint8_t *)frame->bytes,
        frame->length,
        I2C_DAC_TIMEOUT_MS
    );
}


/*
 * Pulses LDAC to update the DAC outputs.
 *
 * LDAC is active LOW:
 *   LOW  = update outputs
 *   HIGH = normal / inactive state
 */
static HAL_StatusTypeDef activateDAC(void)
{
    TickType_t delayTicks;

    // Assert LDAC.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);

    // Wait for the required pulse duration.
    delayTicks = pdMS_TO_TICKS(T_LDAC_MS);

    if (delayTicks == 0U)
    {
        delayTicks = 1U;
    }

    vTaskDelay(delayTicks);

    // Release LDAC and leave it HIGH for the next transfer.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);

    return HAL_OK;
}


/*
 * Initializes the DAC control pin.
 */
HAL_StatusTypeDef i2cDacInit(void)
{
    // LDAC must start HIGH.
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);

    return HAL_OK;
}


/*
 * Checks whether the DAC responds on I2C.
 */
HAL_StatusTypeDef i2cDacProbe(void)
{
    return i2cIsDeviceReady(
        I2C_DAC_BUS,
        (uint16_t)(I2C_DAC_ADDRESS7 << 1U)
    );
}


/*
 * Converts millivolts into a 12-bit DAC code.
 */
HAL_StatusTypeDef i2cDacSetMillivolts(uint16_t millivolts)
{
    uint32_t code;

    if (millivolts >= I2C_DAC_FULL_SCALE_MV)
    {
        return HAL_ERROR;
    }

    code = (
        (uint32_t)millivolts * I2C_DAC_CODE_COUNT
    ) / I2C_DAC_FULL_SCALE_MV;

    return i2cDacSetCode((uint16_t)code);
}


/*
 * Sets the DAC output code on channels A and B.
 */
HAL_StatusTypeDef i2cDacSetCode(uint16_t code)
{
    I2cDacFrame_t frame;
    HAL_StatusTypeDef status;

    // 1. Build the I2C frame.
    status = buildOutputFrame(code, &frame);

    if (status != HAL_OK)
    {
        return status;
    }

    // 2. Send the frame to the DAC.
    status = transmitFrame(&frame);

    if (status != HAL_OK)
    {
        return status;
    }

    // 3. Pulse LDAC to update the analog outputs.
    status = activateDAC();

    return status;
}