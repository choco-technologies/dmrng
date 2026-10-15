#ifndef DMRNG_INTERNAL_H
#define DMRNG_INTERNAL_H
#include "dmrng.h"
#include "dmosi.h"
#define DMRNG_MAGIC 0x524E4721u
struct dmrng {
    uint32_t magic;
    dmrng_config_t config;
    dmosi_mutex_t lock;
    uint32_t opens;
};
#endif
