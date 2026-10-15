#define DMOD_ENABLE_REGISTRATION ON
#include "dmrng_port.h"
#include "dmod.h"
#include "../stm32_common/stm32_common.h"
int dmod_init(const Dmod_Config_t* config)
{
    (void)config;
    return 0;
}
int dmod_deinit(void)
{
    return stm32_rng_port_deinit();
}
