# RNG validation

## Native

Configure with `DMOD_CPU_FAMILY=x86_64`, build and run CTest. The deterministic
mock is test-only and is excluded from all hardware release matrices.

Nine test steps cover zero/invalid arguments, unaligned odd-sized buffers and
guards, independent blocks, invalid config/duplicate ownership, sustained
reads, concurrent readers, clearing partial output on a latched error,
timeout propagation, and release/recreate.

## STM32F746G-DISCO, 2026-10-15

Hardware: STM32F746NG, ST-Link serial `0670FF535651727067072610`, UART
921600 8N1. Firmware: dmod-boot board `stm32f746g-disco`, with locally built
0.1 dmrng/dmrng_port/test_dmrng packages and its board RNG INI. OpenOCD
program/verify completed with `Verified OK`.

After boot `/dev/rng` was present. Three consecutive runs of `test_dmrng`
each passed **6/6** steps: two simultaneous read handles and write-open
rejection, 64 KiB sustained generation, invalid config/duplicate ownership,
different blocks, unaligned buffers/guards, zero/invalid requests.

Sample checksums from those runs: `e664211e`, `2fb8d01b`, `7d471318`.
The expected write-open rejection emits dmdevfs/FileOpen error messages.
No insufficient-stack or invalid-free diagnostics remained after reducing
the test's declared stack to 2048 bytes, which fits the console's stack.

The final revision was retested; full native and board transcripts are in
`tests/results/`.
F4 is cross-compiled only. These are functional checks, not NIST statistical
validation, entropy estimation, or TLS certification. Native injected failures
validate API behavior; they do not physically inject a seed/clock health fault
into the silicon.

## Reproduce

Build and embed the three modules plus the selected board INI in dmod-boot.
For an unreleased version use a local manifest pointing to `build/stm32f7/packages/`
and pass it as `DMBOOT_MANIFEST_URL`. Build with `BOARD=stm32f746g-disco`, then
`cmake --build build --target install-firmware`. Wait for the UART prompt and
run `test_dmrng` repeatedly. Use `scripts/test-hardware.py` for an automated
UART check and a plain-text transcript.
