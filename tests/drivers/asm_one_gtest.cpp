/**
 * asm_one_gtest.cpp - Google Test version of ASM-One V1.48 test
 *
 * Tests ASM-One assembler/editor - opens screen, window, and editor.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class AsmOneTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // ASM-One binary location
        const char* asm_one_path = "/home/guenter/projects/amiga/lxa-apps/ASM-One/V1.48/Asm-One";
        
        ASSERT_EQ(lxa_load_program(asm_one_path, ""), 0) 
            << "Failed to load ASM-One";
        
        // Wait for window to open (ASM-One takes longer to initialize)
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "ASM-One window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let ASM-One fully initialize
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(AsmOneTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(AsmOneTest, ScreenAndEditorReady) {
    // Check that we have content on screen (editor is visible)
    int content_pixels = CountContentPixels(0, 0, 100, 100, 0);
    EXPECT_GT(content_pixels, 0) << "Editor should have visible content";
}

TEST_F(AsmOneTest, RespondsToInput) {
    // Type a simple comment
    TypeString("; Test");
    RunCyclesWithVBlank(20, 50000);
    
    // Just verify the program is still running and responsive
    EXPECT_TRUE(lxa_is_running());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
