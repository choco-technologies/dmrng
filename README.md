# dmrng

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmrng/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmrng/actions/workflows/ci.yml)

Hardware entropy driver for DMOD. STM32F4/F7 ports share the legacy RNG IP
implementation, with a read-only `/dev/rng` device and a small API suitable
for feeding a TLS library's entropy collector. No software random fallback.

## Build

```sh
cmake -S . -B build/stm32f7 -DDMOD_CPU_FAMILY=stm32f7
cmake --build build/stm32f7 --parallel
```

Use `-DDMOD_DIR=/path/to/dmod` for a local SDK. STM32F4 uses
`-DDMOD_CPU_FAMILY=stm32f4` and a separate build directory.
Build outputs are `dmrng`, `dmrng_port` and `test_dmrng`, with `.dmf`,
`.dmfc`, dependency lists and release packages.

## Use on a board

Include the selected preset in the board's flash dependency list:

```text
dmrng driver=board/stm32f746g-disco/rng.ini
test_dmrng
```

The clock driver must already supply CLK48. Instance creation reserves the
48 MHz RNG domain through `dmclk_port` 1.2; it does not retune a shared PLL.
The board preset creates `/dev/rng` (also numbered as `dmrng0`). Install one
MCU or board preset, not both. See [configs](configs/README.md).

```c
#include "dmrng.h"
uint8_t entropy[32];
int result = dmrng_fill(entropy, sizeof(entropy));
/* result == 0: all bytes available; otherwise output has been cleared. */
```

`dmrng_fill` uses the board-configured instance. Applications without a
configured device can own a context with `dmrng_create(NULL)`, use
`dmrng_read`, then `dmrng_destroy`. Only one context may own RNG instance 0.
Read calls are synchronized and run in task context. Context owners must
serialize destruction against direct handle users; `dmrng_fill` does this
internally. Destruction fails with `-EBUSY` while VFS handles remain open.

## Failure behavior

Hardware seed/clock errors, repeated consecutive 32-bit words and timeouts
are returned as negative errno values. The first word after startup is
withheld for the continuous comparison. A hardware fault remains latched
until the instance is destroyed and recreated; no automatic error clearing
or pseudo-random substitution is performed. An unsuccessful read clears its
whole output, including bytes already produced before the fault.

The configured timeout bounds the polling budget for each hardware word,
not the total request or time spent waiting on mutexes. Interrupt/task
preemption may extend elapsed wall-clock time. Polling avoids dependence on
an RTOS tick frequency; it does not disable interrupts during generation.

## Tests

```sh
cmake -S . -B build/native -DDMOD_CPU_FAMILY=x86_64
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure
```

The native-only deterministic port supports error injection and is excluded
from hardware CI/release matrices. It is not an entropy source. On a configured
STM32 board run `test_dmrng`; see [test report](docs/testing.md).

STM32F746G-DISCO has been tested on hardware. STM32F4 compiles but has not
been physically validated. Functional sample checks are not statistical
certification or proof of cryptographic entropy quality.

## Layout

```text
configs/{mcu,board}/       MCU and board INI presets
include/                  Public API, port contract, STM32 register definitions
src/dmrng.c               API, INI parsing, read-only dmdrvi device
src/port/stm32_common/     Shared hardware access and fault handling
src/port/{stm32f4,stm32f7}/ Family lifecycle wrappers/toolchain selection
src/port/x86_64/          Deterministic test-only port
tests/                    Core tests and board diagnostic
```

[API](docs/api-reference.md) · [Port implementation](docs/port-implementation.md)

Author: Patryk Kubiak. License: MIT.
