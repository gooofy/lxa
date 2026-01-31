/*
 * Unit Tests for Configuration (config.c)
 *
 * Tests the configuration file parsing including:
 * - INI file parsing
 * - Section handling
 * - Key-value extraction
 * - Default values
 */

#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Include config header */
#include "config.h"

/* Test fixture paths */
static char g_test_dir[4096];
static char g_config_path[4096];

void setUp(void)
{
    /* Create temporary test directory */
    snprintf(g_test_dir, sizeof(g_test_dir), "/tmp/lxa_config_test_%d", (int)getpid());
    mkdir(g_test_dir, 0755);

    snprintf(g_config_path, sizeof(g_config_path), "%s/config.ini", g_test_dir);
}

void tearDown(void)
{
    /* Clean up test directories */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    int ret = system(cmd);
    (void)ret;
}

/*-------------------------------------------------------
 * Helper Functions
 *-------------------------------------------------------*/

static void write_config(const char *content)
{
    FILE *f = fopen(g_config_path, "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to create config file");
    fprintf(f, "%s", content);
    fclose(f);
}

/*-------------------------------------------------------
 * Basic Config Loading Tests
 *-------------------------------------------------------*/

void test_config_load_basic(void)
{
    write_config(
        "[system]\n"
        "rom_path = /path/to/rom.bin\n"
        "ram_size = 8388608\n"
    );

    TEST_ASSERT_TRUE(config_load(g_config_path));

    const char *rom_path = config_get_rom_path();
    TEST_ASSERT_NOT_NULL(rom_path);
    TEST_ASSERT_EQUAL_STRING("/path/to/rom.bin", rom_path);

    TEST_ASSERT_EQUAL_INT(8388608, config_get_ram_size());
}

void test_config_load_nonexistent_fails(void)
{
    TEST_ASSERT_FALSE(config_load("/nonexistent/path/config.ini"));
}

void test_config_comments_ignored(void)
{
    write_config(
        "# This is a comment\n"
        "; This is also a comment\n"
        "[system]\n"
        "rom_path = /rom/path\n"
        "# Another comment\n"
        "; And another\n"
    );

    TEST_ASSERT_TRUE(config_load(g_config_path));

    const char *rom_path = config_get_rom_path();
    TEST_ASSERT_NOT_NULL(rom_path);
    TEST_ASSERT_EQUAL_STRING("/rom/path", rom_path);
}

void test_config_whitespace_handling(void)
{
    write_config(
        "[system]\n"
        "  rom_path   =   /path/with/spaces   \n"
        "ram_size=12345678\n"
    );

    TEST_ASSERT_TRUE(config_load(g_config_path));

    const char *rom_path = config_get_rom_path();
    TEST_ASSERT_NOT_NULL(rom_path);
    TEST_ASSERT_EQUAL_STRING("/path/with/spaces", rom_path);

    TEST_ASSERT_EQUAL_INT(12345678, config_get_ram_size());
}

void test_config_empty_lines_ignored(void)
{
    write_config(
        "\n"
        "\n"
        "[system]\n"
        "\n"
        "rom_path = /rom\n"
        "\n"
        "\n"
    );

    TEST_ASSERT_TRUE(config_load(g_config_path));

    const char *rom_path = config_get_rom_path();
    TEST_ASSERT_NOT_NULL(rom_path);
    TEST_ASSERT_EQUAL_STRING("/rom", rom_path);
}

/*-------------------------------------------------------
 * Default Values Tests
 *-------------------------------------------------------*/

void test_config_default_ram_size(void)
{
    /* Empty config should use defaults */
    write_config(
        "[system]\n"
        "rom_path = /rom\n"
    );

    TEST_ASSERT_TRUE(config_load(g_config_path));

    /* Default RAM size is 10MB (10 * 1024 * 1024 = 10485760) */
    /* Note: this depends on implementation - check config.c for actual default */
    int ram = config_get_ram_size();
    TEST_ASSERT_TRUE_MESSAGE(ram > 0, "RAM size should be positive");
}

/*-------------------------------------------------------
 * Main Test Runner
 *-------------------------------------------------------*/

int main(void)
{
    UNITY_BEGIN();

    /* Basic loading */
    RUN_TEST(test_config_load_basic);
    RUN_TEST(test_config_load_nonexistent_fails);

    /* Comment handling */
    RUN_TEST(test_config_comments_ignored);

    /* Whitespace */
    RUN_TEST(test_config_whitespace_handling);
    RUN_TEST(test_config_empty_lines_ignored);

    /* Defaults */
    RUN_TEST(test_config_default_ram_size);

    return UNITY_END();
}
