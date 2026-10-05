/**
  ******************************************************************************
  * @file    i2C_dac.c
  * @brief   MCP4728 I2C DAC driver starter.
  * @details Three TODOs to complete, in the order i2cDacSetCode calls them:
  *          buildOutputFrame, transmitFrame, then activateDAC. Use MCP4728
  *          datasheet sections 5.6.1 through 5.6.4.
  *
  *          i2cDacProbe works before any of them do - run it from the CLI to
  *          confirm the part answers on the bus before debugging your frame.
  ******************************************************************************
  */

#include "i2C_dac.h"

#include "FreeRTOS.h"
#include "task.h"      // vTaskDelay for LDAC strobe timing

#include "debug.h"
#include "i2c.h"       // STM32 HAL I2C handle (hi2c1)
#include "i2cBus.h"

#define I2C_DAC_TIMEOUT_MS 15

// Minimum required LDAC active-low pulse duration
#define T_LDAC_MS 1


#define I2C_DAC_BUS I2C_BUS_1

// Device I2C address configuration (Base 0x60, bits A2..A0 set to 000)
#define MCP4728_A2 0U
#define MCP4728_A1 0U
#define MCP4728_A0 0U
#define I2C_DAC_ADDRESS7 (0x60U | (MCP4728_A2 << 2U) | (MCP4728_A1 << 1U) | MCP4728_A0)

// Multi-Write Command bits (C2..C0 = 010, W1..W0 = 00)
#define MCP4728_C2 0U
#define MCP4728_C1 1U
#define MCP4728_C0 0U
#define MCP4728_W1 0U
#define MCP4728_W0 0U

// Channel selection bits (DAC1..DAC0: 00 = Ch A, 01 = Ch B)
#define MCP4728_DAC1_A 0U 
#define MCP4728_DAC0_A 0U
#define MCP4728_DAC1_B 0U 
#define MCP4728_DAC0_B 1U

// UDAC bit: 1 defer output pin update until LDAC strobe
#define MCP4728_UDAC 1U

// Operational configuration flags (VDD ref, normal power mode, 1x gain)
#define MCP4728_VREF 0U   
#define MCP4728_PD1 0U    
#define MCP4728_PD0 0U
#define MCP4728_GX 0U     

// Payload structure for outgoing I2C buffer
typedef struct {
    uint8_t bytes[I2C_DAC_FRAME_MAX_LENGTH];
    uint16_t length;
} I2cDacFrame_t;

/**
 * @brief Constructs the Multi-Write command payload for channels A and B.
 * @param code 12-bit digital value to set on both channels.
 * @param frame Output pointer storing constructed payload data and length.
 * @return HAL_OK on success, HAL_ERROR if arguments are invalid.
 */
static HAL_StatusTypeDef buildOutputFrame(uint16_t code, I2cDacFrame_t *frame)
{
    if (code > I2C_DAC_CODE_MAX || frame == NULL) {
        return HAL_ERROR;
    }

    frame->length = 0U;

    // --- Channel A Configuration ---
    // Byte 0: Command & Channel Selection (C2..C0 | W1..W0 | DAC1..DAC0 | UDAC)
    frame->bytes[0] = (MCP4728_C2 << 7) | (MCP4728_C1 << 6) | (MCP4728_C0 << 5) | (MCP4728_W1 << 4) | 
                      (MCP4728_W0 << 3) | (MCP4728_DAC1_A << 2) | (MCP4728_DAC0_A << 1) | MCP4728_UDAC;

    // Byte 1: Settings & High Nibble Data (VREF | PD1..PD0 | GX | D11..D8)
    frame->bytes[1] = (MCP4728_VREF << 7) | (MCP4728_PD1 << 6) | (MCP4728_PD0 << 5) | (MCP4728_GX << 4)   | (code >> 8);

    // Byte 2: Low Byte Data (D7..D0)
    frame->bytes[2] = (uint8_t)(code & 0xFF);

    // --- Channel B Configuration ---
    // Byte 3: Command & Channel Selection
    frame->bytes[3] = (MCP4728_C2 << 7) | (MCP4728_C1 << 6) | (MCP4728_C0 << 5) | (MCP4728_W1 << 4) | 
                      (MCP4728_W0 << 3) | (MCP4728_DAC1_B << 2) | (MCP4728_DAC0_B << 1) | MCP4728_UDAC;

    // Byte 4: Settings & High Nibble Data
    frame->bytes[4] = (MCP4728_VREF << 7) | (MCP4728_PD1 << 6) | (MCP4728_PD0 << 5) | (MCP4728_GX << 4)   | (code >> 8);

    // Byte 5: Low Byte Data
    frame->bytes[5] = (uint8_t)(code & 0xFF);

    frame->length = 6U;
    return HAL_OK;
}

/**
 * @brief Transmits a payload frame over I2C to the target DAC address.
 * @param frame Pointer to the struct containing the payload buffer.
 * @return HAL_StatusTypeDef Transmission result from the HAL layer.
 */
static HAL_StatusTypeDef transmitFrame(I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U) {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(&hi2c1, I2C_DAC_ADDRESS7 << 1U, (uint8_t *)frame->bytes, frame->length, I2C_DAC_TIMEOUT_MS);
}

/**
 * @brief Generates an active-low pulse on the LDAC pin (PF10) to update analog outputs.
 * @return HAL_OK
 */
static HAL_StatusTypeDef activateDAC(void)
{
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);  // Assert active-low LDAC
    vTaskDelay(pdMS_TO_TICKS(T_LDAC_MS));                   // Hold pulse width
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);    // Deassert LDAC
    return HAL_OK;
}

HAL_StatusTypeDef i2cDacInit(void)
{
    // Ensure LDAC is idle high to prevent accidental latching on boot
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);

    return HAL_OK;
}

HAL_StatusTypeDef i2cDacProbe(void)
{
    // Check for target presence on the bus via I2C device polling
    return i2cIsDeviceReady(I2C_DAC_BUS, I2C_DAC_ADDRESS7 << 1U);
}

HAL_StatusTypeDef i2cDacSetMillivolts(uint16_t millivolts)
{
    uint32_t code;

    if (millivolts >= I2C_DAC_FULL_SCALE_MV) {
        return HAL_ERROR;
    }

    // Convert millivolts to 12-bit resolution step value without overflow
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