/**
 * devpac_gtest.cpp - Google Test version of Devpac (HiSoft) test
 *
 * Tests Devpac assembler - opens and displays editor.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class DevpacTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* devpac_path = "/home/guenter/projects/amiga/lxa-apps/Devpac/GenAm2";
        
        ASSERT_EQ(lxa_load_program(devpac_path, ""), 0) 
            << "Failed to load Devpac";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "Devpac window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let Devpac initialize
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(DevpacTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(DevpacTest, EditorVisible) {
    int content_pixels = CountContentPixels(0, 0, 100, 100, 0);
    EXPECT_GT(content_pixels, 0) << "Editor should be visible";
}

TEST_F(DevpacTest, RespondsToInput) {
    TypeString("; Test");
    RunCyclesWithVBlank(20, 50000);
    EXPECT_TRUE(lxa_is_running());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
