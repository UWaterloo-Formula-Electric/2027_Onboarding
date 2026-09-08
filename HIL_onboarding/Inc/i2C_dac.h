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

HAL_StatusTypeDef i2cDacSetCode(uint16_t code);

#endif /* I2C_DAC_H */
