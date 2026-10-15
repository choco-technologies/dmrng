#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmrng_port.h"
#include "dmrng_mock.h"
#include <errno.h>
static bool acquired;
static uint32_t sequence;
static int remaining;
static int failure;
dmod_dmrng_port_api_declaration(1.0, int, _acquire, (uint32_t instance))
{
    if (instance != 0) return -ENODEV;
    if (acquired) return -EBUSY;
    acquired = true; sequence = 0x12345678u; remaining = -1; failure = 0;
    return 0;
}
dmod_dmrng_port_api_declaration(1.0, int, _release, (uint32_t instance))
{
    if (instance != 0 || !acquired) return -EINVAL;
    acquired = false;
    return 0;
}
dmod_dmrng_port_api_declaration(1.0, int, _read_word,
    (uint32_t instance, uint32_t timeout_ms, uint32_t* word))
{
    if (word == NULL || timeout_ms == 0) return -EINVAL;
    *word = 0;
    if (instance != 0 || !acquired) return -ENODEV;
    if (remaining == 0) return failure;
    if (remaining > 0) --remaining;
    sequence ^= sequence << 13; sequence ^= sequence >> 17; sequence ^= sequence << 5;
    *word = sequence;
    return 0;
}
dmod_dmrng_port_api_declaration(1.0, void, _mock_fail_after, (int words, int error))
{
    remaining = words; failure = error;
}
int dmod_init(const Dmod_Config_t* config) { (void)config; return 0; }
int dmod_deinit(void) { return acquired ? -EBUSY : 0; }
