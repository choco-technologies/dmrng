# dmrng hardware ports

The core owns configuration, synchronization, context lifetime, byte packing
and the dmdrvi device. The `dmrng_port` module owns physical RNG access.
`DMOD_CPU_FAMILY` selects the family before toolchain setup.

STM32F4/F7 use the same legacy RNG register layout (RM0090/RM0385):
`src/port/stm32_common/stm32_common.c` implements the public port API;
`include/port/stm32_common_regs.h` defines addresses and masks. Family
`port.c` files register that API once and provide module lifecycle hooks.
No interrupt vector is registered: reads use bounded polling with interrupts
left enabled. Common code must not define DMOD_ENABLE_REGISTRATION again.

The RNG domain is acquired at exactly 48 MHz through `dmclk_port` 1.2 before
changing AHB2 gate/reset bit 6. This borrows a compatible running CLK48
without reconfiguring a PLL shared with USB/SDMMC. A pre-enabled generator
is treated as externally owned and rejected. A pre-existing clock gate is
preserved on release. A failed domain release preserves ownership so release
can be retried. A module with a live instance refuses deinitialization.

The first ready word is retained only as a comparison baseline. Subsequent
words must differ. Status bits CECS, SECS, CEIS and SEIS are checked before
and after the data register read. Any error or bounded-poll timeout stops the
generator and latches a negative errno until release/reacquire. There is no
automatic recovery that could hide hardware health failures.

The STM32F746G-DISCO configuration has been physically tested; F4 is only
build-validated. Presence of RNG is checked by exact MCU preset, not inferred
for every STM32F4 part. New MCU variants require checking the reference manual,
clock mux and applicable errata before sharing the implementation.

## Another architecture

Add `src/port/<family>/{config.cmake,port.c}` and implement all three operations
in `dmrng_port.h`. An acquire failure must undo resources acquired during that
attempt; a release error must leave resources owned for retry. Return valid
hardware data only, never ticks, rand() or a fixed-seed PRNG. Register APIs in
one translation unit; keep callback definitions with their registration site.
Add MCU/board presets and validate both failure paths and actual hardware.

The `x86_64` port is exclusively for deterministic tests. CI and release
explicitly exclude it from their hardware matrix.

References: [RM0385](https://www.st.com/resource/en/reference_manual/dm00124865.pdf),
[ST RNG application note AN4230](https://www.st.com/resource/en/application_note/an4230-introduction-to-random-number-generation-validation-using-the-nist-statistical-test-suite-for-stm32-mcus-and-mpus-stmicroelectronics.pdf).
