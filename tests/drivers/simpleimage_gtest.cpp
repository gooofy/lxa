/**
 * simpleimage_gtest.cpp - Google Test driver for SimpleImage sample
 *
 * Tests that the SimpleImage sample matches RKM behavior:
 * - Window opens at 200x100 with WA_RMBTrap only (no title, no gadgets)
 * - Exactly 2 images drawn at (10,10) and (100,10)
 * - Program waits (Delay) before exiting
 * - Image pixels are visible at expected locations
 */

#include "lxa_test.h"

using namespace lxa::testing;

/* ============================================================================
 * Behavioral tests (rootless mode, event-driven)
 * ============================================================================ */

class SimpleImageTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:SimpleImage", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));

        /* Let program initialize and draw images */
        RunCyclesWithVBlank(30);
    }
};

TEST_F(SimpleImageTest, WindowSize) {
    /* RKM original uses WA_Width=200, WA_Height=100 */
    EXPECT_EQ(window_info.width, 200)
        << "Window width should be 200 (matching RKM original)";
    EXPECT_EQ(window_info.height, 100)
        << "Window height should be 100 (matching RKM original)";
}

TEST_F(SimpleImageTest, WindowHasNoBorderTop) {
    /* Without WA_Title, WA_DragBar, WA_CloseGadget, or WA_DepthGadget,
     * the window should have a thin border (BorderTop == 2) */
    std::string output = GetOutput();

    EXPECT_NE(output.find("BorderTop=2"), std::string::npos)
        << "Window should have thin border (BorderTop=2, no title bar). Output: " << output;
}

TEST_F(SimpleImageTest, TwoImagesDrawn) {
    /* Wait for program to finish so all printf output is captured */
    EXPECT_TRUE(lxa_wait_exit(10000))
        << "Program should complete within 10 seconds";

    std::string output = GetOutput();

    EXPECT_NE(output.find("First DrawImage at (10,10) done"), std::string::npos)
        << "First DrawImage should be at (10,10). Output: " << output;
    EXPECT_NE(output.find("Second DrawImage at (100,10) done"), std::string::npos)
        << "Second DrawImage should be at (100,10). Output: " << output;

    /* Verify there is NO third DrawImage (the bug we fixed) */
    EXPECT_EQ(output.find("Third DrawImage"), std::string::npos)
        << "There should be no third DrawImage (RKM has only 2). Output: " << output;
}

TEST_F(SimpleImageTest, ProgramExitsCleanly) {
    /* The program has Delay(100) = 2 seconds, then exits */
    EXPECT_TRUE(lxa_wait_exit(10000))
        << "Program should complete within 10 seconds";

    std::string output = GetOutput();
    EXPECT_NE(output.find("Closing window"), std::string::npos)
        << "Program should close window before exit. Output: " << output;
    EXPECT_NE(output.find("Done"), std::string::npos)
        << "Program should print Done on exit. Output: " << output;
}

/* ============================================================================
 * Pixel verification tests (non-rootless mode)
 * ============================================================================ */

class SimpleImagePixelTest : public LxaUITest {
protected:
    void SetUp() override {
        /* Disable rootless so Intuition renders to screen bitmap */
        config.rootless = false;

        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:SimpleImage", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));

        /* Let rendering complete */
        WaitForEventLoop(200, 10000);
        RunCyclesWithVBlank(30, 200000);
    }
};

TEST_F(SimpleImagePixelTest, FirstImageVisible) {
    /* The first image is drawn at DrawImage(rp, &myImage, 10, 10).
     * Image is 24x10 pixels, a hollow rectangle (border pixels set).
     * With BorderLeft=4, BorderTop=2, the image appears at:
     *   window_x + 4 + 10 = window_x + 14
     *   window_y + 2 + 10 = window_y + 12
     */
    lxa_flush_display();

    int first_image_pixels = CountContentPixels(
        window_info.x + 14,       /* BorderLeft(4) + DrawImage x(10) */
        window_info.y + 12,       /* BorderTop(2) + DrawImage y(10) */
        window_info.x + 14 + 23,  /* 24px wide */
        window_info.y + 12 + 9,   /* 10px tall */
        0  /* background pen */
    );
    EXPECT_GT(first_image_pixels, 10)
        << "First image at (10,10) should have visible pixels";
}

TEST_F(SimpleImagePixelTest, SecondImageVisible) {
    /* The second image is drawn at DrawImage(rp, &myImage, 100, 10).
     * With BorderLeft=4, BorderTop=2, the image appears at:
     *   window_x + 4 + 100 = window_x + 104
     *   window_y + 2 + 10 = window_y + 12
     */
    lxa_flush_display();

    int second_image_pixels = CountContentPixels(
        window_info.x + 104,       /* BorderLeft(4) + DrawImage x(100) */
        window_info.y + 12,        /* BorderTop(2) + DrawImage y(10) */
        window_info.x + 104 + 23,  /* 24px wide */
        window_info.y + 12 + 9,    /* 10px tall */
        0  /* background pen */
    );
    EXPECT_GT(second_image_pixels, 10)
        << "Second image at (100,10) should have visible pixels";
}

TEST_F(SimpleImagePixelTest, NoThirdImage) {
    /* Verify there is NO image at the old (wrong) third position (50, 50).
     * With BorderLeft=4, BorderTop=2, that would be at window_x+54, window_y+52.
     * The area should be empty (all background). */
    lxa_flush_display();

    int third_area_pixels = CountContentPixels(
        window_info.x + 54,       /* BorderLeft(4) + old x(50) */
        window_info.y + 52,       /* BorderTop(2) + old y(50) */
        window_info.x + 54 + 23,  /* 24px wide */
        window_info.y + 52 + 9,   /* 10px tall */
        0  /* background pen */
    );
    EXPECT_EQ(third_area_pixels, 0)
        << "No image should be drawn at the old third position (50,50)";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
