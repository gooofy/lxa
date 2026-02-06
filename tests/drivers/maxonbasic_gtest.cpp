/**
 * maxonbasic_gtest.cpp - Google Test version of MaxonBASIC test
 *
 * Tests MaxonBASIC IDE.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class MaxonBasicTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* maxonbasic_path = "/home/guenter/projects/amiga/lxa-apps/MaxonBASIC/MaxonBASIC3";
        
        ASSERT_EQ(lxa_load_program(maxonbasic_path, ""), 0) 
            << "Failed to load MaxonBASIC";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "MaxonBASIC window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(MaxonBasicTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(MaxonBasicTest, EditorVisible) {
    int content_pixels = CountContentPixels(0, 0, 100, 100, 0);
    EXPECT_GT(content_pixels, 0);
}

TEST_F(MaxonBasicTest, TextEntry) {
    TypeString("10 PRINT ");
    RunCyclesWithVBlank(20, 50000);
    EXPECT_TRUE(lxa_is_running());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
