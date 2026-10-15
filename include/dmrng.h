#ifndef DMRNG_H
#define DMRNG_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dmrng_defs.h"

typedef struct dmrng* dmrng_t;
typedef struct { uint32_t instance; uint32_t timeout_ms; } dmrng_config_t;
#define DMRNG_DEFAULT_TIMEOUT_MS 100u
#define DMRNG_MAX_TIMEOUT_MS 1000u
/* One context owns the physical RNG. NULL config selects instance 0/100 ms.
 * Lifetime changes must be externally serialized against users of the handle. */
dmod_dmrng_api(1.0, dmrng_t, _create, (const dmrng_config_t* config));
/* Returns -EBUSY while device handles are open; NULL is a successful no-op. */
dmod_dmrng_api(1.0, int, _destroy, (dmrng_t context));
dmod_dmrng_api(1.0, bool, _is_valid, (dmrng_t context));
/* Full buffer or negative errno. On failure the whole output is cleared.
 * Timeout bounds each hardware word wait; callable from task context only. */
dmod_dmrng_api(1.0, int, _read, (dmrng_t context, void* buffer, size_t size));
/* TLS-friendly access to the configured instance (no software fallback).
 * Returns -ENODEV until a context has been created, e.g. from its board INI. */
dmod_dmrng_api(1.0, int, _fill, (void* buffer, size_t size));
#endif
