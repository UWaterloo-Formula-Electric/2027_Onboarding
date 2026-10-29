#!/usr/bin/env python3
"""Compile the real driver with HAL fakes and check its bus/pin behavior."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HAL = r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stddef.h>
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int unused; } I2C_HandleTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
#define GPIOF ((void *)1)
#define GPIO_PIN_10 (1U << 10)
void HAL_GPIO_WritePin(void *, uint16_t, GPIO_PinState);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *, uint16_t,
                                        uint8_t *, uint16_t, uint32_t);
#endif
'''
TEST = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "i2c.h"
#include "i2cBus.h"
#include "task.h"
#include "i2C_dac.h"

I2C_HandleTypeDef hi2c1;
static uint8_t payload[16];
static unsigned tx, pins, delays, probe;
static char events[8];
static unsigned eventCount;
static HAL_StatusTypeDef txResult = HAL_OK;
static void reset(void) {
    tx = pins = delays = probe = eventCount = 0;
    txResult = HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *bus,
        uint16_t addr, uint8_t *data, uint16_t length, uint32_t timeout) {
    assert(bus == &hi2c1 && addr == 0xC0 && length == 6 && timeout == 15);
    memcpy(payload, data, length);
    ++tx; events[eventCount++] = 'T';
    return txResult;
}
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state) {
    assert(port == GPIOF && pin == GPIO_PIN_10);
    ++pins; events[eventCount++] = state == GPIO_PIN_SET ? 'H' : 'L';
}
void vTaskDelay(TickType_t ticks) {
    assert(ticks >= 1); ++delays; events[eventCount++] = 'D';
}
HAL_StatusTypeDef i2cIsDeviceReady(I2cBus_t bus, uint16_t addr) {
    assert(bus == I2C_BUS_1 && addr == 0xC0); ++probe;
    return HAL_TIMEOUT;
}
int main(void) {
    reset(); assert(i2cDacInit() == HAL_OK);
    assert(pins == 1 && events[0] == 'H' && tx == 0);
    reset(); assert(i2cDacProbe() == HAL_TIMEOUT && probe == 1);
    // Check every valid 12-bit code, including both byte boundaries.
    for (unsigned code = 0; code < 4096; ++code) {
        reset(); assert(i2cDacSetCode(code) == HAL_OK);
        const uint8_t expected[] = {0x41, code >> 8, code & 255,
                                    0x43, code >> 8, code & 255};
        assert(memcmp(payload, expected, 6) == 0);
        assert(tx == 1 && pins == 2 && delays == 1);
        assert(eventCount == 4 && memcmp(events, "TLDH", 4) == 0);
    }
    reset(); assert(i2cDacSetCode(4096) == HAL_ERROR && tx == 0 && pins == 0);
    reset(); assert(i2cDacSetCode(UINT16_MAX) == HAL_ERROR && tx == 0);
    for (unsigned mv = 0; mv < 3300; ++mv) {
        reset(); assert(i2cDacSetMillivolts(mv) == HAL_OK);
        assert((((unsigned)payload[1] << 8) | payload[2]) == mv * 4096 / 3300);
    }
    reset(); assert(i2cDacSetMillivolts(3300) == HAL_ERROR && tx == 0);
    reset(); assert(i2cDacSetMillivolts(UINT16_MAX) == HAL_ERROR && tx == 0);
    for (unsigned result = HAL_ERROR; result <= HAL_TIMEOUT; ++result) {
        reset(); txResult = result;
        assert(i2cDacSetCode(2048) == HAL_ERROR);
        assert(tx == 1 && pins == 0 && delays == 0);
    }
    puts("PASS: 4096 codes, 3300 voltages, bounds, I2C failures, probe and LDAC order");
}
'''
with tempfile.TemporaryDirectory(prefix='hil-dac-test-') as directory:
    tmp = Path(directory)
    (tmp / 'stm32f7xx_hal.h').write_text(HAL)
    (tmp / 'FreeRTOS.h').write_text('#pragma once\n#include <stdint.h>\ntypedef uint32_t TickType_t;\n#ifndef TEST_TICK_HZ\n#define TEST_TICK_HZ 1000U\n#endif\n#define pdMS_TO_TICKS(ms) ((ms) * TEST_TICK_HZ / 1000U)\n')
    (tmp / 'task.h').write_text('#include "FreeRTOS.h"\nvoid vTaskDelay(TickType_t);\n')
    (tmp / 'debug.h').write_text('')
    (tmp / 'i2c.h').write_text('#include "stm32f7xx_hal.h"\nextern I2C_HandleTypeDef hi2c1;\n')
    (tmp / 'test.c').write_text(TEST)
    for tick_hz in (1000, 100):
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra',
                        '-Werror', f'-DTEST_TICK_HZ={tick_hz}', '-I', str(tmp),
                        '-I', str(ROOT / 'Inc'), str(ROOT / 'Src/i2c_dac.c'),
                        str(tmp / 'test.c'), '-o', str(tmp / 'test')], check=True)
        subprocess.run([str(tmp / 'test')], check=True)
