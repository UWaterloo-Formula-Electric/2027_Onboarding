/**
  *****************************************************************************
  * @file    i2cBus.c
  * @brief   Generic transfer wrappers for the bench I2C buses
  * @details I2C1, I2C2 and I2C3 are broken out to the bench connectors. The
  * register based helpers cover the common case of an 8 bit register address
  * followed by data. Device specific framing belongs in that device's own
  * file, which should call these rather than the HAL directly.
  *****************************************************************************
  */

#include "i2cBus.h"

#include "bsp.h"
#include "debug.h"

#define I2C_TIMEOUT_MS 15
#define I2C_READY_TRIALS 2

/* Note: All these I2C methods are blocking, 
because if they were to be interrupted for wahtever reason, 
they would show up as busy and then the call would return 
they are busy */

/* you MUST shift the devAddress left by 1 bit, because the 
address in I2C lives in bits 7 to 1, leavin bit 0 for R/W*/

/* Example: if you want to address a device at address 0x12, you need 
to pass 0x24 to the HAL */
/* 0b00001100 -> 0b00011000 (0x12 -> 0x24) */

static I2C_HandleTypeDef *getI2cHandle(I2cBus_t bus)
{
    switch (bus) {
        case I2C_BUS_1:
            return &I2C_1_HANDLE;
        case I2C_BUS_2:
            return &I2C_2_HANDLE;
        case I2C_BUS_3:
            return &I2C_3_HANDLE;
        default:
            return NULL;
    }
}

HAL_StatusTypeDef i2cIsDeviceReady(I2cBus_t bus, uint16_t devAddress)
{
    I2C_HandleTypeDef *handle = getI2cHandle(bus);

    if (handle == NULL) {
        ERROR_PRINT("Failed to probe I2C bus %d, bad argument\n", bus);
        return HAL_ERROR;
    }

    return HAL_I2C_IsDeviceReady(handle, devAddress, I2C_READY_TRIALS, I2C_TIMEOUT_MS);
}
