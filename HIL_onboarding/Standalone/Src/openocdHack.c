/**
  *****************************************************************************
  * @file    openocdHack.c
  * @brief   Standalone replacement for common/Src/freertos_openocd_hack.c
  * @details OpenOCD's FreeRTOS thread awareness reads uxTopUsedPriority out of
  * the ELF to work out how many priority levels the task lists hold. Nothing
  * in the firmware references it, so used keeps the compiler from dropping it
  * and -Wl,--undefined=uxTopUsedPriority in the Makefile keeps --gc-sections
  * from doing the same. The retain attribute would be the tidier answer but
  * arm-none-eabi-gcc 13 ignores it for this target.
  *****************************************************************************
  */

#include "FreeRTOS.h"

__attribute__((used))
const int uxTopUsedPriority = configMAX_PRIORITIES - 1;
