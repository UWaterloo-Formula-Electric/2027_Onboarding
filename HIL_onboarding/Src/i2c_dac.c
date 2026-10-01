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
#include "task.h"      // gives us vTaskDelay, used to hold LDAC low for a moment

#include "debug.h"
#include "i2c.h"       // declares hi2c1, the handle for I2C peripheral 1 (set up by CubeMX)
#include "i2cBus.h"

#define I2C_DAC_TIMEOUT_MS 15

// 210ns minimum, 1 tick is plenty
#define T_LDAC_MS 1


#define I2C_DAC_BUS I2C_BUS_1

// ---- Chip address (datasheet section 5.3) ----
// The MCP4728's 7-bit I2C address is a fixed device code "1100" followed by
// 3 user-settable bits A2 A1 A0. Ours are wired to 0, so the address is
// 1100 000 = 0x60. This is not yet the byte we put on the wire - see
// I2C_DAC_ADDRESS7 << 1U further down, where HAL wants it shifted left
// by 1 to leave room for the R/W bit it adds itself.
#define MCP4728_A2 0U
#define MCP4728_A1 0U
#define MCP4728_A0 0U
#define I2C_DAC_ADDRESS7 (0x60U | (MCP4728_A2 << 2U) | (MCP4728_A1 << 1U) | MCP4728_A0)

// ---- Command byte fields (datasheet Table 5-1, section 5.6.2) ----
// C2 C1 C0 = 010 and W1 W0 = 00 together select the "Multi-Write" command:
// write one channel's input register, don't touch EEPROM. This is byte 0
// of each channel's 3-byte message.
#define MCP4728_C2 0U
#define MCP4728_C1 1U
#define MCP4728_C0 0U
#define MCP4728_W1 0U
#define MCP4728_W0 0U

// DAC1/DAC0 pick which of the chip's four internal DACs (channels A-D) this
// message is for (datasheet Table 4-3: 00=A, 01=B, 10=C, 11=D). We only use
// A and B, so DAC1 is 0 for both and DAC0 is the only bit that differs.
#define MCP4728_DAC1_A 0U 
#define MCP4728_DAC0_A 0U
#define MCP4728_DAC1_B 0U 
#define MCP4728_DAC0_B 1U

// UDAC = 1 means "load this channel's input register, but do NOT update the
// output pin yet". We want both channels loaded first, then updated together
// by strobing LDAC (in activateDAC), rather than updating one at a time.
#define MCP4728_UDAC 1U

// ---- Settings byte fields (byte 1 of each channel's message) ----
#define MCP4728_VREF 0U   // 0 = use VDD (the chip's supply pin) as the reference voltage
#define MCP4728_PD1 0U    // Power-down bits: 00 = normal operating mode (channel is on)
#define MCP4728_PD0 0U
#define MCP4728_GX 0U     // Gain = x1 (gain of x2 only applies when using the internal reference, which we aren't)

// A frame is just the list of bytes we hand to HAL for one I2C transmission.
// The address is NOT stored here - transmitFrame passes it separately.
typedef struct {
    uint8_t bytes[I2C_DAC_FRAME_MAX_LENGTH];
    uint16_t length;
} I2cDacFrame_t;

// Builds the 6-byte Multi-Write frame: 3 bytes for channel A, then 3 bytes
// for channel B, both carrying the same 12-bit code.
static HAL_StatusTypeDef buildOutputFrame(uint16_t code, I2cDacFrame_t *frame)
{
    if (code > I2C_DAC_CODE_MAX || frame == NULL) {
        // code > 4095 can't fit in 12 bits, and a NULL frame has nowhere to write to
        return HAL_ERROR;
    }

    frame->length = 0U;

    // ============ Channel A ============

    // Byte 0 - command byte. Bit layout (datasheet Fig 5-8):
    //   bit:  7   6   5   4   3    2      1      0
    //        C2  C1  C0  W1  W0  DAC1  DAC0  UDAC
    // Each field gets shifted left to its bit position, then OR'd together
    // to pack them all into one byte. For channel A this works out to 0x41.
    frame->bytes[0] = (MCP4728_C2 << 7) | (MCP4728_C1 << 6) | (MCP4728_C0 << 5) | (MCP4728_W1 << 4) | 
                    (MCP4728_W0 << 3) | (MCP4728_DAC1_A << 2) | (MCP4728_DAC0_A << 1) | MCP4728_UDAC;

    // Byte 1 - settings + top 4 bits of the code. Bit layout:
    //   bit:  7     6    5    4    3    2    1    0
    //        VREF  PD1  PD0  GX  D11  D10   D9   D8
    // "code >> 8" slides the 12-bit code right by 8, which drops the low 8
    // bits and leaves just D11:D8 sitting in the bottom nibble of this byte.
    // (code is already capped at 4095 = 0xFFF by the check above, so those
    // top bits can never be more than 4 bits wide here.)
    frame->bytes[1] = (MCP4728_VREF << 7) | (MCP4728_PD1 << 6) | (MCP4728_PD0 << 5) | (MCP4728_GX << 4)   | (code >> 8);

    // Byte 2 - the low 8 bits of the code (D7:D0).
    // "code & 0xFF" masks (keeps) only the bottom 8 bits and throws away
    // everything above them.
    frame->bytes[2] = (uint8_t)(code & 0xFF);

    // ============ Channel B ============
    // Identical structure to channel A, just with the DAC1_B/DAC0_B select
    // bits instead, which makes byte 3 come out as 0x43 instead of 0x41.
    // That one-bit difference is what tells the chip "this is channel B".
    frame->bytes[3] = (MCP4728_C2 << 7) | (MCP4728_C1 << 6) | (MCP4728_C0 << 5) | (MCP4728_W1 << 4) | 
                    (MCP4728_W0 << 3) | (MCP4728_DAC1_B << 2) | (MCP4728_DAC0_B << 1) | MCP4728_UDAC;
    frame->bytes[4] = (MCP4728_VREF << 7) | (MCP4728_PD1 << 6) | (MCP4728_PD0 << 5) | (MCP4728_GX << 4)   | (code >> 8);
    frame->bytes[5] = (uint8_t)(code & 0xFF);

    // 2 channels x 3 bytes = 6. This is everything after the address byte;
    // transmitFrame will send the address separately before these bytes.
    frame->length = 6U;
    return HAL_OK;
}

// Sends the frame over I2C to the DAC's address.
static HAL_StatusTypeDef transmitFrame(I2cDacFrame_t *frame)
{
    if (frame == NULL || frame->length == 0U) {
        return HAL_ERROR;
    }
    // HAL_I2C_Master_Transmit handles the low-level I2C details for us:
    // START, the address byte, our data bytes, waiting for ACKs, then STOP.
    //   &hi2c1                    - which I2C hardware peripheral to use
    //   I2C_DAC_ADDRESS7 << 1U    - the 7-bit address shifted left by 1;
    //                               HAL fills in the R/W bit itself in bit 0
    //   (uint8_t *)frame->bytes   - the bytes to send
    //   frame->length             - how many bytes (6)
    //   I2C_DAC_TIMEOUT_MS        - give up after 15 ms if the bus is stuck
    return HAL_I2C_Master_Transmit(&hi2c1, I2C_DAC_ADDRESS7 << 1U, (uint8_t *)frame->bytes, frame->length, I2C_DAC_TIMEOUT_MS);
}

// Pulses LDAC (wired to pin PF10) so the chip copies both channels' input
// registers into their output registers at the same moment, updating both
// VOUT pins together. LDAC is active low, so "pulse" means low-then-high.
static HAL_StatusTypeDef activateDAC(void)
{
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);  // assert: drive PF10 low -> this falling edge is what triggers the update
    vTaskDelay(pdMS_TO_TICKS(T_LDAC_MS));                   // hold low briefly (datasheet only needs 210 ns; 1 ms is comfortably more)
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);    // release: drive PF10 back high, parking it for the next write
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
    // Sends just the address and checks whether the chip ACKs - no data, no
    // frame needed. This is the datasheet's "Device Connection Test"
    // (section 7.1.1). Always run this first: if it fails, the problem is
    // wiring/power/address, not anything in buildOutputFrame.
    // i2cBus wants the 7 bit address already shifted up for the R/W bit
    return i2cIsDeviceReady(I2C_DAC_BUS, I2C_DAC_ADDRESS7 << 1U);
}

HAL_StatusTypeDef i2cDacSetMillivolts(uint16_t millivolts)
{
    uint32_t code;

    if (millivolts >= I2C_DAC_FULL_SCALE_MV) {
        return HAL_ERROR;
    }

    // Rearranged from Vout = (code / 4096) * Vref:
    //   code = millivolts * 4096 / full_scale_millivolts
    // Cast to uint32_t and multiply BEFORE dividing, so we don't lose
    // precision to integer division, and so the intermediate value
    // (millivolts * 4096, which can be over a million) doesn't overflow
    // a smaller type.
    code = ((uint32_t)millivolts * I2C_DAC_CODE_COUNT) / I2C_DAC_FULL_SCALE_MV;

    return i2cDacSetCode((uint16_t)code);
}

// Ties the three steps together in order: build the bytes, send them over
// I2C, then strobe LDAC so the new values actually reach the output pins.
// If any step fails, we bail out immediately rather than continuing with
// bad or unsent data.
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