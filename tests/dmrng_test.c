#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmrng.h"
#include <errno.h>
#include <string.h>
#ifdef DMRNG_MOCK
#include "dmrng_mock.h"
#include "dmosi.h"
#endif
static dmrng_t owned;
void dmod_test_setup(void)
{
    /* Hardware has a board-configured device; host tests own their context. */
    owned = dmrng_create(NULL);
}
void dmod_test_teardown(void)
{
    if (owned != NULL) DMOD_TEST_EXPECT_EQ(dmrng_destroy(owned), 0);
    owned = NULL;
}
DMOD_TEST_STEP(zero_and_invalid_reads)
{
    DMOD_TEST_EXPECT_EQ(dmrng_fill(NULL, 0), 0);
    DMOD_TEST_EXPECT_EQ(dmrng_fill(NULL, 1), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmrng_destroy(NULL), 0);
    DMOD_TEST_EXPECT_FALSE(dmrng_is_valid(NULL));
}
DMOD_TEST_STEP(unaligned_and_guarded_buffer)
{
    uint8_t data[35]; memset(data, 0xa5, sizeof(data));
    DMOD_TEST_EXPECT_EQ(dmrng_fill(data + 1, 33), 0);
    DMOD_TEST_EXPECT_EQ(data[0], 0xa5);
    DMOD_TEST_EXPECT_EQ(data[34], 0xa5);
    bool changed = false;
    for (size_t i = 1; i < 34; ++i) changed |= data[i] != 0xa5;
    DMOD_TEST_EXPECT_TRUE(changed);
}
DMOD_TEST_STEP(independent_blocks)
{
    uint8_t first[64], second[64];
    DMOD_TEST_EXPECT_EQ(dmrng_fill(first, sizeof(first)), 0);
    DMOD_TEST_EXPECT_EQ(dmrng_fill(second, sizeof(second)), 0);
    DMOD_TEST_EXPECT_NE(memcmp(first, second, sizeof(first)), 0);
}
DMOD_TEST_STEP(single_owner_and_configuration_validation)
{
    dmrng_config_t bad = {1, 100};
    DMOD_TEST_EXPECT_NULL(dmrng_create(&bad));
    bad.instance = 0; bad.timeout_ms = 0;
    DMOD_TEST_EXPECT_NULL(dmrng_create(&bad));
    bad.timeout_ms = DMRNG_MAX_TIMEOUT_MS + 1;
    DMOD_TEST_EXPECT_NULL(dmrng_create(&bad));
    DMOD_TEST_EXPECT_NULL(dmrng_create(NULL));
}
DMOD_TEST_STEP(sustained_generation)
{
    uint8_t data[256]; uint32_t hash = 2166136261u;
    for (unsigned block = 0; block < 256; ++block) {
        int rc = dmrng_fill(data, sizeof(data));
        DMOD_TEST_EXPECT_EQ(rc, 0);
        if (rc != 0) break;
        for (size_t i = 0; i < sizeof(data); ++i) hash = (hash ^ data[i]) * 16777619u;
    }
    Dmod_Printf("RNG_SAMPLE bytes=65536 checksum=%08x\n", (unsigned)hash);
}
#ifndef DMRNG_MOCK
DMOD_TEST_STEP(vfs_device)
{
    uint8_t first[37], second[37];
    void* a = Dmod_FileOpen("/dev/rng", "rb");
    void* b = Dmod_FileOpen("/dev/rng", "rb");
    DMOD_TEST_EXPECT_NOT_NULL(a); DMOD_TEST_EXPECT_NOT_NULL(b);
    if (a != NULL && b != NULL) {
        DMOD_TEST_EXPECT_EQ(Dmod_FileRead(first, 1, sizeof(first), a), sizeof(first));
        DMOD_TEST_EXPECT_EQ(Dmod_FileRead(second, 1, sizeof(second), b), sizeof(second));
        DMOD_TEST_EXPECT_NE(memcmp(first, second, sizeof(first)), 0);
    }
    if (a != NULL) Dmod_FileClose(a);
    if (b != NULL) Dmod_FileClose(b);
    void* writer = Dmod_FileOpen("/dev/rng", "wb");
    DMOD_TEST_EXPECT_NULL(writer);
    if (writer != NULL) Dmod_FileClose(writer);
}
#else
static void reader_thread(void* arg)
{
    int* result = arg;
    uint8_t bytes[31];
    *result = 0;
    for (unsigned i = 0; i < 100 && *result == 0; ++i)
        *result = dmrng_fill(bytes, sizeof(bytes));
}
DMOD_TEST_STEP(concurrent_readers)
{
    int a = -1, b = -1;
    dmosi_thread_t first = dmosi_thread_create(reader_thread, &a, 1, 16384, "rng-a", NULL);
    dmosi_thread_t second = dmosi_thread_create(reader_thread, &b, 1, 16384, "rng-b", NULL);
    DMOD_TEST_EXPECT_NOT_NULL(first); DMOD_TEST_EXPECT_NOT_NULL(second);
    if (first != NULL) { DMOD_TEST_EXPECT_EQ(dmosi_thread_join(first), 0); dmosi_thread_destroy(first); }
    if (second != NULL) { DMOD_TEST_EXPECT_EQ(dmosi_thread_join(second), 0); dmosi_thread_destroy(second); }
    DMOD_TEST_EXPECT_EQ(a, 0); DMOD_TEST_EXPECT_EQ(b, 0);
}
DMOD_TEST_STEP(fail_closed_and_clear_partial_output)
{
    uint8_t data[17]; memset(data, 0xa5, sizeof(data));
    dmrng_port_mock_fail_after(2, -EIO);
    DMOD_TEST_EXPECT_EQ(dmrng_fill(data, sizeof(data)), -EIO);
    for (size_t i = 0; i < sizeof(data); ++i) DMOD_TEST_EXPECT_EQ(data[i], 0);
    DMOD_TEST_EXPECT_EQ(dmrng_fill(data, sizeof(data)), -EIO);
}
DMOD_TEST_STEP(timeout_is_propagated)
{
    uint8_t data[8]; memset(data, 0xa5, sizeof(data));
    dmrng_port_mock_fail_after(0, -ETIMEDOUT);
    DMOD_TEST_EXPECT_EQ(dmrng_fill(data, sizeof(data)), -ETIMEDOUT);
    for (size_t i = 0; i < sizeof(data); ++i) DMOD_TEST_EXPECT_EQ(data[i], 0);
}
DMOD_TEST_STEP(release_and_recreate)
{
    DMOD_TEST_EXPECT_EQ(dmrng_destroy(owned), 0); owned = NULL;
    uint8_t data = 0xa5;
    DMOD_TEST_EXPECT_EQ(dmrng_fill(&data, 1), -ENODEV);
    DMOD_TEST_EXPECT_EQ(data, 0);
    owned = dmrng_create(NULL);
    DMOD_TEST_EXPECT_NOT_NULL(owned);
    DMOD_TEST_EXPECT_EQ(dmrng_read(owned, &data, 1), 0);
}
#endif
