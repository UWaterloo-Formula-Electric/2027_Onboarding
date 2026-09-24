##########################################################################################################################
# Standalone build for HIL_onboarding - no common/ directory required.
#
# board.mk next to this file is the other build: it includes common/tail.mk and
# only works from the parent firmware repo's root. This Makefile builds the same
# board on its own, with Standalone/ supplying the handful of things common/
# used to. Keep both - board.mk is what re-integration upstream needs.
#
#   make              build Bin/Release/HIL_onboarding.elf, .hex and .bin
#   make flash        build, then program over SWD with OpenOCD
#   make flash-stlink build, then program with st-flash
#   make gdb          open a debug session against the running target
#   make terminal     open a serial console on the CLI (PORT=, BAUD= to override)
#   make size         section sizes for the last build
#   make clean        remove build/ and Bin/
##########################################################################################################################

TARGET := HIL_onboarding
CUBE_DIR := Cube-F7-Src-respin
BUILD_DIR := build
BIN_DIR := Bin/Release

# debug build?
DEBUG := 1
# optimization
OPT := -Og

PREFIX := arm-none-eabi-
CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
CP := $(PREFIX)objcopy
SZ := $(PREFIX)size
GDB := $(PREFIX)gdb

#######################################
# source
#######################################
# This board's own code, plus Standalone/ in place of common/
C_SOURCES := \
Src/userInit.c \
Src/mainTaskEntry.c \
Src/errorHandler.c \
Src/hilCli.c \
Src/uartBus.c \
Src/i2cBus.c \
Src/spiBus.c \
Src/i2c_dac.c \
Standalone/Src/debug.c \
Standalone/Src/FreeRTOS_CLI.c \
Standalone/Src/openocdHack.c

# CubeMX generated sources, mirroring Cube-Lib.mk's list. Unlike Cube-Lib.mk
# this keeps Core/Src/syscalls.c: it only conflicted with common/Src/newlibHack.c,
# which no longer exists here, and its _write is what routes printf into
# Standalone/Src/debug.c's __io_putchar.
CUBE_C_SOURCES := \
Core/Src/main.c \
Core/Src/gpio.c \
Core/Src/freertos.c \
Core/Src/dma.c \
Core/Src/can.c \
Core/Src/dac.c \
Core/Src/i2c.c \
Core/Src/spi.c \
Core/Src/tim.c \
Core/Src/usart.c \
Core/Src/stm32f7xx_it.c \
Core/Src/stm32f7xx_hal_msp.c \
Core/Src/stm32f7xx_hal_timebase_tim.c \
Core/Src/system_stm32f7xx.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cortex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_gpio.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_exti.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_can.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dac.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dac_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_spi.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_spi_ex.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart.c \
Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart_ex.c \
Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
Middlewares/Third_Party/FreeRTOS/Source/list.c \
Middlewares/Third_Party/FreeRTOS/Source/queue.c \
Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
Middlewares/Third_Party/FreeRTOS/Source/timers.c \
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1/port.c

C_SOURCES += $(addprefix $(CUBE_DIR)/, $(CUBE_C_SOURCES))

ASM_SOURCES := $(CUBE_DIR)/startup_stm32f769xx.s

#######################################
# CFLAGS
#######################################
CPU := -mcpu=cortex-m7
FPU := -mfpu=fpv5-d16
FLOAT-ABI := -mfloat-abi=hard
MCU := $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# BOARD_TYPE_F7 is what Standalone/Inc/boardTypes.h turns into IS_BOARD_F7,
# which Inc/bsp.h #errors without
C_DEFS := \
-DUSE_HAL_DRIVER \
-DSTM32F769xx \
-DBOARD_TYPE_F7

C_INCLUDES := \
-IInc \
-IStandalone/Inc \
-I$(CUBE_DIR)/Core/Inc \
-I$(CUBE_DIR)/Drivers/STM32F7xx_HAL_Driver/Inc \
-I$(CUBE_DIR)/Drivers/STM32F7xx_HAL_Driver/Inc/Legacy \
-I$(CUBE_DIR)/Middlewares/Third_Party/FreeRTOS/Source/include \
-I$(CUBE_DIR)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS \
-I$(CUBE_DIR)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 \
-I$(CUBE_DIR)/Drivers/CMSIS/Device/ST/STM32F7xx/Include \
-I$(CUBE_DIR)/Drivers/CMSIS/Include

ASFLAGS := $(MCU) $(OPT) -Wall -fdata-sections -ffunction-sections
CFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# -MMD names the .d after the .o on its own. Spelling it with -MF"$(@:%.o=%.d)"
# the way CubeMX's Makefile does only works if CFLAGS is recursively expanded;
# CFLAGS is := here, so $@ would expand to nothing at this point.
CFLAGS += -MMD -MP

#######################################
# LDFLAGS
#######################################
LDSCRIPT := $(CUBE_DIR)/STM32F769xx_FLASH.ld

LIBS := -lc -lm -lnosys
# --undefined pins uxTopUsedPriority, which only OpenOCD's thread awareness
# reads and --gc-sections would otherwise drop. See Standalone/Src/openocdHack.c.
LDFLAGS := $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBS) \
           -Wl,-Map=$(BIN_DIR)/$(TARGET).map,--cref -Wl,--gc-sections \
           -Wl,--undefined=uxTopUsedPriority

#######################################
# build
#######################################
OBJECTS := $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.s=.o))

.PHONY: all size flash load flash-stlink gdb terminal clean

all: $(BIN_DIR)/$(TARGET).elf $(BIN_DIR)/$(TARGET).hex $(BIN_DIR)/$(TARGET).bin

# Object paths mirror the source tree rather than being flattened with notdir,
# so a new Src/tim.c could never collide with Cube's Core/Src/tim.c
$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	@mkdir -p $(dir $@)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BIN_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BIN_DIR)/%.hex: $(BIN_DIR)/%.elf
	$(CP) -O ihex $< $@

$(BIN_DIR)/%.bin: $(BIN_DIR)/%.elf
	$(CP) -O binary -S $< $@

size: $(BIN_DIR)/$(TARGET).elf
	$(SZ) -A $<

#######################################
# flash
#######################################
OPENOCD ?= openocd
OPENOCD_INTERFACE ?= interface/stlink.cfg
OPENOCD_TARGET ?= target/stm32f7x.cfg

flash: $(BIN_DIR)/$(TARGET).elf
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) \
	  -c "program $< verify reset exit"

# board.mk spells this verb "load", so accept both
load: flash

flash-stlink: $(BIN_DIR)/$(TARGET).bin
	st-flash --reset write $< 0x8000000

gdb: $(BIN_DIR)/$(TARGET).elf
	$(GDB) $< -ex "target extended-remote localhost:3333"

#######################################
# serial console
#######################################
# Baud comes from MX_UART4_Init in Cube-F7-Src-respin/Core/Src/usart.c. Override
# PORT for a different adapter, e.g. make terminal PORT=/dev/ttyUSB0. Exit
# picocom with Ctrl-A Ctrl-X, tio with Ctrl-T Q, screen with Ctrl-A K.
PORT ?= /dev/ttyACM0
BAUD ?= 115200

terminal:
	@if command -v picocom >/dev/null 2>&1; then \
	  echo "picocom on $(PORT) at $(BAUD), exit with Ctrl-A Ctrl-X"; \
	  picocom -b $(BAUD) $(PORT); \
	elif command -v tio >/dev/null 2>&1; then \
	  echo "tio on $(PORT) at $(BAUD), exit with Ctrl-T Q"; \
	  tio -b $(BAUD) $(PORT); \
	elif command -v screen >/dev/null 2>&1; then \
	  echo "screen on $(PORT) at $(BAUD), exit with Ctrl-A K"; \
	  screen $(PORT) $(BAUD); \
	else \
	  echo "No serial terminal found. Install one of: picocom, tio, screen."; \
	  exit 1; \
	fi

#######################################
# clean
#######################################
clean:
	-rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(shell find $(BUILD_DIR) -name '*.d' 2>/dev/null)
