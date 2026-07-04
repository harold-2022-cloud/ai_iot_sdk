#include "bsp_system.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TEST_BSP_MEM_HDR = 8,
    TEST_BSP_MEM_SIZE_OFF = 0,
    TEST_BSP_MEM_MAGIC_OFF = 4,
};

#define TEST_BSP_MEM_MAGIC_ALIVE 0x6D656D31u
#define TEST_BSP_MEM_MAGIC_FREED 0x6D656D30u

typedef struct {
    void *ptr;
    size_t size;
    unsigned free_count;
} allocation_t;

static allocation_t g_allocs[128];
static size_t g_alloc_count;
static unsigned g_unknown_free_count;
static char g_log[4096];
static size_t g_log_len;
static int g_failures;

static void fail_at(const char *file, int line, const char *expr)
{
    fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, expr);
    g_failures++;
}

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) fail_at(__FILE__, __LINE__, #expr); } while (0)
#define ASSERT_EQ_U32(expected, actual) \
    do { \
        uint32_t e__ = (uint32_t)(expected); \
        uint32_t a__ = (uint32_t)(actual); \
        if (e__ != a__) { \
            fprintf(stderr, "%s:%d: assertion failed: expected 0x%08x got 0x%08x\n", \
                    __FILE__, __LINE__, e__, a__); \
            g_failures++; \
        } \
    } while (0)
#define ASSERT_EQ_SIZE(expected, actual) \
    do { \
        size_t e__ = (size_t)(expected); \
        size_t a__ = (size_t)(actual); \
        if (e__ != a__) { \
            fprintf(stderr, "%s:%d: assertion failed: expected %zu got %zu\n", \
                    __FILE__, __LINE__, e__, a__); \
            g_failures++; \
        } \
    } while (0)

static void reset_logs(void)
{
    g_log_len = 0;
    g_log[0] = '\0';
}

static void reset_allocations(void)
{
    for (size_t i = 0; i < g_alloc_count; ++i) {
        free(g_allocs[i].ptr);
    }
    memset(g_allocs, 0, sizeof(g_allocs));
    g_alloc_count = 0;
    g_unknown_free_count = 0;
    reset_logs();
}

static allocation_t *find_allocation(void *ptr)
{
    for (size_t i = 0; i < g_alloc_count; ++i) {
        if (g_allocs[i].ptr == ptr) {
            return &g_allocs[i];
        }
    }
    return NULL;
}

static unsigned total_known_frees(void)
{
    unsigned total = 0;
    for (size_t i = 0; i < g_alloc_count; ++i) {
        total += g_allocs[i].free_count;
    }
    return total;
}

void *os_malloc(size_t size)
{
    ASSERT_TRUE(g_alloc_count < sizeof(g_allocs) / sizeof(g_allocs[0]));
    void *ptr = malloc(size == 0 ? 1 : size);
    ASSERT_TRUE(ptr != NULL);
    if (ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0xA5, size == 0 ? 1 : size);
    g_allocs[g_alloc_count++] = (allocation_t){
        .ptr = ptr,
        .size = size,
        .free_count = 0,
    };
    return ptr;
}

void os_free(void *ptr)
{
    allocation_t *alloc = find_allocation(ptr);
    if (alloc == NULL) {
        g_unknown_free_count++;
        return;
    }
    alloc->free_count++;
}

void *os_realloc(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    ASSERT_TRUE(!"Bsp_Mem_Realloc must not delegate to os_realloc");
    return NULL;
}

int os_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(g_log + g_log_len, sizeof(g_log) - g_log_len, fmt, ap);
    va_end(ap);
    if (ret > 0) {
        size_t used = (size_t)ret;
        if (used >= sizeof(g_log) - g_log_len) {
            g_log_len = sizeof(g_log) - 1;
        } else {
            g_log_len += used;
        }
    }
    return ret;
}

static uint32_t read_header_u32(const void *user_ptr, size_t offset)
{
    uint32_t value;
    memcpy(&value, (const uint8_t *)user_ptr - TEST_BSP_MEM_HDR + offset, sizeof(value));
    return value;
}

static void write_header_u32(void *user_ptr, size_t offset, uint32_t value)
{
    memcpy((uint8_t *)user_ptr - TEST_BSP_MEM_HDR + offset, &value, sizeof(value));
}

static allocation_t *allocation_for_user_ptr(void *user_ptr)
{
    return find_allocation((uint8_t *)user_ptr - TEST_BSP_MEM_HDR);
}

static void test_valid_alloc_free_sets_header_and_freed_magic(void)
{
    reset_allocations();

    void *ptr = Bsp_Mem_Malloc(16);
    ASSERT_TRUE(ptr != NULL);
    ASSERT_EQ_U32(16, read_header_u32(ptr, TEST_BSP_MEM_SIZE_OFF));
    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_ALIVE, read_header_u32(ptr, TEST_BSP_MEM_MAGIC_OFF));

    memset(ptr, 0x11, 16);
    Bsp_Mem_Free(ptr);

    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_FREED, read_header_u32(ptr, TEST_BSP_MEM_MAGIC_OFF));
    ASSERT_EQ_SIZE(1, total_known_frees());
    ASSERT_EQ_SIZE(0, g_unknown_free_count);
    ASSERT_TRUE(g_log[0] == '\0');
}

static void test_valid_realloc_preserves_data_and_releases_old_block(void)
{
    reset_allocations();

    uint8_t *ptr = (uint8_t *)Bsp_Mem_Malloc(8);
    ASSERT_TRUE(ptr != NULL);
    for (uint8_t i = 0; i < 8; ++i) {
        ptr[i] = (uint8_t)(0xC0u + i);
    }
    allocation_t *old_alloc = allocation_for_user_ptr(ptr);
    ASSERT_TRUE(old_alloc != NULL);

    uint8_t *grown = (uint8_t *)Bsp_Mem_Realloc(ptr, 24);
    ASSERT_TRUE(grown != NULL);
    ASSERT_TRUE(grown != ptr);
    for (uint8_t i = 0; i < 8; ++i) {
        ASSERT_EQ_U32((uint32_t)(0xC0u + i), grown[i]);
    }
    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_FREED, read_header_u32(ptr, TEST_BSP_MEM_MAGIC_OFF));
    ASSERT_EQ_SIZE(1, old_alloc->free_count);
    ASSERT_EQ_U32(24, read_header_u32(grown, TEST_BSP_MEM_SIZE_OFF));
    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_ALIVE, read_header_u32(grown, TEST_BSP_MEM_MAGIC_OFF));

    Bsp_Mem_Free(grown);
    ASSERT_EQ_SIZE(2, total_known_frees());
    ASSERT_EQ_SIZE(0, g_unknown_free_count);
}

static void test_foreign_pointer_free_is_logged_and_ignored(void)
{
    reset_allocations();
    uint8_t foreign[32] = {0};
    void *foreign_user_ptr = foreign + TEST_BSP_MEM_HDR;
    write_header_u32(foreign_user_ptr, TEST_BSP_MEM_SIZE_OFF, 12);
    write_header_u32(foreign_user_ptr, TEST_BSP_MEM_MAGIC_OFF, 0xDEADBEEFu);

    Bsp_Mem_Free(foreign_user_ptr);

    ASSERT_EQ_SIZE(0, total_known_frees());
    ASSERT_EQ_SIZE(0, g_unknown_free_count);
    ASSERT_TRUE(strstr(g_log, "foreign/corrupt") != NULL);
    ASSERT_TRUE(strstr(g_log, "free") != NULL);
}

static void test_double_free_is_logged_and_does_not_free_twice(void)
{
    reset_allocations();

    void *ptr = Bsp_Mem_Malloc(10);
    ASSERT_TRUE(ptr != NULL);
    Bsp_Mem_Free(ptr);
    reset_logs();

    Bsp_Mem_Free(ptr);

    allocation_t *alloc = allocation_for_user_ptr(ptr);
    ASSERT_TRUE(alloc != NULL);
    ASSERT_EQ_SIZE(1, alloc->free_count);
    ASSERT_EQ_SIZE(0, g_unknown_free_count);
    ASSERT_TRUE(strstr(g_log, "double-free/UAF") != NULL);
}

static void test_corrupt_magic_rejects_free_until_header_is_restored(void)
{
    reset_allocations();

    void *ptr = Bsp_Mem_Malloc(14);
    ASSERT_TRUE(ptr != NULL);
    write_header_u32(ptr, TEST_BSP_MEM_MAGIC_OFF, 0x12345678u);

    Bsp_Mem_Free(ptr);

    ASSERT_EQ_SIZE(0, total_known_frees());
    ASSERT_EQ_SIZE(0, g_unknown_free_count);
    ASSERT_TRUE(strstr(g_log, "foreign/corrupt") != NULL);

    write_header_u32(ptr, TEST_BSP_MEM_MAGIC_OFF, TEST_BSP_MEM_MAGIC_ALIVE);
    reset_logs();
    Bsp_Mem_Free(ptr);
    ASSERT_EQ_SIZE(1, total_known_frees());
    ASSERT_TRUE(g_log[0] == '\0');
}

static void test_psram_functions_share_bsp_mem_ownership_set(void)
{
    reset_allocations();

    uint8_t *psram = (uint8_t *)Bsp_Psram_Malloc(6);
    ASSERT_TRUE(psram != NULL);
    ASSERT_EQ_U32(6, read_header_u32(psram, TEST_BSP_MEM_SIZE_OFF));
    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_ALIVE, read_header_u32(psram, TEST_BSP_MEM_MAGIC_OFF));
    Bsp_Mem_Free(psram);
    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_FREED, read_header_u32(psram, TEST_BSP_MEM_MAGIC_OFF));

    uint8_t *mem = (uint8_t *)Bsp_Mem_Malloc(5);
    ASSERT_TRUE(mem != NULL);
    Bsp_Psram_Free(mem);
    ASSERT_EQ_U32(TEST_BSP_MEM_MAGIC_FREED, read_header_u32(mem, TEST_BSP_MEM_MAGIC_OFF));

    uint8_t *zeroed = (uint8_t *)Bsp_Psram_Calloc(4, 3);
    ASSERT_TRUE(zeroed != NULL);
    for (size_t i = 0; i < 12; ++i) {
        ASSERT_EQ_U32(0, zeroed[i]);
    }
    for (size_t i = 0; i < 12; ++i) {
        zeroed[i] = (uint8_t)(i + 1);
    }

    uint8_t *grown = (uint8_t *)Bsp_Psram_Realloc(zeroed, 20);
    ASSERT_TRUE(grown != NULL);
    for (size_t i = 0; i < 12; ++i) {
        ASSERT_EQ_U32((uint32_t)(i + 1), grown[i]);
    }
    Bsp_Psram_Free(grown);

    ASSERT_EQ_SIZE(4, total_known_frees());
    ASSERT_EQ_SIZE(0, g_unknown_free_count);
}

static void run_test(const char *name, void (*fn)(void))
{
    int before = g_failures;
    fn();
    if (g_failures == before) {
        printf("PASS %s\n", name);
    } else {
        printf("FAIL %s\n", name);
    }
}

int main(void)
{
    run_test("valid alloc/free", test_valid_alloc_free_sets_header_and_freed_magic);
    run_test("valid realloc", test_valid_realloc_preserves_data_and_releases_old_block);
    run_test("foreign pointer free", test_foreign_pointer_free_is_logged_and_ignored);
    run_test("double-free", test_double_free_is_logged_and_does_not_free_twice);
    run_test("corrupt magic", test_corrupt_magic_rejects_free_until_header_is_restored);
    run_test("Bsp_Psram aliases", test_psram_functions_share_bsp_mem_ownership_set);
    reset_allocations();
    return g_failures == 0 ? 0 : 1;
}
