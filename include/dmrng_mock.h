#ifndef DMRNG_MOCK_H
#define DMRNG_MOCK_H
#include "dmrng_port_defs.h"
/* Test-only deterministic source. Never built into an STM32 port or released. */
dmod_dmrng_port_api(1.0, void, _mock_fail_after, (int words, int error));
#endif
