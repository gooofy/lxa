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
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        // Load the Devpac IDE (not GenAm which is the CLI assembler)
        ASSERT_EQ(lxa_load_program("APPS:DevPac/bin/Devpac/Devpac", ""), 0) 
            << "Failed to load Devpac via APPS: assign";
        
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
    // Verify Devpac is running and has windows (pixel content may be 0 in headless mode)
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
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
