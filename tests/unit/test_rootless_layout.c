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

void test_rootless_layout_maps_rootless_local_coords_to_screen_coords(void)
{
    int screen_x = 0;
    int screen_y = 0;

    rootless_layout_screen_coords(25, 0, 12, 7, &screen_x, &screen_y);

    TEST_ASSERT_EQUAL_INT(37, screen_x);
    TEST_ASSERT_EQUAL_INT(7, screen_y);
}

void test_rootless_layout_expands_host_height_to_include_screen_rows_above_window(void)
{
    TEST_ASSERT_EQUAL_INT(256, rootless_layout_host_height(11, 245));
}

void test_rootless_layout_keeps_host_height_when_window_starts_at_screen_top(void)
{
    TEST_ASSERT_EQUAL_INT(245, rootless_layout_host_height(0, 245));
}

void test_rootless_layout_maps_extra_top_strip_back_to_screen_origin(void)
{
    int screen_x = 0;
    int screen_y = 0;

    rootless_layout_screen_coords(20, 11, 15, 5, &screen_x, &screen_y);

    TEST_ASSERT_EQUAL_INT(35, screen_x);
    TEST_ASSERT_EQUAL_INT(5, screen_y);
}

void test_rootless_layout_reports_host_origin_at_screen_top_when_window_is_lower(void)
{
    TEST_ASSERT_EQUAL_INT(0, rootless_layout_host_origin_y(11));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rootless_layout_preserves_width_without_host_expansion);
    RUN_TEST(test_rootless_layout_expands_to_remaining_screen_width);
    RUN_TEST(test_rootless_layout_clamps_negative_left_edge_before_expanding);
    RUN_TEST(test_rootless_layout_keeps_existing_width_when_already_wide_enough);
    RUN_TEST(test_rootless_layout_ignores_offscreen_left_edges);
    RUN_TEST(test_rootless_layout_maps_rootless_local_coords_to_screen_coords);
    RUN_TEST(test_rootless_layout_expands_host_height_to_include_screen_rows_above_window);
    RUN_TEST(test_rootless_layout_keeps_host_height_when_window_starts_at_screen_top);
    RUN_TEST(test_rootless_layout_maps_extra_top_strip_back_to_screen_origin);
    RUN_TEST(test_rootless_layout_reports_host_origin_at_screen_top_when_window_is_lower);
    return UNITY_END();
}
