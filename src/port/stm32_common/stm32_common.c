#include "dmod.h"
#include "dmrng_port.h"
#include "dmclk_port.h"
#include "port/stm32_common_regs.h"
#include "stm32_common.h"
#include <errno.h>

static bool acquired;
static bool clock_reserved;
static bool gate_was_enabled;
static bool have_previous;
static uint32_t previous;
static int fault;

static void reset_peripheral(void)
{
    Dmod_EnterCritical();
    STM32_RCC_REG(STM32_RCC_AHB2RSTR) |= STM32_RCC_RNGEN;
    STM32_RCC_REG(STM32_RCC_AHB2RSTR) &= ~STM32_RCC_RNGEN;
    Dmod_ExitCritical();
}

dmod_dmrng_port_api_declaration(1.0, int, _acquire, (uint32_t instance))
{
    if (instance != 0) return -ENODEV;
    if (acquired || clock_reserved) return -EBUSY;
    /* Do not acquire a clock reservation or reset an externally-owned RNG. */
    if ((STM32_RCC_REG(STM32_RCC_AHB2ENR) & STM32_RCC_RNGEN) &&
        (STM32_RNG_REG(0) & STM32_RNG_CR_RNGEN)) return -EBUSY;
    dmclk_frequency_t actual;
    int rc = dmclk_port_acquire_domain(dmclk_domain_rng, 48000000u, 0, &actual);
    if (rc != 0) return rc;
    clock_reserved = true;
    Dmod_EnterCritical();
    gate_was_enabled = (STM32_RCC_REG(STM32_RCC_AHB2ENR) & STM32_RCC_RNGEN) != 0;
    if (!gate_was_enabled) STM32_RCC_REG(STM32_RCC_AHB2ENR) |= STM32_RCC_RNGEN;
    (void)STM32_RCC_REG(STM32_RCC_AHB2ENR);
    Dmod_ExitCritical();
    reset_peripheral();
    STM32_RNG_REG(0) = STM32_RNG_CR_RNGEN;
    previous = 0;
    have_previous = false;
    fault = 0;
    acquired = true;
    return 0;
}

dmod_dmrng_port_api_declaration(1.0, int, _release, (uint32_t instance))
{
    if (instance != 0 || !acquired) return -EINVAL;
    STM32_RNG_REG(0) = 0;
    if (!gate_was_enabled) {
        Dmod_EnterCritical();
        STM32_RCC_REG(STM32_RCC_AHB2ENR) &= ~STM32_RCC_RNGEN;
        Dmod_ExitCritical();
    }
    int rc = dmclk_port_release_domain(dmclk_domain_rng);
    if (rc != 0) return rc; /* Keep ownership so a failed release can be retried. */
    clock_reserved = false;
    acquired = false;
    have_previous = false;
    previous = 0;
    return 0;
}

static int next_word(uint32_t timeout_ms, uint32_t* word)
{
    /* Independent of scheduler ticks; also bounded before the RTOS starts.
     * CLK48 is 48 MHz; hardware needs only a few microseconds per word. */
    uint32_t attempts = timeout_ms * 1000u;
    while (attempts-- != 0) {
        uint32_t status = STM32_RNG_REG(4);
        if (status & STM32_RNG_SR_ERRORS) return -EIO;
        if (status & STM32_RNG_SR_DRDY) {
            uint32_t value = STM32_RNG_REG(8);
            if (STM32_RNG_REG(4) & STM32_RNG_SR_ERRORS) return -EIO;
            *word = value;
            return 0;
        }
        dmclk_port_delay_us(1);
    }
    return -ETIMEDOUT;
}

dmod_dmrng_port_api_declaration(1.0, int, _read_word,
    (uint32_t instance, uint32_t timeout_ms, uint32_t* word))
{
    if (word == NULL || timeout_ms == 0 || timeout_ms > 1000) return -EINVAL;
    *word = 0;
    if (instance != 0 || !acquired) return -ENODEV;
    if (fault != 0) return fault;
    if (!have_previous) {
        fault = next_word(timeout_ms, &previous); /* Discard first sample. */
        if (fault != 0) { STM32_RNG_REG(0) = 0; return fault; }
        have_previous = true;
    }
    uint32_t value;
    fault = next_word(timeout_ms, &value);
    if (fault == 0 && value == previous) fault = -EIO;
    if (fault != 0) {
        STM32_RNG_REG(0) = 0; /* Fail closed until destroy/recreate. */
        return fault;
    }
    previous = value;
    *word = value;
    return 0;
}

int stm32_rng_port_deinit(void)
{
    return acquired || clock_reserved ? -EBUSY : 0;
}
