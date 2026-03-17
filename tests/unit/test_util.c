#include "unity.h"

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

extern FILE *g_logf;
extern bool g_debug;

static char g_log_path[256];

static void *write_char_log_line(void *arg)
{
    const char *text = (const char *)arg;

    while (*text)
    {
        lputc(LOG_INFO, *text++);
    }

    return NULL;
}

static void assert_log_contains_exactly_once(const char *needle)
{
    FILE *file = fopen(g_log_path, "r");
    TEST_ASSERT_NOT_NULL(file);

    char buffer[4096];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[bytes_read] = '\0';

    const char *first = strstr(buffer, needle);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_TRUE(strstr(first + 1, needle) == NULL);
}

void setUp(void)
{
    snprintf(g_log_path, sizeof(g_log_path), "/tmp/lxa_test_util_%d.log", (int)getpid());
    unlink(g_log_path);

    g_logf = fopen(g_log_path, "w+");
    TEST_ASSERT_NOT_NULL(g_logf);
    g_debug = false;
}

void tearDown(void)
{
    if (g_logf != NULL)
    {
        fclose(g_logf);
        g_logf = NULL;
    }

    unlink(g_log_path);
    g_debug = false;
}

void test_lputc_serializes_whole_lines_across_threads(void)
{
    pthread_t thread_a;
    pthread_t thread_b;
    static const char line_a[] = "AAAAAA\n";
    static const char line_b[] = "BBBBBB\n";

    TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread_a, NULL, write_char_log_line, (void *)line_a));
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread_b, NULL, write_char_log_line, (void *)line_b));

    TEST_ASSERT_EQUAL_INT(0, pthread_join(thread_a, NULL));
    TEST_ASSERT_EQUAL_INT(0, pthread_join(thread_b, NULL));

    fflush(g_logf);

    assert_log_contains_exactly_once(line_a);
    assert_log_contains_exactly_once(line_b);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lputc_serializes_whole_lines_across_threads);
    return UNITY_END();
}
