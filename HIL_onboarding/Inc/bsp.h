/**
  *****************************************************************************
  * @file    bsp.h
  * @brief   Board support package (BSP) header file for the HIL onboarding
  *          sandbox.
  * @details Instead of directly referring to hardware in source code, create
  * a define here and use that everywhere else, the same convention every
  * other board uses. See ../README.md.
  *
  ******************************************************************************
  */

#ifndef HIL_BSP_H
#define HIL_BSP_H

// Compiles the CAN paths out of common/Src/debug.c, so this board links
// without userCan.c. Must be defined before any common/ header is included.
#define DISABLE_CAN_FEATURES

#include "boardTypes.h"
#include "main.h"
#include "usart.h"
#include "stdbool.h"

#if IS_BOARD_F7
#include "stm32f7xx_hal.h"
#include "dac.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"

/* Peripheral handles ------------------------------------------------------ */

// The schematic's debug link, now pinned to PH13/PH14 with DMA on TX and RX
#define DEBUG_UART_HANDLE huart4

// Signal injection, broken out to the bench connectors
#define DAC_HANDLE hdac
#define DAC_1_CHANNEL DAC_CHANNEL_1
#define DAC_2_CHANNEL DAC_CHANNEL_2
#define I2C_1_HANDLE hi2c1
#define I2C_2_HANDLE hi2c2
#define I2C_3_HANDLE hi2c3
#define SPI_4_HANDLE hspi4
#define SPI_5_HANDLE hspi5

#define PWM_TIM_HANDLE htim8        // PC6/PC7/PC8
#define PWM_8_CHANNEL TIM_CHANNEL_1
#define PWM_9_CHANNEL TIM_CHANNEL_2
#define PWM_10_CHANNEL TIM_CHANNEL_3
#define STATS_TIM_HANDLE htim7      // Owned by common/Src/debug.c

#define DEBUG_LED_PIN GPIO3V_1_Pin
#define DEBUG_LED_PORT GPIO3V_1_GPIO_Port
#define ERROR_LED_PIN GPIO3V_2_Pin
#define ERROR_LED_PORT GPIO3V_2_GPIO_Port

#define GPIO5V_WRITE(n, state) HAL_GPIO_WritePin(GPIO5V_##n##_GPIO_Port, GPIO5V_##n##_Pin, (state))
#define GPIO5V_READ(n) HAL_GPIO_ReadPin(GPIO5V_##n##_GPIO_Port, GPIO5V_##n##_Pin)
#define GPIO12V_WRITE(n, state) HAL_GPIO_WritePin(GPIO12V_##n##_GPIO_Port, GPIO12V_##n##_Pin, (state))
#define GPIO12V_READ(n) HAL_GPIO_ReadPin(GPIO12V_##n##_GPIO_Port, GPIO12V_##n##_Pin)
#define GPIO3V_WRITE(n, state) HAL_GPIO_WritePin(GPIO3V_##n##_GPIO_Port, GPIO3V_##n##_Pin, (state))
#define GPIO3V_READ(n) HAL_GPIO_ReadPin(GPIO3V_##n##_GPIO_Port, GPIO3V_##n##_Pin)

// TODO: PWM_1..PWM_7 (PG2-PG8) have no timer AF on this part, GPIO only

#else

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#pragma message "BOARD_TYPE_F7: " #BOARD_TYPE_F7
#error Compiling for unknown board type

#endif

// Comment out to remove debug printing
#define DEBUG_ON

// Comment out to remove error printing
#define ERROR_PRINT_ON

#define CONSOLE_PRINT_ON

#endif /* HIL_BSP_H */
