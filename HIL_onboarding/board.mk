BUILD_TARGET = HIL_onboarding
BOARD_NAME = HIL_onboarding
BOARD_NAME_UPPER = HIL_ONBOARDING
BOARD_ARCHITECTURE = F7
MCU_DEFINE = STM32F769xx

# Onboarding sandbox - see README.md. No CAN at all, so no DBC/DTC codegen and
# no userCan.c/userCanF7.c. Inc/bsp.h defines DISABLE_CAN_FEATURES, which is
# what compiles the CAN paths out of common/Src/debug.c.
DBC_CODEGEN = 0

COMMON_LIB_SRC := debug.c FreeRTOS_CLI.c freertos_openocd_hack.c newlibHack.c

CUBE_F7_MAKEFILE_PATH := $(BOARD_NAME)/Cube-F7-Src-respin/

include common/tail.mk
