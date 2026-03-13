#include "unity.h"

#include "rootless_layout.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_rootless_layout_preserves_width_without_host_expansion(void)
{
    TEST_ASSERT_EQUAL_INT(400, rootless_layout_host_width(640, 0, 400, false));
}

void test_rootless_layout_expands_to_remaining_screen_width(void)
{
    TEST_ASSERT_EQUAL_INT(640, rootless_layout_host_width(640, 0, 400, true));
}

void test_rootless_layout_clamps_negative_left_edge_before_expanding(void)
{
    TEST_ASSERT_EQUAL_INT(640, rootless_layout_host_width(640, -20, 320, true));
}

void test_rootless_layout_keeps_existing_width_when_already_wide_enough(void)
{
    TEST_ASSERT_EQUAL_INT(700, rootless_layout_host_width(640, 10, 700, true));
}

void test_rootless_layout_ignores_offscreen_left_edges(void)
{
    TEST_ASSERT_EQUAL_INT(320, rootless_layout_host_width(640, 800, 320, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rootless_layout_preserves_width_without_host_expansion);
    RUN_TEST(test_rootless_layout_expands_to_remaining_screen_width);
    RUN_TEST(test_rootless_layout_clamps_negative_left_edge_before_expanding);
    RUN_TEST(test_rootless_layout_keeps_existing_width_when_already_wide_enough);
    RUN_TEST(test_rootless_layout_ignores_offscreen_left_edges);
    return UNITY_END();
}
