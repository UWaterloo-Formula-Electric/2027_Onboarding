# Working in HIL_onboarding/

An onboarding sandbox (see `README.md`) for writing an I2C DAC driver. Code
here should still read as if it belongs in this codebase next to `bmu/`,
`vcu/`, `pdu/` - a maintainer familiar with those boards should not be able
to tell it was written differently.

There is no CAN on this board, so ignore any CAN advice below that survived
from the parent `HIL/` copy.

**Important caveat**: `bmu`/`vcu`/`pdu` are not perfectly uniform — they
were clearly written by different people across different years, and it
shows (indentation varies, brace placement varies, some files are
snake_case, some are camelCase). This file distills the *majority* pattern
or the *most disciplined* example where the codebase itself is split, and
states that as the single target style for new HIL code, rather than
pretending the reference codebase has no drift. Where a rule below picks one
option over another, that's a deliberate call to give HIL one consistent
style going forward — not a claim that the other option never appears
elsewhere in this repo.

There is no `.clang-format` anywhere in this repo (checked — none exists),
so nothing will auto-enforce this. Read it back before treating a change as
done.

## Canonical files to pattern-match against

When unsure how something should look, go read the real thing rather than
guessing:
- `bmu/Src/userInit.c` — pre-RTOS init hook shape, error-checking pattern.
- `bmu/Inc/bsp.h` — BSP macro style, `#if IS_BOARD_F7 / #else #error` guard.
- `bmu/Src/mainTaskEntry.c` — trivial task shape/period pattern.
- `bmu/Src/controlStateMachine.c` + `common/Inc/state_machine.h` — FSM
  pattern, if HIL ever grows one.
- `pdu/Src/controlStateMachine_mock.c` — CLI command shape: the
  `CLI_Command_Definition_t` struct, `FreeRTOS_CLIGetParameter` +
  `sscanf` parsing, and `COMMAND_OUTPUT` for output.
- `bmu/Src/F7_Src/ltc_common.c` — the cleanest example of the driver-layer
  pattern in this repo. Read this before writing the DAC driver (see
  "Driver-layer style" below).

## Formatting

- **Indentation: 4 spaces, no tabs.** This is the majority convention
  (`bmu/Src/batteries.c`, `bmu/Src/controlStateMachine.c`,
  `vcu/Src/brakeAndThrottle.c`, `pdu/Src/cooling.c` all use it). A couple of
  reference files (`vcu/Src/traction_control.c`, `endurance_mode.c`) are
  tab-indented and one (`bmu/Src/fanControl.c`) is 2-space — don't follow
  those, they're outliers.
- **Function braces on their own line** (`void foo()\n{`), matching
  `bmu/Src/userInit.c`, `bmu/Src/controlStateMachine.c`,
  `bmu/Src/batteries.c`, and what's already in `Src/*.c`. The reference
  codebase is genuinely ~50/50 split on this (`contactorControl.c` uses
  same-line braces) — this file picks next-line as HIL's standard so it's at
  least internally consistent.
- **`if`/`for`/`while`/`switch` braces on the same line** (`if (x) {`) —
  this is the one bracing rule that actually holds consistently across the
  reference codebase.
- **Space before the paren** in control statements: `if (x)`, not `if(x)`.
- Wrap a `switch` `case` body in its own `{ }` block when it has any local
  state, matching `bmu/Src/controlStateMachine.c`'s `case` blocks.
- No enforced line-length limit exists in this codebase, but wrap long
  expressions by hand for readability rather than letting a single line run
  very long (see `vcu/Src/brakeAndThrottle.c`'s manually-wrapped
  `map_range_float` calls for the expected shape).

## Comments: keep them sparse

**This codebase comments lightly. Match that.** Verbose commentary clutters
the code and does not look like anything else in the repo. Measured comment
lines as a percentage of code lines:

| File | Ratio |
|---|---|
| `bmu/Inc/bsp.h` | 2% |
| `pdu/Inc/bsp.h` | 9% |
| `vcu/Inc/bsp.h` | 12% |
| `bmu` + `vcu` + `pdu` `Src/*.c` aggregate | 16% |

Headers land around **2-12%**, `.c` files around **16%**, excluding the
Doxygen file header. If a new file is well above that, it is over-commented
- go delete, don't rationalize.

What the reference boards actually use:
- **Short section headers**, one line, a few words: `// AUX`,
  `// Fault LEDs`, `// BUTTON LEDS`, `// Read pins (we have 4 buttons)`.
- **Brief trailing comments** where a name genuinely isn't enough:
  `#define STATS_TIM_HANDLE htim3      // For FreeRTOS`,
  `#define ADC_TIM_HANDLE htim6        // For ADC1 & ADC3 (acts as trigger source)`.
- **Datasheet citations** next to a nontrivial constant or bit position -
  `/* See Table 2 in BQ24650RVAT's datasheet for logic */`. Keep doing this
  one; it is the most valuable comment type here.
- The **Doxygen file header** (see "Header file conventions" below).

What to avoid, all of which has actually been written into HIL and then
deleted again:
- Multi-line prose blocks explaining *why* a choice was made, or what was
  considered and rejected. That belongs in the commit message or `README.md`,
  not above a `#define`. `bmu/Inc/bsp.h` has ~60 macros and 2 comment lines
  in the entire body.
- Narrating hardware facts already obvious from the identifier. `#define
  CAN_HANDLE hcan3` needs nothing; a three-line note on which bus has which
  interrupts enabled is clutter.
- Restating what a line plainly does.
- Multi-line `// TODO:` blocks. One line, per the format in "Function
  structure / error handling" below.
- Long comments justifying a deviation from convention. If a deviation needs
  a paragraph to defend, raise it instead of writing the paragraph.

When a real constraint is genuinely non-obvious and would cost someone an
afternoon (a timer another module secretly owns, a pin that cannot do the
thing its net name implies), one short line is right. The bar is "would a
competent embedded dev reading this file be surprised" - not "is this
interesting".

## Naming conventions

- **Functions**: camelCase, verb-first (`getX`, `checkX`, `initX`,
  `sendCanMessage`). The reference codebase has snake_case pockets
  (`vcu/Src/traction_control.c`, `endurance_mode.c`) — treat those as
  outliers, not a second valid style, for new HIL code.
- **Locals/parameters**: camelCase.
- **Globals**: plain camelCase, no prefix (`maxChargeCurrent`,
  `regenEnabled`). Don't invent a `g_`-style prefix — only one file in the
  whole reference codebase uses one, it's not the norm.
- **Structs/typedefs**: prefer `typedef struct { ... } Foo_t;` (anonymous
  struct, `_t` suffix) — this is the more common of the two patterns found
  (`pdu/Inc/loadSensor.h`, `pdu/Inc/lvMeasure.h`).
- **Enums**: `_t` suffix, PascalCase type name. Enum *values*: ALL_CAPS for
  plain/flag enums (`BOTS_FAILED_BIT`); if HIL ever grows an FSM, use the
  `PREFIX_Mixed_Case` style from `bmu/Inc/controlStateMachine.h`
  (`STATE_Self_Check`, `EV_HV_Toggle`) instead of ALL_CAPS, matching that
  convention specifically for states/events.
- **Macros/`#define` constants**: ALL_CAPS, always. This is the one naming
  rule that is fully consistent across the entire reference codebase — no
  exceptions to look for here.
- **`static` for file-local helpers**: mark them `static`. The reference
  codebase is inconsistent about this (`vcu/Src/brakeAndThrottle.c` does it,
  `bmu/Src/batteries.c` mostly doesn't) — follow the cleaner example and do
  it consistently in HIL even though not everything upstream does.

## Header file conventions

- Include guard: `#ifndef FOO_H` / `#define FOO_H` (blank line between them
  is common but optional), closing with `#endif /* FOO_H */`. **Do not use a
  leading double underscore** (`__FOO_H`) — that's a reserved identifier.
  Several older files in this repo (including `common/sample-bsp.h` and the
  real boards' `bsp.h`) do use `__BSP_H`; `HIL/Inc/bsp.h` intentionally uses
  `HIL_BSP_H` instead, and new HIL headers should follow that, not the
  legacy pattern.
- Include ordering: no rule is enforced repo-wide. Default to: the file's
  own header first (for a `.c` file), then other project headers, then
  system/HAL headers — or just match whatever the file you're editing
  already does. Don't invent alphabetical ordering; only one file in the
  reference codebase does that and it's not the norm.
- Doxygen file-header block (`@file`/`@author`/`@brief`/`@details`): keep
  using it for new files, matching `bmu/Src/userInit.c` and
  `HIL/Src/userInit.c`/`mainTaskEntry.c`. Fine to omit for a very short,
  self-explanatory stub file.

## Function structure / error handling

- Return `HAL_StatusTypeDef` for anything that can fail, propagated with the
  standard idiom used throughout the reference codebase:
  ```c
  if (foo() != HAL_OK) {
      ERROR_PRINT("Failed to foo\n");
      return HAL_ERROR;
  }
  ```
- If HIL ever grows an FSM, its transition functions should return
  `uint32_t` (the next state), matching `bmu/Src/controlStateMachine.c` —
  that's a deliberately different convention from the `HAL_StatusTypeDef`
  rule above, scoped specifically to FSM transition functions.
- `// TODO: <description>` — capital TODO, colon, one space. The reference
  codebase has several inconsistent variants (`TODO -`, `Todo:`); standardize
  on this one for new code.
- Don't leave a function whose real behavior is commented out while its
  surrounding code (a log message, a caller, a name) implies it still works.
  The research pass found a real example of this upstream — a
  `motorOverheated()`-style function that still logs "Overheating!" but had
  its actual protective action commented out, silently doing nothing. If
  something is a stub, make that obvious (an explicit `// TODO:` and a
  visibly-a-stub return), don't leave a misleading half-implementation.

## Logging

`common/Inc/debug.h` provides `DEBUG_PRINT` / `DEBUG_PRINT_ISR` /
`ERROR_PRINT` / `ERROR_PRINT_ISR` / `CONSOLE_PRINT` / `COMMAND_OUTPUT`.
Important fact about the mechanism: `DEBUG_PRINT` and `ERROR_PRINT` expand
to the *exact same underlying macro* — there's no compiler-enforced
severity difference between them. The distinction is a convention we keep
by hand, so be deliberate about it:

- **`ERROR_PRINT`**: genuine failure paths only — a HAL call failed, a fault
  tripped, a sanity check failed.
- **`DEBUG_PRINT`**: routine informational/trace logging — state entered,
  action started, periodic status.
- **Always use the `_ISR` variant from interrupt context** (any HAL
  callback). The reference codebase is fully disciplined about this — zero
  violations across every `*_Callback` function in `bmu`/`vcu`/`pdu`.
- `CONSOLE_PRINT` is effectively unused in board code. Inside a CLI command
  handler use `COMMAND_OUTPUT`, and note it *overwrites* `writeBuffer` on
  every call — for multi-line output, return `pdTRUE` to be re-invoked, the
  way `i2cScanCommand` does. Help strings follow
  `"<cmd> <args>:\r\n  <description>\r\n"`.
- **Message format**: capitalize the first letter, use "Failed to X"
  phrasing for failures, end with `\n`. (The reference codebase mixes `\n`
  and `\r\n` inconsistently — just use `\n` and be consistent within HIL.)
  Don't bother prefixing every message with a module/function name; the
  file/function context already tells you that — only a couple of reference
  files do heavy prefixing and it's not the norm.
- **Format specifiers**, matching the dominant reference-codebase pattern:
  - `uint32_t` → `%lu` (decimal).
  - Bitmasks/notification words → `0x%lX`.
  - Small register/byte values → `0x%x` or `%02X`.
  - Floats → bare `%f`, no precision needed.
  - 64-bit CAN signal values → `PRIu64` from `<inttypes.h>`, not `%llu`
    (one reference file uses both inconsistently — prefer `PRIu64`, it's the
    more portable choice already present in this codebase).
- Log on failure, not on every success. `pdu/Src/loadSensor.c` is the model:
  a periodic publish logs only when it fails.

## Driver-layer style — read this before writing the DAC driver

Follow the pattern from BMU's cell-monitor drivers:

- **Never call `HAL_I2C_*` / `HAL_SPI_*` directly.** `Src/i2cBus.c` already
  owns every HAL I2C call on this board; your driver calls `i2cWriteReg()` /
  `i2cReadReg()` / `i2cIsDeviceReady()` from `i2cBus.h`. This mirrors how
  every LTC driver goes through `ltc_common.c`'s single `spi_tx_rx`.
- **One file per device**, named after the part, not after the bus. There is
  no `i2cDevices.c`; there is `dacMcp4728.c` (or whatever the part is).
- Public driver API shape: `HAL_StatusTypeDef <prefix>_<verb>_<noun>(...)`
  for anything that can fail (`<prefix>_init`, `<prefix>_read_x`,
  `<prefix>_write_x`), with output data passed via an out-parameter pointer
  rather than returned by value.
- Register/command bytes as `#define`s, not enums or bitfield structs —
  matches every chip driver in this codebase.
- Cite the datasheet in a comment next to any nontrivial constant or bit
  position (e.g. `ltc_common.h`'s `VUV`/`VOV` derivation comments) — this is
  a genuinely useful habit the driver layer follows consistently.
- Give driver files a full Doxygen file header describing the physical chip
  (part number, what it does, how it's wired) — the driver layer is the
  most consistently well-documented part of this codebase; keep that up.

## Things NOT to "fix" without asking first

These are deliberate, documented in `README.md`:
- `#define DISABLE_CAN_FEATURES` in `Inc/bsp.h`. Removing it makes
  `common/Src/debug.c` require `userCan.c` and a generated `<board>_dtc.h`,
  and the board stops linking.
- No CAN anywhere, no `Gen/` directory, no DBC/DTC codegen
  (`DBC_CODEGEN = 0` in `board.mk`).
- `Src/errorHandler.c` replaces `common/Src/generalErrorHandler.c`, which
  needs a DTC header this board does not have.
- Code Generation stays `As external` for `mainTask`/`printTaskName`/
  `cliTaskName` in CubeMX. `Default` silently produces duplicate symbols —
  see `README.md`.
