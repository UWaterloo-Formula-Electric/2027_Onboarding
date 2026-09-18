/**
  ******************************************************************************
  * @file    i2C_dac.h
  * @brief   Starter interface for the MCP4728 I2C DAC onboarding task.
  ******************************************************************************
  */

#ifndef I2C_DAC_H
#define I2C_DAC_H

#include "stm32f7xx_hal.h"

#include <stdint.h>

#define I2C_DAC_FRAME_MAX_LENGTH 16U

// 12 bit DAC, so D11:D0 spans 0 to 4095
#define I2C_DAC_CODE_COUNT 4096U
#define I2C_DAC_CODE_MAX (I2C_DAC_CODE_COUNT - 1U)

// Full scale output, from V_out = (code / 4096) * V_ref.
//
// MCP4728_VREF in i2c_dac.c is 0, which selects VDD as the reference rather
// than the internal 2.048 V (datasheet Table 4-3), and the gain bit is ignored
// in that mode. So this is the DAC's supply rail, not 2048 - measure it and
// correct the value if the part is not on 3V3. Setting MCP4728_VREF to 1 would
// make 2048 right instead.
#define I2C_DAC_FULL_SCALE_MV 3300U

// Parks LDAC high. Call before the first transfer.
HAL_StatusTypeDef i2cDacInit(void);

// Returns HAL_OK if the DAC acknowledges its address on the configured bus
HAL_StatusTypeDef i2cDacProbe(void);

HAL_StatusTypeDef i2cDacSetCode(uint16_t code);

// Converts to a 12 bit code and hands off to i2cDacSetCode
HAL_StatusTypeDef i2cDacSetMillivolts(uint16_t millivolts);

#endif /* I2C_DAC_H */
