#ifndef DMRNG_PORT_H
#define DMRNG_PORT_H
#include <stdint.h>
#include "dmrng_port_defs.h"
/* Calls are serialized by the core. acquire/release own the peripheral and
 * its clock reservation. A fault stays latched until release + acquire. */
dmod_dmrng_port_api(1.0, int, _acquire, (uint32_t instance));
dmod_dmrng_port_api(1.0, int, _release, (uint32_t instance));
dmod_dmrng_port_api(1.0, int, _read_word, (uint32_t instance, uint32_t timeout_ms, uint32_t* word));
#endif
