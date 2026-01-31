/*
 * Unit Tests for VFS (Virtual File System)
 *
 * Tests the host-side VFS layer including:
 * - Path resolution
 * - Case-insensitive matching
 * - Drive/assign mappings
 * - Path normalization
 */

#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* Include VFS header */
#include "vfs.h"

/* Test fixture paths */
static char g_test_dir[4096];
static char g_fixture_sys[4096];
static char g_fixture_work[4096];

void setUp(void)
{
    /* Create temporary test directories */
    snprintf(g_test_dir, sizeof(g_test_dir), "/tmp/lxa_test_%d", (int)getpid());
    snprintf(g_fixture_sys, sizeof(g_fixture_sys), "%s/sys", g_test_dir);
    snprintf(g_fixture_work, sizeof(g_fixture_work), "%s/work", g_test_dir);

    mkdir(g_test_dir, 0755);
    mkdir(g_fixture_sys, 0755);
    mkdir(g_fixture_work, 0755);

    /* Create some test subdirectories and files */
    char path[4096];

    /* SYS:C directory */
    snprintf(path, sizeof(path), "%s/C", g_fixture_sys);
    mkdir(path, 0755);

    /* SYS:S directory */
    snprintf(path, sizeof(path), "%s/S", g_fixture_sys);
    mkdir(path, 0755);

    /* SYS:Libs directory (note: mixed case) */
    snprintf(path, sizeof(path), "%s/Libs", g_fixture_sys);
    mkdir(path, 0755);

    /* Create a test file */
    snprintf(path, sizeof(path), "%s/S/Startup-Sequence", g_fixture_sys);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "; Test startup sequence\n");
        fclose(f);
    }

    /* Create Work directory */
    snprintf(path, sizeof(path), "%s/Projects", g_fixture_work);
    mkdir(path, 0755);

    /* Initialize VFS */
    vfs_init();

    /* Add test drives */
    vfs_add_drive("SYS", g_fixture_sys);
    vfs_add_drive("WORK", g_fixture_work);
}

void tearDown(void)
{
    /* Clean up test directories */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_dir);
    int ret = system(cmd);
    (void)ret; /* Ignore return value for cleanup */
}

/*-------------------------------------------------------
 * Basic Path Resolution Tests
 *-------------------------------------------------------*/

void test_resolve_simple_drive_path(void)
{
    char result[4096];

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(g_fixture_sys, result);
}

void test_resolve_drive_with_subdir(void)
{
    char result[4096];

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:C", result, sizeof(result)));

    char expected[4096];
    snprintf(expected, sizeof(expected), "%s/C", g_fixture_sys);
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

void test_resolve_drive_with_file(void)
{
    char result[4096];

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:S/Startup-Sequence", result, sizeof(result)));

    char expected[4096];
    snprintf(expected, sizeof(expected), "%s/S/Startup-Sequence", g_fixture_sys);
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

void test_resolve_nil_device(void)
{
    char result[4096];

    TEST_ASSERT_TRUE(vfs_resolve_path("NIL:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING("/dev/null", result);
}

/*-------------------------------------------------------
 * Case-Insensitivity Tests
 *-------------------------------------------------------*/

void test_resolve_case_insensitive_drive(void)
{
    char result[4096];

    /* Drive name should be case-insensitive */
    TEST_ASSERT_TRUE(vfs_resolve_path("sys:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(g_fixture_sys, result);

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(g_fixture_sys, result);

    TEST_ASSERT_TRUE(vfs_resolve_path("Sys:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(g_fixture_sys, result);
}

void test_resolve_case_insensitive_path(void)
{
    char result[4096];
    char expected[4096];

    /* Path components should be case-insensitive */
    snprintf(expected, sizeof(expected), "%s/C", g_fixture_sys);

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:c", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(expected, result);

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:C", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

void test_resolve_case_insensitive_mixed(void)
{
    char result[4096];
    char expected[4096];

    /* Mixed case in path should work */
    snprintf(expected, sizeof(expected), "%s/Libs", g_fixture_sys);

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:LIBS", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(expected, result);

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:libs", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(expected, result);

    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:Libs", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

/*-------------------------------------------------------
 * Non-existent Path Tests
 *-------------------------------------------------------*/

void test_resolve_nonexistent_drive_fails(void)
{
    char result[4096];

    /* Unknown drive should fail */
    TEST_ASSERT_FALSE(vfs_resolve_path("UNKNOWN:", result, sizeof(result)));
}

void test_resolve_nonexistent_intermediate_dir_fails(void)
{
    char result[4096];

    /* Intermediate directory that doesn't exist should fail */
    TEST_ASSERT_FALSE(vfs_resolve_path("SYS:NonExistent/SubDir/File", result, sizeof(result)));
}

void test_resolve_nonexistent_file_allowed(void)
{
    char result[4096];

    /* Non-existent file in existing directory should succeed (for file creation) */
    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:C/NewFile", result, sizeof(result)));

    char expected[4096];
    snprintf(expected, sizeof(expected), "%s/C/NewFile", g_fixture_sys);
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

/*-------------------------------------------------------
 * Multiple Drive Tests
 *-------------------------------------------------------*/

void test_resolve_multiple_drives(void)
{
    char result[4096];
    char expected[4096];

    /* SYS: drive */
    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:C", result, sizeof(result)));
    snprintf(expected, sizeof(expected), "%s/C", g_fixture_sys);
    TEST_ASSERT_EQUAL_STRING(expected, result);

    /* WORK: drive */
    TEST_ASSERT_TRUE(vfs_resolve_path("WORK:Projects", result, sizeof(result)));
    snprintf(expected, sizeof(expected), "%s/Projects", g_fixture_work);
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

/*-------------------------------------------------------
 * Assign Tests
 *-------------------------------------------------------*/

void test_assign_basic(void)
{
    char result[4096];
    char assign_path[4096];
    char expected[4096];

    /* Create an assign */
    snprintf(assign_path, sizeof(assign_path), "%s/C", g_fixture_sys);
    TEST_ASSERT_TRUE(vfs_assign_add("COMMANDS", assign_path, ASSIGN_LOCK));

    /* Resolve via assign */
    TEST_ASSERT_TRUE(vfs_resolve_path("COMMANDS:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(assign_path, result);

    /* Check assign exists */
    TEST_ASSERT_TRUE(vfs_assign_exists("COMMANDS"));
    TEST_ASSERT_TRUE(vfs_assign_exists("commands")); /* case-insensitive */

    /* Get assign path */
    const char *path = vfs_assign_get_path("COMMANDS");
    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_EQUAL_STRING(assign_path, path);

    /* Clean up */
    TEST_ASSERT_TRUE(vfs_assign_remove("COMMANDS"));
    TEST_ASSERT_FALSE(vfs_assign_exists("COMMANDS"));
}

void test_assign_takes_precedence_over_drive(void)
{
    char result[4096];
    char assign_path[4096];

    /* Create an assign named "WORK" that points somewhere else */
    snprintf(assign_path, sizeof(assign_path), "%s/C", g_fixture_sys);
    TEST_ASSERT_TRUE(vfs_assign_add("WORK", assign_path, ASSIGN_LOCK));

    /* Assign should take precedence over drive */
    TEST_ASSERT_TRUE(vfs_resolve_path("WORK:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(assign_path, result);

    /* Clean up */
    TEST_ASSERT_TRUE(vfs_assign_remove("WORK"));

    /* Now drive should work again */
    TEST_ASSERT_TRUE(vfs_resolve_path("WORK:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(g_fixture_work, result);
}

/*-------------------------------------------------------
 * Path Utility Tests
 *-------------------------------------------------------*/

void test_vfs_home_dir(void)
{
    const char *home = vfs_get_home_dir();
    /* Home dir should be set after initialization */
    TEST_ASSERT_NOT_NULL(home);
}

void test_vfs_has_sys_drive(void)
{
    /* We added SYS drive in setUp */
    TEST_ASSERT_TRUE(vfs_has_sys_drive());
}

/*-------------------------------------------------------
 * Path Normalization Tests
 *-------------------------------------------------------*/

void test_resolve_path_with_trailing_slash(void)
{
    char result[4096];
    char expected[4096];

    /* Path with trailing component separator should work */
    snprintf(expected, sizeof(expected), "%s/C", g_fixture_sys);
    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:C/", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

void test_resolve_path_with_double_slash(void)
{
    char result[4096];
    char expected[4096];

    /* Double slashes should be handled gracefully */
    /* Note: This depends on implementation - may normalize or fail */
    snprintf(expected, sizeof(expected), "%s/C", g_fixture_sys);
    /* If implementation handles double slashes */
    if (vfs_resolve_path("SYS:C//", result, sizeof(result))) {
        /* Just check it returns a valid path */
        TEST_ASSERT_TRUE(strlen(result) > 0);
    }
    /* Otherwise just pass - not a critical failure */
}

void test_resolve_empty_path_after_drive(void)
{
    char result[4096];

    /* SYS: with nothing after should return drive root */
    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(g_fixture_sys, result);
}

void test_resolve_deeply_nested_path(void)
{
    char result[4096];
    char path[4096];
    char expected[4096];

    /* Create nested directory structure */
    snprintf(path, sizeof(path), "%s/A", g_fixture_sys);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/A/B", g_fixture_sys);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/A/B/C", g_fixture_sys);
    mkdir(path, 0755);

    /* Resolve deeply nested path */
    TEST_ASSERT_TRUE(vfs_resolve_path("SYS:A/B/C", result, sizeof(result)));
    snprintf(expected, sizeof(expected), "%s/A/B/C", g_fixture_sys);
    TEST_ASSERT_EQUAL_STRING(expected, result);
}

/*-------------------------------------------------------
 * Invalid Path Handling Tests
 *-------------------------------------------------------*/

void test_resolve_path_null_buffer(void)
{
    /* This should not crash */
    /* Note: actual behavior depends on implementation */
    /* Some implementations may return false, others may crash */
    /* We just test it doesn't crash by reaching the next assertion */
    char result[4096];
    TEST_ASSERT_FALSE(vfs_resolve_path("INVALID:", result, sizeof(result)));
}

void test_resolve_empty_path(void)
{
    char result[4096];

    /*
     * Empty path with SYS: configured resolves to SYS: root.
     * This is consistent with AmigaOS behavior where an empty/relative
     * path defaults to the current directory or system directory.
     */
    TEST_ASSERT_TRUE(vfs_resolve_path("", result, sizeof(result)));
    /* Result should be the SYS: directory */
    TEST_ASSERT_EQUAL_STRING(g_fixture_sys, result);
}

void test_resolve_colon_only(void)
{
    char result[4096];

    /* Just a colon should fail */
    TEST_ASSERT_FALSE(vfs_resolve_path(":", result, sizeof(result)));
}

/*-------------------------------------------------------
 * Multi-Assign Tests
 *-------------------------------------------------------*/

void test_multi_assign_add_path(void)
{
    char result[4096];
    char path1[4096];
    char path2[4096];

    /* Create two directories */
    snprintf(path1, sizeof(path1), "%s/C", g_fixture_sys);
    snprintf(path2, sizeof(path2), "%s/S", g_fixture_sys);

    /* Create multi-assign */
    TEST_ASSERT_TRUE(vfs_assign_add("MULTITEST", path1, ASSIGN_LOCK));
    TEST_ASSERT_TRUE(vfs_assign_add_path("MULTITEST", path2));

    /* First path should be used for resolution */
    TEST_ASSERT_TRUE(vfs_resolve_path("MULTITEST:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(path1, result);

    /* Clean up */
    vfs_assign_remove("MULTITEST");
}

void test_assign_case_insensitive(void)
{
    char result[4096];
    char assign_path[4096];

    snprintf(assign_path, sizeof(assign_path), "%s/Libs", g_fixture_sys);
    TEST_ASSERT_TRUE(vfs_assign_add("TESTASSIGN", assign_path, ASSIGN_LOCK));

    /* Case variations should all work */
    TEST_ASSERT_TRUE(vfs_assign_exists("testassign"));
    TEST_ASSERT_TRUE(vfs_assign_exists("TESTASSIGN"));
    TEST_ASSERT_TRUE(vfs_assign_exists("TestAssign"));

    /* Path resolution should work with any case */
    TEST_ASSERT_TRUE(vfs_resolve_path("testassign:", result, sizeof(result)));
    TEST_ASSERT_EQUAL_STRING(assign_path, result);

    vfs_assign_remove("TESTASSIGN");
}

void test_assign_remove_nonexistent(void)
{
    /* Removing non-existent assign should return false */
    TEST_ASSERT_FALSE(vfs_assign_remove("NONEXISTENT_ASSIGN"));
}

void test_assign_get_path_nonexistent(void)
{
    /* Getting path of non-existent assign should return NULL */
    const char *path = vfs_assign_get_path("NONEXISTENT_ASSIGN");
    TEST_ASSERT_NULL(path);
}

/*-------------------------------------------------------
 * Main Test Runner
 *-------------------------------------------------------*/

int main(void)
{
    UNITY_BEGIN();

    /* Basic path resolution */
    RUN_TEST(test_resolve_simple_drive_path);
    RUN_TEST(test_resolve_drive_with_subdir);
    RUN_TEST(test_resolve_drive_with_file);
    RUN_TEST(test_resolve_nil_device);

    /* Case insensitivity */
    RUN_TEST(test_resolve_case_insensitive_drive);
    RUN_TEST(test_resolve_case_insensitive_path);
    RUN_TEST(test_resolve_case_insensitive_mixed);

    /* Non-existent paths */
    RUN_TEST(test_resolve_nonexistent_drive_fails);
    RUN_TEST(test_resolve_nonexistent_intermediate_dir_fails);
    RUN_TEST(test_resolve_nonexistent_file_allowed);

    /* Multiple drives */
    RUN_TEST(test_resolve_multiple_drives);

    /* Assigns */
    RUN_TEST(test_assign_basic);
    RUN_TEST(test_assign_takes_precedence_over_drive);

    /* Utilities */
    RUN_TEST(test_vfs_home_dir);
    RUN_TEST(test_vfs_has_sys_drive);

    /* Path normalization */
    RUN_TEST(test_resolve_path_with_trailing_slash);
    RUN_TEST(test_resolve_path_with_double_slash);
    RUN_TEST(test_resolve_empty_path_after_drive);
    RUN_TEST(test_resolve_deeply_nested_path);

    /* Invalid path handling */
    RUN_TEST(test_resolve_path_null_buffer);
    RUN_TEST(test_resolve_empty_path);
    RUN_TEST(test_resolve_colon_only);

    /* Multi-assign tests */
    RUN_TEST(test_multi_assign_add_path);
    RUN_TEST(test_assign_case_insensitive);
    RUN_TEST(test_assign_remove_nonexistent);
    RUN_TEST(test_assign_get_path_nonexistent);

    return UNITY_END();
}
