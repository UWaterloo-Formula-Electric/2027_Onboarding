# HIL_onboarding

A stripped-down sandbox copy of `../HIL/` for new members. The task: **drive
the external DAC over I2C so the HIL board can output an analog signal to the
VCU.**

Everything not needed for that has been removed, so there is no CAN stack, no
DBC/DTC codegen, and no half-written skeleton files to wade through. It
builds and links as-is - start from a board that already works, and add one
driver.

## The task

1. Find the DAC on the bus. Power the board, open the CLI, run `i2cScan 1`
   (then 2, then 3). One of them should report an address. If none do, that
   is a wiring, pull-up, or bus-number problem, not a firmware problem - sort
   it out before writing code.
2. Complete the starter driver in `Src/i2C_dac.c` + `Inc/i2C_dac.h`. The
   intended MCP4728 field values are named for you. Use the datasheet to
   determine the command-frame structure and place those fields, including
   the channel selection and 12-bit DAC code. It must call `i2cBus.h`, never
   the HAL directly. See `CLAUDE.md`'s driver-layer section for the expected
   shape.
3. Use the existing `i2cDacSetCode` CLI command to exercise it without a
   debugger.
4. Verify against the VCU - the output should show up as a real analog value
   on the VCU's ADC.

The MCP4728 frame intentionally defers its analog-output update to LDAC. Find
and configure the appropriate GPIO, then strobe it after the I2C transfer.

## What is here

```
HIL_onboarding/
  Inc/
    bsp.h                Board pin/peripheral handle macros
    hilFreeRTOSConfig.h  FreeRTOS settings CubeMX would otherwise overwrite
    hilCli.h             CLI commands
    i2cBus.h             I2C transfer wrappers   <- what your driver calls
    spiBus.h             SPI transfer wrappers   <- unused by this task
    errorHandler.h       Fatal error handler
  Src/
    userInit.c           Pre-RTOS init hook, called from Cube's main.c
    mainTaskEntry.c      Main task: blinks the debug LED
    hilCli.c             i2cScan / i2cDacSetCode commands
    i2cBus.c
    spiBus.c
    errorHandler.c
  Cube-F7-Src-respin/    STM32CubeMX project (HIL_2026.ioc) + hand-derived Cube-Lib.mk
  board.mk
  CLAUDE.md              Code style rules for this directory
  README.md              You are here
```

## What was removed, and why it matters

Relative to `../HIL/`: `HIL_can.c/h`, `canReceive.c/h`, `canInject.c/h` and
`canHeartbeatStub.c` are gone, `userCan.c`/`userCanF7.c` are dropped from
`COMMON_LIB_SRC`, and the `canSendTask` entry is removed from the CubeMX task
list.

The mechanism that makes that possible is one line in `Inc/bsp.h`:

```c
#define DISABLE_CAN_FEATURES
```

`common/Src/debug.c` guards its CAN-dependent parts on that - the heartbeat
CLI commands and the UART-over-CAN tunnel in `printTask`. Without it, `debug.c`
would need `userCan.c` and a generated `<board>_dtc.h`, and the board would
not link.

`Src/errorHandler.c` exists for the same reason. `common/Src/debug.c` calls
`handleError()`, which expands to `_handleError()`. The shared implementation
in `common/Src/generalErrorHandler.c` needs a generated DTC header, so this
board provides its own small version instead: print the failing file and line
to the UART, then stop.

CAN1/CAN2/CAN3 are still configured in the `.ioc` and still initialised by
`main.c`. That is harmless - the peripherals are set up and nobody talks to
them. Leaving them alone keeps the CubeMX project identical to `../HIL/`'s.

## FreeRTOS tasks

Four, declared in the CubeMX task table:

| Task | Priority | Stack | Entry function | Code generation |
|---|---|---|---|---|
| `defaultTask` | Normal | 256 | `defaultTaskFunction` | Default |
| `mainTask` | Normal | 1000 | `mainTaskFunction` | As external |
| `printTaskName` | Low | 1000 | `printTask` | As external |
| `cliTaskName` | Low | 1000 | `cliTask` | As external |

Keep Code Generation on **`As external`** for the three real ones. Their
bodies live in `Src/mainTaskEntry.c` and `common/Src/debug.c`; setting one
back to `Default` makes CubeMX generate a second body for the same symbol.
That does not fail the build, because `common/tail.mk` passes `-z muldefs`,
which silently keeps whichever definition comes first in link order. The
generated stub is `for(;;) { osDelay(1); }`, so if it won you would get a
board that boots and does nothing, with no error.

`defaultTask` cannot be deleted - CubeMX requires one task and greys out both
Delete and Code Generation for it. It costs ~1 KB of heap and does nothing.
Leave it.

## CLI

Serial adapter on the debug UART (`USART3`) at 230400 baud. `help` lists
everything.

Free from `common/Src/debug.c`: `heap`, `taskList`, `stats`, `reset`,
`version`. This board adds, in `Src/hilCli.c`:

```
i2cScan  <bus>                     Scan bus 1-3, list responding 7 bit addresses
i2cDacSetCode <code>              Call the configured MCP4728 driver output
```

`i2cDacSetCode` takes its 12-bit code in hexadecimal. The MCP4728's default
7-bit address is configured in its driver; the bus wrapper may require that
address to be shifted before transmission.

`taskList` prints each task's minimum free stack, which is how you would
justify the 1000-word stack sizes rather than leaving them at a guess.

## Building

Not part of the root `Makefile` or CI. From the repo root:

```
make -f HIL_onboarding/board.mk HIL_onboarding
```

Output lands in `Bin/HIL_onboarding/Release/`. Flash with:

```
make -f HIL_onboarding/board.mk load LOAD_TARGET=HIL_onboarding
```

Last known-good build: 63000 B text, 516 B data, 59904 B bss.

## Known issues inherited from ../HIL/

- `SPI5` is configured as 4 bit frames at 50 MHz (`SPI4` is correct at 8 bit
  / 1.5625 MHz). Irrelevant to this task, which is I2C only, but do not
  assume SPI5 works until that is fixed in CubeMX.
- `PWM_1`..`PWM_7` (PG2-PG8) are net-labelled on the schematic but have no
  timer alternate function on this part, so they are GPIO only. `PWM_8`/
  `PWM_9`/`PWM_10` (PC6/PC7/PC8, TIM8) are the real ones.
