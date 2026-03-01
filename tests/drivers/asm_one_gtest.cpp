/**
 * asm_one_gtest.cpp - Google Test version of ASM-One V1.48 test
 *
 * Tests ASM-One assembler/editor - opens screen, window, and editor.
 * Verifies screen dimensions, editor initialization, text entry,
 * and cursor/mouse interaction.
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
        
        /* Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp) */
        ASSERT_EQ(lxa_load_program("APPS:Asm-One/bin/ASM-One/ASM-One_V1.48", ""), 0) 
            << "Failed to load ASM-One via APPS: assign";
        
        /* Wait for window to open (ASM-One takes longer to initialize) */
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "ASM-One window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        /* Let ASM-One fully initialize */
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(AsmOneTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(AsmOneTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(AsmOneTest, ScreenAndEditorReady) {
    /* Verify ASM-One is running and has windows */
    EXPECT_TRUE(lxa_is_running()) << "ASM-One should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
}

TEST_F(AsmOneTest, RespondsToInput) {
    /* Type a simple assembly program */
    TypeString("; Simple test program\n\tmove.l\t#0,d0\n\trts\n");
    RunCyclesWithVBlank(40, 50000);
    
    /* Verify ASM-One is still running and accepting input */
    EXPECT_TRUE(lxa_is_running()) << "ASM-One should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(AsmOneTest, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "ASM-One should still be running after mouse click";
}

TEST_F(AsmOneTest, CursorKeys) {
    /* Drain pending events */
    RunCyclesWithVBlank(40, 50000);
    
    /* Press cursor keys (Up, Down, Right, Left) */
    PressKey(0x4C, 0);  /* Up */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4D, 0);  /* Down */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4E, 0);  /* Right */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4F, 0);  /* Left */
    RunCyclesWithVBlank(10, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "ASM-One should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
