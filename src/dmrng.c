#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmrng_internal.h"
#include "dmrng_port.h"
#include <errno.h>
#include <string.h>

static dmrng_t active;
static dmosi_mutex_t registry_lock;

dmod_dmrng_api_declaration(1.0, bool, _is_valid, (dmrng_t ctx))
{
    return ctx != NULL && ctx == active && ctx->magic == DMRNG_MAGIC;
}

dmod_dmrng_api_declaration(1.0, dmrng_t, _create, (const dmrng_config_t* config))
{
    dmrng_config_t cfg = {0, DMRNG_DEFAULT_TIMEOUT_MS};
    if (config != NULL) cfg = *config;
    if (cfg.instance != 0 || cfg.timeout_ms == 0 || cfg.timeout_ms > DMRNG_MAX_TIMEOUT_MS)
        return NULL;
    if (registry_lock == NULL || dmosi_mutex_lock(registry_lock) != 0) return NULL;
    dmrng_t ctx = NULL;
    if (active == NULL) ctx = Dmod_Malloc(sizeof(*ctx));
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
        ctx->config = cfg;
        ctx->lock = dmosi_mutex_create(false);
        if (ctx->lock == NULL || dmrng_port_acquire(cfg.instance) != 0) {
            if (ctx->lock != NULL) dmosi_mutex_destroy(ctx->lock);
            Dmod_Free(ctx);
            ctx = NULL;
        } else {
            ctx->magic = DMRNG_MAGIC;
            active = ctx;
        }
    }
    dmosi_mutex_unlock(registry_lock);
    return ctx;
}

dmod_dmrng_api_declaration(1.0, int, _destroy, (dmrng_t ctx))
{
    if (ctx == NULL) return 0;
    if (!dmrng_is_valid(ctx)) return -EINVAL;
    int rc = dmosi_mutex_lock(registry_lock);
    if (rc != 0) return rc;
    rc = dmosi_mutex_lock(ctx->lock);
    if (rc == 0) {
        rc = ctx->opens ? -EBUSY : dmrng_port_release(ctx->config.instance);
        dmosi_mutex_unlock(ctx->lock);
        if (rc == 0) {
            active = NULL;
            ctx->magic = 0;
            dmosi_mutex_destroy(ctx->lock);
            Dmod_Free(ctx);
        }
    }
    dmosi_mutex_unlock(registry_lock);
    return rc;
}

static void clear_output(void* buffer, size_t size)
{
    volatile uint8_t* out = buffer;
    while (size--) *out++ = 0;
}

dmod_dmrng_api_declaration(1.0, int, _read, (dmrng_t ctx, void* buffer, size_t size))
{
    if (buffer == NULL && size != 0) return -EINVAL;
    if (!dmrng_is_valid(ctx)) {
        if (buffer != NULL) clear_output(buffer, size);
        return -EINVAL;
    }
    int rc = dmosi_mutex_lock(ctx->lock);
    if (rc != 0) {
        if (buffer != NULL) clear_output(buffer, size);
        return rc;
    }
    uint8_t* out = buffer;
    size_t remaining = size;
    while (remaining != 0) {
        uint32_t word;
        rc = dmrng_port_read_word(ctx->config.instance, ctx->config.timeout_ms, &word);
        if (rc != 0) break;
        size_t count = remaining < sizeof(word) ? remaining : sizeof(word);
        memcpy(out, &word, count);
        word = 0;
        out += count;
        remaining -= count;
    }
    if (rc != 0 && buffer != NULL) clear_output(buffer, size);
    dmosi_mutex_unlock(ctx->lock);
    return rc;
}

dmod_dmrng_api_declaration(1.0, int, _fill, (void* buffer, size_t size))
{
    if (buffer == NULL && size != 0) return -EINVAL;
    int rc = registry_lock == NULL ? -ENODEV : dmosi_mutex_lock(registry_lock);
    if (rc != 0) {
        if (buffer != NULL) clear_output(buffer, size);
        return rc;
    }
    if (active == NULL) {
        if (buffer != NULL) clear_output(buffer, size);
        rc = -ENODEV;
    } else {
        rc = dmrng_read(active, buffer, size);
    }
    dmosi_mutex_unlock(registry_lock);
    return rc;
}

int dmod_init(const Dmod_Config_t* config)
{
    (void)config;
    registry_lock = dmosi_mutex_create(false);
    return registry_lock == NULL ? -ENOMEM : 0;
}

int dmod_deinit(void)
{
    if (active != NULL) return -EBUSY;
    dmosi_mutex_destroy(registry_lock);
    registry_lock = NULL;
    return 0;
}

/* Device interface. Registrations share this translation unit with the API. */
#include "dmod.h"
#include "dmrng_internal.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <errno.h>
#include <string.h>

static const char* config_section(dmini_context_t ini)
{
    if (dmini_has_key(ini, "rng", "instance")) return "rng";
    if (dmini_has_key(ini, "dmrng", "instance")) return "dmrng";
    return dmini_section_count(ini) ? dmini_section_name(ini, 0) : "rng";
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, dmdrvi_context_t, _create,
    (dmini_context_t ini, dmdrvi_dev_num_t* number))
{
    if (number == NULL) return NULL;
    dmrng_config_t cfg = {0, DMRNG_DEFAULT_TIMEOUT_MS};
    if (ini != NULL) {
        const char* section = config_section(ini);
        int instance = dmini_get_int(ini, section, "instance", 0);
        int timeout = dmini_get_int(ini, section, "timeout_ms", DMRNG_DEFAULT_TIMEOUT_MS);
        if (instance != 0 || timeout <= 0 || timeout > (int)DMRNG_MAX_TIMEOUT_MS) return NULL;
        cfg.instance = (uint32_t)instance;
        cfg.timeout_ms = (uint32_t)timeout;
    }
    dmrng_t ctx = dmrng_create(&cfg);
    if (ctx == NULL) return NULL;
    memset(number, 0, sizeof(*number));
    number->flags = DMDRVI_NUM_MAJOR | DMDRVI_NUM_ALT_NAME;
    memcpy(number->alt_name, "rng", sizeof("rng"));
    return (dmdrvi_context_t)ctx;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, void, _free, (dmdrvi_context_t context))
{
    int rc = dmrng_destroy((dmrng_t)context);
    if (rc != 0) DMOD_LOG_ERROR("dmrng: cannot release context (%d)\n", rc);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, void*, _open,
    (dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t* number))
{
    dmrng_t ctx = (dmrng_t)context;
    if (!dmrng_is_valid(ctx) || number == NULL || number->major != 0 ||
        (flags & (DMDRVI_O_WRONLY | DMDRVI_O_RDWR)) != 0) return NULL;
    if (dmosi_mutex_lock(ctx->lock) != 0) return NULL;
    void* result = NULL;
    if (ctx->opens != UINT32_MAX) { ++ctx->opens; result = ctx; }
    dmosi_mutex_unlock(ctx->lock);
    return result;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, void, _close,
    (dmdrvi_context_t context, void* handle))
{
    dmrng_t ctx = (dmrng_t)context;
    if (!dmrng_is_valid(ctx) || handle != ctx) return;
    if (dmosi_mutex_lock(ctx->lock) != 0) return;
    if (ctx->opens != 0) --ctx->opens;
    dmosi_mutex_unlock(ctx->lock);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, dmdrvi_ssize_t, _read,
    (dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset))
{
    (void)offset;
    dmrng_t ctx = (dmrng_t)context;
    if (!dmrng_is_valid(ctx) || handle != ctx || ctx->opens == 0) return -EBADF;
    if ((uint64_t)size > INT64_MAX) return -EOVERFLOW;
    int rc = dmrng_read(ctx, buffer, size);
    return rc == 0 ? (dmdrvi_ssize_t)size : rc;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, dmdrvi_ssize_t, _write,
    (dmdrvi_context_t ctx, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset))
{
    (void)ctx; (void)handle; (void)buffer; (void)size; (void)offset;
    return -EROFS;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, int, _ioctl,
    (dmdrvi_context_t ctx, void* handle, int command, void* arg))
{
    (void)ctx; (void)handle; (void)command; (void)arg;
    return -ENOTTY;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, int, _flush,
    (dmdrvi_context_t ctx, void* handle))
{
    return dmrng_is_valid((dmrng_t)ctx) && handle == (void*)ctx ? 0 : -EBADF;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmrng, int, _stat,
    (dmdrvi_context_t ctx, const char* path, dmdrvi_stat_t* stat))
{
    (void)path;
    if (!dmrng_is_valid((dmrng_t)ctx) || stat == NULL) return -EINVAL;
    stat->size = 0;
    stat->mode = 0020000 | 0444;
    return 0;
}
