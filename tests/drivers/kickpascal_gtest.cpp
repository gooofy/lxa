/**
 * kickpascal_gtest.cpp - Google Test version of KickPascal 2 test
 *
 * Tests KickPascal IDE - editor with text entry and cursor keys.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class KickPascalTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* kickpascal_path = "/home/guenter/projects/amiga/lxa-apps/KickPascal/KickPascal2";
        
        ASSERT_EQ(lxa_load_program(kickpascal_path, ""), 0) 
            << "Failed to load KickPascal";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "KickPascal window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let KickPascal initialize
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(KickPascalTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(KickPascalTest, EditorVisible) {
    int content_pixels = CountContentPixels(0, 0, 100, 100, 0);
    EXPECT_GT(content_pixels, 0) << "Editor should be visible";
}

TEST_F(KickPascalTest, TextEntry) {
    // Type a simple Pascal comment
    TypeString("{ Test }");
    RunCyclesWithVBlank(20, 50000);
    EXPECT_TRUE(lxa_is_running());
}

TEST_F(KickPascalTest, CursorKeys) {
    // Test cursor key handling (RAWKEY 0x4C-0x4F)
    PressKey(0x4F, 0);  // Cursor right
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4E, 0);  // Cursor left
    RunCyclesWithVBlank(10, 50000);
    
    EXPECT_TRUE(lxa_is_running());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
