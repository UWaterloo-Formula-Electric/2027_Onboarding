# HIL DAC onboarding

The MCP4728 driver writes the same requested 12-bit code to channels A and B
using I2C1. The existing CLI accepts a code (`i2cDacSetCode`) or millivolts
(`i2cDacSetVoltage`). Independent A/B voltage selection is not exposed by
this starter interface.

## Driver behavior

Each channel is a three-byte Multi-Write record. For code `0xABC`, the payload
is `41 0A BC 43 0A BC`: channel A then channel B. The HAL sends the device
address separately (`0x60` shifted left once, `0xC0`). There is no seventh
payload byte. Reference is VDD, normal power mode, and UDAC is high to defer
output updates. After a successful transfer, PF10 (LDAC) goes low for at
least one scheduler tick and returns high. Failed transfers do not pulse LDAC.

PF10 is configured as a push-pull output initially high in both the generated
GPIO source and the CubeMX `.ioc` configuration. Initialization also parks it
high before the first write. LDAC updates all DAC output registers; this driver
only writes the A and B input registers. EEPROM is not written.

The millivolt conversion assumes a 3300 mV DAC supply. For example, 1650 mV
maps to code 2048. Confirm the supply on hardware and adjust
`I2C_DAC_FULL_SCALE_MV` if necessary; code 4095 produces slightly less than VDD.

Reference: [Microchip MCP4728 datasheet](https://ww1.microchip.com/downloads/en/devicedoc/22187e.pdf),
section 5.6.2 and Figure 5-8 (Multi-Write), and the LDAC timing specifications.

## Software validation

From the repository root, run the hardware-independent driver tests:

```sh
python3 HIL_onboarding/tests/test_i2c_dac.py
```

These compile the real driver with HAL fakes and check all 4096 valid codes,
all 3300 valid millivolt inputs, out-of-range rejection, I2C1/address/timeout,
transfer failure behavior, probe, initialization and LDAC ordering. The test
runs with 1000 Hz and 100 Hz scheduler ticks, including the minimum-tick fallback.

With the UWFE ARM toolchain available, build the standalone project:

```sh
cd HIL_onboarding
make -j4
```

Or from the repository root using the existing Docker image:

```sh
docker run --rm -v "$PWD:/workspace" -w /workspace/HIL_onboarding \
  ghcr.io/uwaterloo-formula-electric/uwfe-dev:stable \
  make -j4 BUILD_DIR=/tmp/hil-build
```

The temporary object directory avoids modifying object files tracked in the
starter repository. Firmware outputs are under `HIL_onboarding/Bin/Release/`.

## Bench validation (requires HIL board and debugger)

1. Connect the board/debugger with the team's guidance and flash the built ELF
   using `make flash` on the host from `HIL_onboarding`.
2. Open the board's serial CLI (115200 baud), e.g.
   `make terminal PORT=/dev/cu.YOUR_SERIAL_DEVICE` on macOS, substituting the
   actual serial device path.
3. Run `i2cDacProbe`; expect `DAC acknowledged`. Confirm the device address
   with `i2cScan 1` if it does not respond.
4. Run `i2cDacSetCode 0x000`, `i2cDacSetVoltage 1650`, and
   `i2cDacSetCode 0xFFF`. Measure both VOUTA and VOUTB: approximately 0 V,
   1.65 V, and just below the measured VDD, respectively.
5. Use a logic analyzer to confirm address 0x60 on I2C1, the two three-byte
   records, and LDAC staying high through transmission followed by a low pulse.
6. Optionally use `i2cDacRamp 100` and observe both outputs sweep together.

A successful build and fake-HAL tests do not verify wiring, acknowledgements,
measured output voltages, or signal timing on the physical board.
