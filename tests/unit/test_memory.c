/*
 * Unit Tests for Memory Access
 *
 * Tests the host-side memory access layer including:
 * - Read/write byte/word/long
 * - Memory region boundaries
 * - Big-endian byte ordering
 * - Alignment considerations
 *
 * Note: These tests use a mock memory implementation to test
 * the memory access logic without requiring the full emulator.
 */

#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Memory configuration - mirrors lxa.c */
#define RAM_START   0x000000
#define RAM_SIZE    (10 * 1024 * 1024)  /* 10 MB */
#define RAM_END     (RAM_START + RAM_SIZE - 1)

#define ROM_START   0xfc0000
#define ROM_SIZE    (256 * 1024)  /* 256 KB */
#define ROM_END     (ROM_START + ROM_SIZE - 1)

/* Test memory arrays */
static uint8_t *g_test_ram = NULL;
static uint8_t *g_test_rom = NULL;

/* Memory access functions - simplified versions for testing */
static uint8_t test_read8(uint32_t address)
{
    if (address >= RAM_START && address <= RAM_END) {
        return g_test_ram[address - RAM_START];
    }
    if (address >= ROM_START && address <= ROM_END) {
        return g_test_rom[address - ROM_START];
    }
    return 0xFF; /* Bus error would occur */
}

static uint16_t test_read16(uint32_t address)
{
    /* Big-endian: high byte first */
    return (test_read8(address) << 8) | test_read8(address + 1);
}

static uint32_t test_read32(uint32_t address)
{
    /* Big-endian: high byte first */
    return (test_read8(address) << 24) |
           (test_read8(address + 1) << 16) |
           (test_read8(address + 2) << 8) |
           test_read8(address + 3);
}

static void test_write8(uint32_t address, uint8_t value)
{
    if (address >= RAM_START && address <= RAM_END) {
        g_test_ram[address - RAM_START] = value;
    }
    /* ROM writes are silently ignored for testing */
}

static void test_write16(uint32_t address, uint16_t value)
{
    /* Big-endian: high byte first */
    test_write8(address, (value >> 8) & 0xFF);
    test_write8(address + 1, value & 0xFF);
}

static void test_write32(uint32_t address, uint32_t value)
{
    /* Big-endian: high byte first */
    test_write8(address, (value >> 24) & 0xFF);
    test_write8(address + 1, (value >> 16) & 0xFF);
    test_write8(address + 2, (value >> 8) & 0xFF);
    test_write8(address + 3, value & 0xFF);
}

void setUp(void)
{
    /* Allocate test memory */
    g_test_ram = calloc(1, RAM_SIZE);
    g_test_rom = calloc(1, ROM_SIZE);
    TEST_ASSERT_NOT_NULL(g_test_ram);
    TEST_ASSERT_NOT_NULL(g_test_rom);
}

void tearDown(void)
{
    free(g_test_ram);
    free(g_test_rom);
    g_test_ram = NULL;
    g_test_rom = NULL;
}

/*-------------------------------------------------------
 * Byte Read/Write Tests
 *-------------------------------------------------------*/

void test_write_read_byte_ram(void)
{
    uint32_t addr = 0x1000;

    test_write8(addr, 0x42);
    TEST_ASSERT_EQUAL_HEX8(0x42, test_read8(addr));

    test_write8(addr, 0xFF);
    TEST_ASSERT_EQUAL_HEX8(0xFF, test_read8(addr));

    test_write8(addr, 0x00);
    TEST_ASSERT_EQUAL_HEX8(0x00, test_read8(addr));
}

void test_write_read_byte_boundaries(void)
{
    /* Test at RAM start */
    test_write8(RAM_START, 0xAA);
    TEST_ASSERT_EQUAL_HEX8(0xAA, test_read8(RAM_START));

    /* Test at RAM end */
    test_write8(RAM_END, 0xBB);
    TEST_ASSERT_EQUAL_HEX8(0xBB, test_read8(RAM_END));
}

void test_read_byte_from_rom(void)
{
    /* Pre-populate ROM with test data */
    g_test_rom[0] = 0x11;
    g_test_rom[1] = 0x22;
    g_test_rom[ROM_SIZE - 1] = 0xFF;

    TEST_ASSERT_EQUAL_HEX8(0x11, test_read8(ROM_START));
    TEST_ASSERT_EQUAL_HEX8(0x22, test_read8(ROM_START + 1));
    TEST_ASSERT_EQUAL_HEX8(0xFF, test_read8(ROM_END));
}

/*-------------------------------------------------------
 * Word (16-bit) Read/Write Tests
 *-------------------------------------------------------*/

void test_write_read_word_ram(void)
{
    uint32_t addr = 0x2000;

    test_write16(addr, 0x1234);
    TEST_ASSERT_EQUAL_HEX16(0x1234, test_read16(addr));

    test_write16(addr, 0xFFFF);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, test_read16(addr));

    test_write16(addr, 0x0000);
    TEST_ASSERT_EQUAL_HEX16(0x0000, test_read16(addr));
}

void test_word_big_endian_order(void)
{
    uint32_t addr = 0x3000;

    /* Write a word */
    test_write16(addr, 0xABCD);

    /* Verify byte order (big-endian) */
    TEST_ASSERT_EQUAL_HEX8(0xAB, test_read8(addr));      /* High byte first */
    TEST_ASSERT_EQUAL_HEX8(0xCD, test_read8(addr + 1));  /* Low byte second */
}

void test_word_boundaries(void)
{
    /* Test at RAM start */
    test_write16(RAM_START, 0x1122);
    TEST_ASSERT_EQUAL_HEX16(0x1122, test_read16(RAM_START));

    /* Test at RAM end - 1 (last valid word address) */
    test_write16(RAM_END - 1, 0x3344);
    TEST_ASSERT_EQUAL_HEX16(0x3344, test_read16(RAM_END - 1));
}

/*-------------------------------------------------------
 * Long (32-bit) Read/Write Tests
 *-------------------------------------------------------*/

void test_write_read_long_ram(void)
{
    uint32_t addr = 0x4000;

    test_write32(addr, 0x12345678);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, test_read32(addr));

    test_write32(addr, 0xFFFFFFFF);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, test_read32(addr));

    test_write32(addr, 0x00000000);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, test_read32(addr));
}

void test_long_big_endian_order(void)
{
    uint32_t addr = 0x5000;

    /* Write a long */
    test_write32(addr, 0xDEADBEEF);

    /* Verify byte order (big-endian) */
    TEST_ASSERT_EQUAL_HEX8(0xDE, test_read8(addr));
    TEST_ASSERT_EQUAL_HEX8(0xAD, test_read8(addr + 1));
    TEST_ASSERT_EQUAL_HEX8(0xBE, test_read8(addr + 2));
    TEST_ASSERT_EQUAL_HEX8(0xEF, test_read8(addr + 3));
}

void test_long_boundaries(void)
{
    /* Test at RAM start */
    test_write32(RAM_START, 0xAABBCCDD);
    TEST_ASSERT_EQUAL_HEX32(0xAABBCCDD, test_read32(RAM_START));

    /* Test at RAM end - 3 (last valid long address) */
    test_write32(RAM_END - 3, 0x11223344);
    TEST_ASSERT_EQUAL_HEX32(0x11223344, test_read32(RAM_END - 3));
}

/*-------------------------------------------------------
 * Mixed Size Access Tests
 *-------------------------------------------------------*/

void test_mixed_access_sizes(void)
{
    uint32_t addr = 0x6000;

    /* Write a long */
    test_write32(addr, 0xCAFEBABE);

    /* Read as two words */
    TEST_ASSERT_EQUAL_HEX16(0xCAFE, test_read16(addr));
    TEST_ASSERT_EQUAL_HEX16(0xBABE, test_read16(addr + 2));

    /* Read as four bytes */
    TEST_ASSERT_EQUAL_HEX8(0xCA, test_read8(addr));
    TEST_ASSERT_EQUAL_HEX8(0xFE, test_read8(addr + 1));
    TEST_ASSERT_EQUAL_HEX8(0xBA, test_read8(addr + 2));
    TEST_ASSERT_EQUAL_HEX8(0xBE, test_read8(addr + 3));
}

void test_overlapping_writes(void)
{
    uint32_t addr = 0x7000;

    /* Write a long */
    test_write32(addr, 0x11223344);

    /* Overwrite middle word */
    test_write16(addr + 1, 0xFFFF);

    /* Result should be: 0x11 FF FF 44 */
    TEST_ASSERT_EQUAL_HEX8(0x11, test_read8(addr));
    TEST_ASSERT_EQUAL_HEX8(0xFF, test_read8(addr + 1));
    TEST_ASSERT_EQUAL_HEX8(0xFF, test_read8(addr + 2));
    TEST_ASSERT_EQUAL_HEX8(0x44, test_read8(addr + 3));
}

/*-------------------------------------------------------
 * Memory Region Tests
 *-------------------------------------------------------*/

void test_ram_rom_separation(void)
{
    /* Write to RAM */
    test_write32(0x1000, 0xAAAAAAAA);

    /* Pre-populate ROM */
    g_test_rom[0x1000] = 0xBB;
    g_test_rom[0x1001] = 0xBB;
    g_test_rom[0x1002] = 0xBB;
    g_test_rom[0x1003] = 0xBB;

    /* RAM read should return RAM data */
    TEST_ASSERT_EQUAL_HEX32(0xAAAAAAAA, test_read32(0x1000));

    /* ROM read should return ROM data */
    TEST_ASSERT_EQUAL_HEX32(0xBBBBBBBB, test_read32(ROM_START + 0x1000));
}

void test_consecutive_addresses(void)
{
    /* Write consecutive bytes */
    for (uint32_t i = 0; i < 256; i++) {
        test_write8(0x8000 + i, i & 0xFF);
    }

    /* Read them back */
    for (uint32_t i = 0; i < 256; i++) {
        TEST_ASSERT_EQUAL_HEX8(i & 0xFF, test_read8(0x8000 + i));
    }
}

void test_memory_clear_and_fill(void)
{
    uint32_t base = 0x9000;
    uint32_t size = 1024;

    /* Fill with pattern */
    for (uint32_t i = 0; i < size; i += 4) {
        test_write32(base + i, 0xDEADBEEF);
    }

    /* Verify pattern */
    for (uint32_t i = 0; i < size; i += 4) {
        TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, test_read32(base + i));
    }

    /* Clear memory */
    for (uint32_t i = 0; i < size; i += 4) {
        test_write32(base + i, 0x00000000);
    }

    /* Verify cleared */
    for (uint32_t i = 0; i < size; i += 4) {
        TEST_ASSERT_EQUAL_HEX32(0x00000000, test_read32(base + i));
    }
}

/*-------------------------------------------------------
 * SysBase (Address 4) Tests
 * In AmigaOS, address 4 contains the pointer to ExecBase
 *-------------------------------------------------------*/

void test_sysbase_access(void)
{
    /* Write a pointer value to address 4 */
    test_write32(4, 0x00000400);

    /* Read it back */
    TEST_ASSERT_EQUAL_HEX32(0x00000400, test_read32(4));
}

/*-------------------------------------------------------
 * Memory Configuration Verification
 *-------------------------------------------------------*/

void test_ram_size_constant(void)
{
    /* Verify RAM size is 10MB */
    TEST_ASSERT_EQUAL_INT(10 * 1024 * 1024, RAM_SIZE);
}

void test_rom_size_constant(void)
{
    /* Verify ROM size is 256KB */
    TEST_ASSERT_EQUAL_INT(256 * 1024, ROM_SIZE);
}

void test_rom_start_address(void)
{
    /* Verify ROM starts at 0xfc0000 (standard Amiga ROM address) */
    TEST_ASSERT_EQUAL_HEX32(0xfc0000, ROM_START);
}

/*-------------------------------------------------------
 * Main Test Runner
 *-------------------------------------------------------*/

int main(void)
{
    UNITY_BEGIN();

    /* Byte access */
    RUN_TEST(test_write_read_byte_ram);
    RUN_TEST(test_write_read_byte_boundaries);
    RUN_TEST(test_read_byte_from_rom);

    /* Word access */
    RUN_TEST(test_write_read_word_ram);
    RUN_TEST(test_word_big_endian_order);
    RUN_TEST(test_word_boundaries);

    /* Long access */
    RUN_TEST(test_write_read_long_ram);
    RUN_TEST(test_long_big_endian_order);
    RUN_TEST(test_long_boundaries);

    /* Mixed sizes */
    RUN_TEST(test_mixed_access_sizes);
    RUN_TEST(test_overlapping_writes);

    /* Memory regions */
    RUN_TEST(test_ram_rom_separation);
    RUN_TEST(test_consecutive_addresses);
    RUN_TEST(test_memory_clear_and_fill);

    /* SysBase */
    RUN_TEST(test_sysbase_access);

    /* Configuration */
    RUN_TEST(test_ram_size_constant);
    RUN_TEST(test_rom_size_constant);
    RUN_TEST(test_rom_start_address);

    return UNITY_END();
}
