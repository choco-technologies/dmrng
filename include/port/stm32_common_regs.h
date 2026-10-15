#ifndef DMRNG_STM32_COMMON_REGS_H
#define DMRNG_STM32_COMMON_REGS_H
#include <stdint.h>
/* RM0090/RM0385: identical legacy RNG IP, AHB2 gate/reset bit 6. */
#define STM32_RNG_BASE 0x50060800u
#define STM32_RCC_BASE 0x40023800u
#define STM32_RCC_AHB2RSTR (STM32_RCC_BASE + 0x14u)
#define STM32_RCC_AHB2ENR (STM32_RCC_BASE + 0x34u)
#define STM32_RCC_RNGEN (1u << 6)
#define STM32_RNG_CR_RNGEN (1u << 2)
#define STM32_RNG_SR_DRDY (1u << 0)
#define STM32_RNG_SR_ERRORS ((1u << 1) | (1u << 2) | (1u << 5) | (1u << 6))
#define STM32_RNG_REG(offset) (*(volatile uint32_t*)(uintptr_t)(STM32_RNG_BASE + (offset)))
#define STM32_RCC_REG(address) (*(volatile uint32_t*)(uintptr_t)(address))
#endif
