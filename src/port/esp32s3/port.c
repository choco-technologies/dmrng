#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmrng_port.h"
#include "dmclk_port.h"
#include <errno.h>
#include <stdint.h>

/*
 * dmrng_port for the ESP32-S3: WDEV_RND_REG, a 32-bit word of the hardware random generator.
 *
 * The register is a PRNG that takes 2 bits of entropy from a hardware source on every APB clock cycle: the noise of the RF
 * subsystem while the Wi-Fi or the Bluetooth is on, or - what is done here, as the bootloader of ESP-IDF does - the 8 MHz RC
 * oscillator, which ESP-IDF documents as enough for strong random numbers (it adds the SAR ADC as insurance; that part, with its
 * internal analog bus, is not done here). Without a source the register is plain pseudo-random. A word must not be taken faster than
 * the entropy comes in: ESP-IDF waits 1778 APB cycles (22 us at 80 MHz) between the words of the S3; so does the port (25 us).
 *
 * Only instance 0. There is no ready flag and no error flag: the timeout of the API is not needed, and a word equal to the previous
 * one (probability 2^-32) is a fault, as in the STM32 port.
 */
#define SYSCON_WIFI_CLK_EN_REG      (*(volatile uint32_t *)0x60026014UL)
#define WIFI_CLK_RNG_EN             (1UL << 15)
#define RTC_CNTL_CLK_CONF_REG       (*(volatile uint32_t *)0x60008074UL)
#define RTC_CNTL_DIG_CLK8M_EN       (1UL << 10)
#define WDEV_RND_REG                (*(volatile uint32_t *)0x6003507CUL)
#define WORD_SPACING_US             25U

static bool acquired;
static bool rng_clock_was_on;
static bool rc8m_was_on;
static bool have_previous;
static uint32_t previous;
static int fault;

dmod_dmrng_port_api_declaration(1.0, int, _acquire, (uint32_t instance))
{
    if (instance != 0)
        return -ENODEV;
    if (acquired)
        return -EBUSY;
    Dmod_EnterCritical();
    rng_clock_was_on = (SYSCON_WIFI_CLK_EN_REG & WIFI_CLK_RNG_EN) != 0;
    rc8m_was_on = (RTC_CNTL_CLK_CONF_REG & RTC_CNTL_DIG_CLK8M_EN) != 0;
    SYSCON_WIFI_CLK_EN_REG |= WIFI_CLK_RNG_EN;
    RTC_CNTL_CLK_CONF_REG |= RTC_CNTL_DIG_CLK8M_EN;
    Dmod_ExitCritical();
    have_previous = false;
    previous = 0;
    fault = 0;
    acquired = true;
    return 0;
}

dmod_dmrng_port_api_declaration(1.0, int, _release, (uint32_t instance))
{
    if (instance != 0 || !acquired)
        return -EINVAL;
    Dmod_EnterCritical();
    if (!rng_clock_was_on)
        SYSCON_WIFI_CLK_EN_REG &= ~WIFI_CLK_RNG_EN;
    if (!rc8m_was_on)
        RTC_CNTL_CLK_CONF_REG &= ~RTC_CNTL_DIG_CLK8M_EN;
    Dmod_ExitCritical();
    acquired = false;
    have_previous = false;
    previous = 0;
    return 0;
}

static uint32_t next_word(void)
{
    dmclk_port_delay_us(WORD_SPACING_US);
    return WDEV_RND_REG;
}

dmod_dmrng_port_api_declaration(1.0, int, _read_word, (uint32_t instance, uint32_t timeout_ms, uint32_t *word))
{
    if (word == NULL || timeout_ms == 0 || timeout_ms > 1000)
        return -EINVAL;
    *word = 0;
    if (instance != 0 || !acquired)
        return -ENODEV;
    if (fault != 0)
        return fault;
    if (!have_previous)
    {
        previous = next_word();                         /* the first sample is discarded */
        have_previous = true;
    }
    uint32_t value = next_word();
    if (value == previous)
    {
        fault = -EIO;                                   /* fail closed until release + acquire */
        return fault;
    }
    previous = value;
    *word = value;
    return 0;
}

int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    return 0;
}

int dmod_deinit(void)
{
    return acquired ? -EBUSY : 0;
}
