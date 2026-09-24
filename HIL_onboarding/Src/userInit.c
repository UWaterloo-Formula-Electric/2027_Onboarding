/**
  *****************************************************************************
  * @file    userInit.c
  * @brief   Initialization before RTOS starts
  * @details Contains the userInit function, which is called before the RTOS
  * starts to allow the user to initialize modules or other things that must
  * be done before the RTOS starts.
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"

#include "bsp.h"
#include "debug.h"
#include "hilCli.h"
#include "uartBus.h"
#include "i2C_dac.h"

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    signed char *pcTaskName )
{
    HAL_GPIO_WritePin(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_PIN_SET);
    printf("Stack overflow for task %s\n", pcTaskName);
}

// This is declared with weak linkage in all Cube main.c files, and called
// before freeRTOS initializes and starts up
void userInit()
{
    /* First light. printf falls through to a polled transmit until the print
     * task exists, so this lands on the wire before anything below can fail.
     * If the console is silent from here, the fault is the pins, the baud or
     * the cable, not the firmware. */
    printf("\n\nHIL_onboarding booting\n");

    /* Should be the first thing initialized, otherwise print will fail */
    if (debugInit() != HAL_OK) {
        Error_Handler();
    }

    if (uartBusInit() != HAL_OK) {
        Error_Handler();
    }

    if (i2cDacInit() != HAL_OK) {
        Error_Handler();
    }

    if (hilCliInit() != HAL_OK) {
        Error_Handler();
    }

    printf("Finished user init\n");
}
