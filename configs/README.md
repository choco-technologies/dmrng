# RNG configurations

The layout follows dmdma, dmsdio and dmi2c: `mcu/` describes the peripheral
available on a chip, and `board/<board>/rng.ini` provides the board preset.
RNG uses no external GPIO, so the board preset repeats the MCU settings.
Clock setup belongs to the board's clock configuration, not to these INIs.

The core parses these settings through dmini and registers a read-only dmdrvi
device. STM32F746G-DISCO is hardware-tested; other presets are not a claim of
physical validation.

| Board | MCU | Preset |
|-------|-----|--------|
| STM32F4-DISCOVERY | STM32F407VG | `board/stm32f4-discovery/rng.ini` |
| STM32F429I-DISCOVERY | STM32F429ZI | `board/stm32f429i-discovery/rng.ini` |
| STM32F746G-DISCO | STM32F746NG | `board/stm32f746g-disco/rng.ini` |
| NUCLEO-F767ZI | STM32F767ZI | `board/nucleo-f767zi/rng.ini` |

`mcu/` also contains STM32F746ZG, for custom boards using that package.
Only explicitly listed MCUs have presets: RNG availability must be checked
for the exact part, rather than inferred from its STM32F4/F7 family name.

## Fields

- `[rng]`: device alias exposed as `/dev/rng`.
- `driver_name=dmrng`: driver selected by dmdevfs.
- `driver_order=0`: create after the board clock setup (use an earlier order
  for the clock driver, as in dmod-boot's STM32F746G-DISCO configuration).
- `instance=0`: the single RNG peripheral, indexed from zero.
- `timeout_ms=100`: polling budget per hardware word (1..1000 ms); the driver must
  return an error when entropy cannot be obtained, never substitute a PRNG.

Install only one selected MCU or board preset into `/configs/drivers/dmrng/`.
Do not copy the entire presets tree into the active configuration directory:
that would request the same peripheral multiple times.

The `configs` resource in `dmrng.dmr` ships this whole reference tree in the
core package; dmf-get selects the actual file via `--config`, for example:

```sh
dmf-get install dmrng --config board/stm32f746g-disco/rng.ini --config-dir /configs/drivers/dmrng
```

A board dependency entry can use `dmrng driver=board/stm32f746g-disco/rng.ini`
with dmf-get's existing config-map mechanism when assembling a board configuration.

## Hardware references

- [ST AN4230: RNG overview and supported families](https://www.st.com/resource/en/application_note/an4230-introduction-to-random-number-generation-validation-using-the-nist-statistical-test-suite-for-stm32-mcus-and-mpus-stmicroelectronics.pdf)
- [STM32F746NG](https://www.st.com/en/microcontrollers-microprocessors/stm32f746ng.html)
- [STM32F767/777](https://www.st.com/en/microcontrollers-microprocessors/stm32f7x7.html)
