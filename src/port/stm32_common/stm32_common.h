#ifndef DMRNG_STM32_COMMON_H
#define DMRNG_STM32_COMMON_H
/* The common file implements the public port API. Family wrappers only own
 * DMOD lifecycle; no interrupt handler is needed for bounded polling. */
int stm32_rng_port_deinit(void);
#endif
