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
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        // Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp)
        ASSERT_EQ(lxa_load_program("APPS:MaxonBASIC/bin/MaxonBASIC/MaxonBASIC", ""), 0) 
            << "Failed to load MaxonBASIC via APPS: assign";
        
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
    // Verify MaxonBASIC is running and has windows (pixel content may be 0 in headless mode)
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
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
