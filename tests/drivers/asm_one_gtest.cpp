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
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        // Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp)
        ASSERT_EQ(lxa_load_program("APPS:Asm-One/bin/ASM-One/ASM-One_V1.48", ""), 0) 
            << "Failed to load ASM-One via APPS: assign";
        
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
    // Verify ASM-One is running and has windows (pixel content may be 0 in headless mode)
    EXPECT_TRUE(lxa_is_running()) << "ASM-One should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
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
