/**
 * api_gtest.cpp - Google Test version of liblxa API tests
 *
 * Tests Phase 57 API extensions:
 * - lxa_inject_rmb_click()
 * - lxa_inject_drag()
 * - lxa_get_screen_info()
 * - lxa_read_pixel()
 * - lxa_read_pixel_rgb()
 */

#include "lxa_test.h"

using namespace lxa::testing;

class LxaAPITest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load SimpleGad for testing
        ASSERT_EQ(lxa_load_program("SYS:SimpleGad", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let program initialize
        RunCyclesWithVBlank(40, 50000);
    }
};

TEST_F(LxaAPITest, WindowOpens) {
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should be open";
}

TEST_F(LxaAPITest, GetScreenInfo) {
    lxa_screen_info_t screen_info;
    bool got_info = lxa_get_screen_info(&screen_info);
    
    ASSERT_TRUE(got_info) << "lxa_get_screen_info() should succeed";
    EXPECT_GT(screen_info.width, 0) << "Screen width should be > 0";
    EXPECT_GT(screen_info.height, 0) << "Screen height should be > 0";
    EXPECT_GT(screen_info.depth, 0) << "Screen depth should be > 0";
    EXPECT_GT(screen_info.num_colors, 0) << "num_colors should be > 0";
    EXPECT_EQ(screen_info.num_colors, (1 << screen_info.depth)) 
        << "num_colors should equal 2^depth";
}

TEST_F(LxaAPITest, ReadPixel) {
    int pen;
    
    // Valid pixel read
    bool read_ok = lxa_read_pixel(10, 10, &pen);
    ASSERT_TRUE(read_ok) << "lxa_read_pixel() at (10,10) should succeed";
    EXPECT_GE(pen, 0) << "Pen value should be >= 0";
    EXPECT_LT(pen, 256) << "Pen value should be < 256";
    
    // Out of bounds - negative
    bool oob_read = lxa_read_pixel(-1, -1, &pen);
    EXPECT_FALSE(oob_read) << "lxa_read_pixel() at (-1,-1) should fail";
    
    // Out of bounds - huge
    oob_read = lxa_read_pixel(10000, 10000, &pen);
    EXPECT_FALSE(oob_read) << "lxa_read_pixel() at (10000,10000) should fail";
}

TEST_F(LxaAPITest, ReadPixelRGB) {
    uint8_t r, g, b;
    bool read_ok = lxa_read_pixel_rgb(10, 10, &r, &g, &b);
    
    ASSERT_TRUE(read_ok) << "lxa_read_pixel_rgb() at (10,10) should succeed";
    // Just verify we got values - can't predict exact colors
    EXPECT_TRUE(true) << "RGB values retrieved";
}

TEST_F(LxaAPITest, InjectRMBClick) {
    int x = window_info.x + window_info.width / 2;
    int y = window_info.y + window_info.height / 2;
    
    bool rmb_ok = lxa_inject_rmb_click(x, y);
    EXPECT_TRUE(rmb_ok) << "lxa_inject_rmb_click() should succeed";
    
    RunCyclesWithVBlank(10, 50000);
    EXPECT_TRUE(lxa_is_running()) << "Program should still be running";
}

TEST_F(LxaAPITest, InjectDrag) {
    int x1 = window_info.x + 50;
    int y1 = window_info.y + 50;
    int x2 = window_info.x + 100;
    int y2 = window_info.y + 100;
    
    bool drag_ok = lxa_inject_drag(x1, y1, x2, y2, LXA_MOUSE_LEFT, 5);
    EXPECT_TRUE(drag_ok) << "lxa_inject_drag() should succeed";
    
    RunCyclesWithVBlank(10, 50000);
    EXPECT_TRUE(lxa_is_running()) << "Program should still be running";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
