/**
 * intuition_gtest.cpp - Google Test suite for Intuition library functions
 */

#include "lxa_test.h"

using namespace lxa::testing;

class IntuitionTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
    }

    void RunIntuitionTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Intuition/" + std::string(name);
        int result = lxa_load_program(path.c_str(), "");
        ASSERT_EQ(result, 0) << "Failed to load program " << path;
        
        // Some tests are interactive and need clicks
        if (strcmp(name, "GadgetClick") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));
            
            // Wait for program to reach Test 3
            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "--- Test 3: Close gadget click")) break;
            }
            
            // Click the close gadget
            // Close gadget is at (0, 0) in window, approx (10, 5) center
            Click(window_info.x + 10, window_info.y + 5);
            RunCyclesWithVBlank(20);
        } else if (strcmp(name, "MenuSelect") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));
            
            // Wait for program to reach Test 3
            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "--- Test 3: Simulate menu selection")) break;
            }
            
            // Menu bar is at the top of the screen
            // File menu title is at LeftEdge=5, Width=50. BeatX=0.
            // On custom screen, BarHBorder is usually 0 if not specified.
            int barX = 5 + 25; 
            int barY = 5;
            
            // RMB Down
            lxa_inject_mouse(barX, barY, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20);
            
            // Move to Open item (menu 0, item 0)
            // BarHeight is usually 11. Item 1 is at TopEdge=0, Height=10.
            int itemX = 5 + 40; // BeatX+Width/2
            int itemY = 11 + 1 + 5; // BarHeight + 1 + TopEdge + Height/2
            lxa_inject_mouse(itemX, itemY, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(20);
            
            // RMB Up (Selects item)
            lxa_inject_mouse(itemX, itemY, 0, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20);
        } else if (strcmp(name, "Validation") == 0) {
            // Wait for window
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));
            
            // Check window size
            EXPECT_EQ(window_info.width, 300);
            EXPECT_EQ(window_info.height, 150);
            
            // Check screen info
            lxa_screen_info_t screen_info;
            ASSERT_TRUE(lxa_get_screen_info(&screen_info));
            EXPECT_EQ(screen_info.width, 640);
            EXPECT_EQ(screen_info.height, 200);
            EXPECT_EQ(screen_info.depth, 2);
            
            // Check for content (non-zero pixels)
            RunCyclesWithVBlank(10);
            EXPECT_GT(lxa_get_content_pixels(), 100);
        }
        
        lxa_run_until_exit(timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        bool passed = (output.find("PASS") != std::string::npos) || 
                      (output.find("SUCCESS") != std::string::npos) ||
                      (output.find("All tests passed!") != std::string::npos) ||
                      (output.find("All gadget class tests passed!") != std::string::npos) ||
                      (output.find("All Tests Completed") != std::string::npos) ||
                      (output.find("Done.") != std::string::npos);
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
    }
};

TEST_F(IntuitionTest, BOOPSI) { RunIntuitionTest("BOOPSI"); }
TEST_F(IntuitionTest, GadgetClick) { RunIntuitionTest("GadgetClick"); }
TEST_F(IntuitionTest, GadgetRefresh) { RunIntuitionTest("GadgetRefresh"); }
TEST_F(IntuitionTest, GadgetClass) { RunIntuitionTest("GadgetClass"); }
TEST_F(IntuitionTest, IDCMP) { RunIntuitionTest("IDCMP"); }
TEST_F(IntuitionTest, IDCMPInput) { RunIntuitionTest("IDCMPInput"); }
TEST_F(IntuitionTest, MenuSelect) { RunIntuitionTest("MenuSelect"); }
TEST_F(IntuitionTest, MenuStrip) { RunIntuitionTest("MenuStrip"); }
TEST_F(IntuitionTest, RequesterBasic) { RunIntuitionTest("RequesterBasic"); }
TEST_F(IntuitionTest, ScreenBasic) { RunIntuitionTest("ScreenBasic"); }
TEST_F(IntuitionTest, ScreenManipulation) { RunIntuitionTest("ScreenManipulation"); }
TEST_F(IntuitionTest, Validation) { RunIntuitionTest("Validation"); }
TEST_F(IntuitionTest, WindowBasic) { RunIntuitionTest("WindowBasic"); }
TEST_F(IntuitionTest, WindowManipulation) { RunIntuitionTest("WindowManipulation"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
