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
            
            // Wait for the Amiga program to finish drawing content.
            // The program prints "OK: Content rendered" after RectFill/Text.
            // We must wait for this before checking pixels, because OpenWindow()
            // may take variable amounts of CPU cycles (e.g. creating system gadgets).
            char buffer[16384];
            while (true) {
                lxa_run_cycles(500);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "OK: Content rendered")) break;
            }
            
            // Flush display to convert planar bitmap to chunky pixels WITHOUT
            // running more Amiga cycles (which would let the program close the
            // screen before we can check the pixels).
            lxa_flush_display();
            EXPECT_GT(lxa_get_content_pixels(), 0) << "Expected some rendered content";
        } else if (strcmp(name, "PointerInput") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));

            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "READY: queue test"))
                    break;
            }

            int move_x = window_info.x + 80;
            int move_y = window_info.y + 40;
            lxa_inject_mouse(move_x, move_y, 0, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(10, 50000);
        } else if (strcmp(name, "IDCMPDeltaMove") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));

            /* Wait for the m68k program to be ready for mouse injection */
            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "READY: inject mouse moves")) break;
            }

            /* First establish a baseline position so the delta calculation
             * has a valid "previous" reference */
            int baseX = window_info.x + 100;
            int baseY = window_info.y + 50;
            lxa_inject_mouse(baseX, baseY, 0, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(10, 50000);

            /* Now move by a known delta (+10, +5) */
            lxa_inject_mouse(baseX + 10, baseY + 5, 0, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(20, 50000);

            /* Wait for "READY: inject absolute moves" for Test 2 */
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "READY: inject absolute moves")) break;
            }

            /* Inject another move for absolute mode */
            lxa_inject_mouse(baseX + 30, baseY + 20, 0, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(20, 50000);
        } else if (strcmp(name, "IDCMPSizeVerify") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));

            /* Wait for the m68k program to be ready */
            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "READY: click sizing gadget")) break;
            }

            /* Click+drag the sizing gadget (bottom-right corner).
             * SIZEBBOTTOM means the gadget is at the bottom edge. */
            int sizeX = window_info.x + window_info.width - 5;
            int sizeY = window_info.y + window_info.height - 5;

            /* LMB down on the size gadget triggers SIZEVERIFY */
            lxa_inject_mouse(sizeX, sizeY, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20, 50000);

            /* Drag slightly to trigger the resize */
            lxa_inject_mouse(sizeX + 10, sizeY + 10, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(10, 50000);

            /* Release */
            lxa_inject_mouse(sizeX + 10, sizeY + 10, 0, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20, 50000);

            /* Wait for Test 2 ready */
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "READY: click sizing gadget again")) break;
            }

            /* Click size gadget again — should NOT produce SIZEVERIFY */
            sizeX = window_info.x + window_info.width - 5;
            sizeY = window_info.y + window_info.height - 5;
            lxa_inject_mouse(sizeX, sizeY, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20, 50000);
            lxa_inject_mouse(sizeX + 5, sizeY + 5, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEMOVE);
            RunCyclesWithVBlank(10, 50000);
            lxa_inject_mouse(sizeX + 5, sizeY + 5, 0, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20, 50000);
        } else if (strcmp(name, "IDCMPMenuVerify") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            ASSERT_TRUE(GetWindowInfo(0, &window_info));

            /* Wait for the m68k program to be ready */
            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "READY: press RMB")) break;
            }

            /* RMB press at the menu bar area to trigger MENUVERIFY */
            int menuX = window_info.x + 30;
            int menuY = 5;  /* menu bar is at top of screen */
            lxa_inject_mouse(menuX, menuY, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20, 50000);

            /* Release RMB to dismiss menu */
            lxa_inject_mouse(menuX, menuY, 0, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(20, 50000);
        } else if (strcmp(name, "ScreenManipulation") == 0) {
            timeout_ms = 10000;
        } else if (strcmp(name, "WindowManipulation") == 0) {
            timeout_ms = 10000;
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

class DrawingHelpersDriverTest : public LxaUITest {
protected:
    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();
        ASSERT_EQ(lxa_load_program("SYS:Tests/Intuition/DrawingHelpers", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        /* Do NOT call WaitForEventLoop here — this is a linear program
         * that draws, pauses, and exits.  With LOG_INFO the program runs
         * so fast that a large cycle budget causes it to reach CloseScreen
         * before we can read pixels. */
    }
};

class RequesterBasicDriverTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        ASSERT_EQ(lxa_load_program("SYS:Tests/Intuition/RequesterBasic", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        WaitForEventLoop(100, 10000);
    }
};

TEST_F(IntuitionTest, BOOPSI) { RunIntuitionTest("BOOPSI"); }
TEST_F(IntuitionTest, BOOPSI_IC) { RunIntuitionTest("BOOPSI_IC"); }
TEST_F(IntuitionTest, DrawingHelpers) { RunIntuitionTest("DrawingHelpers"); }
TEST_F(IntuitionTest, GadgetClick) { RunIntuitionTest("GadgetClick"); }
TEST_F(IntuitionTest, GadgetRefresh) { RunIntuitionTest("GadgetRefresh"); }
TEST_F(IntuitionTest, GadgetClass) { RunIntuitionTest("GadgetClass"); }
TEST_F(IntuitionTest, GfxBaseFields) { RunIntuitionTest("GfxBaseFields"); }
TEST_F(IntuitionTest, IDCMP) { RunIntuitionTest("IDCMP"); }
TEST_F(IntuitionTest, IDCMPDeltaMove) { RunIntuitionTest("IDCMPDeltaMove"); }
TEST_F(IntuitionTest, IDCMPInput) { RunIntuitionTest("IDCMPInput"); }
TEST_F(IntuitionTest, IDCMPMenuVerify) { RunIntuitionTest("IDCMPMenuVerify"); }
TEST_F(IntuitionTest, IDCMPReqSet) { RunIntuitionTest("IDCMPReqSet"); }
TEST_F(IntuitionTest, IDCMPSizeVerify) { RunIntuitionTest("IDCMPSizeVerify"); }
TEST_F(IntuitionTest, MenuSelect) { RunIntuitionTest("MenuSelect"); }
TEST_F(IntuitionTest, MenuStrip) { RunIntuitionTest("MenuStrip"); }
TEST_F(IntuitionTest, PointerInput) { RunIntuitionTest("PointerInput"); }
TEST_F(IntuitionTest, RequesterBasic) { GTEST_SKIP() << "RequesterBasic has dedicated interactive coverage"; }
TEST_F(IntuitionTest, ScreenBasic) { RunIntuitionTest("ScreenBasic"); }
TEST_F(IntuitionTest, ScreenManipulation) { RunIntuitionTest("ScreenManipulation"); }
TEST_F(IntuitionTest, Validation) { RunIntuitionTest("Validation"); }
TEST_F(IntuitionTest, WindowBasic) { RunIntuitionTest("WindowBasic"); }
TEST_F(IntuitionTest, WindowManipulation) { RunIntuitionTest("WindowManipulation"); }

TEST_F(RequesterBasicDriverTest, SystemRequesterCancelAndConfirm) {
    ASSERT_TRUE(WaitForWindows(2, 10000));

    lxa_window_info_t parent_info;
    lxa_window_info_t req_info;
    ASSERT_TRUE(GetWindowInfo(0, &parent_info));
    ASSERT_TRUE(GetWindowInfo(1, &req_info));

    ClearOutput();
    Click(req_info.x + req_info.width - 8, req_info.y + 5);
    RunCyclesWithVBlank(40, 50000);

    std::string output;
    bool saw_cancel = false;
    for (int i = 0; i < 20; i++) {
        RunCyclesWithVBlank(5, 50000);
        output = GetOutput();
        if (output.find("OK: SysReqHandler returns cancel for IDCMP_CLOSEWINDOW") != std::string::npos) {
            saw_cancel = true;
            break;
        }
    }

    EXPECT_TRUE(saw_cancel)
        << output;

    ASSERT_TRUE(WaitForWindows(2, 10000));
    ASSERT_TRUE(GetWindowInfo(1, &req_info));

    ClearOutput();
    Click(req_info.x + 40, req_info.y + req_info.height - 18);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_wait_exit(10000));

    output = GetOutput();
    EXPECT_NE(output.find("OK: SysReqHandler returns the gadget ID"), std::string::npos)
        << output;
    EXPECT_NE(output.find("OK: FreeSysRequest closed the system requester"), std::string::npos)
        << output;
    EXPECT_NE(output.find("PASS: requester_basic all tests completed"), std::string::npos)
        << output;
}

TEST_F(DrawingHelpersDriverTest, RendersAndErasesContent) {
    std::string output;
    bool saw_before_erase = false;

    for (int i = 0; i < 40; i++) {
        RunCyclesWithVBlank(2, 50000);
        output = GetOutput();
        if (output.find("READY: before erase") != std::string::npos) {
            saw_before_erase = true;
            break;
        }
    }

    ASSERT_TRUE(saw_before_erase) << output;

    lxa_flush_display();

    EXPECT_GT(CountContentPixels(
                  window_info.x + 10,
                  window_info.y + 10,
                  window_info.x + 34,
                  window_info.y + 22,
                  0),
              20)
        << "Expected DrawBorder output to be visible";

    EXPECT_GT(CountContentPixels(
                  window_info.x + 10,
                  window_info.y + 30,
                  window_info.x + 60,
                  window_info.y + 42,
                  0),
              10)
        << "Expected PrintIText output to be visible";

    EXPECT_GT(CountContentPixels(
                  window_info.x + 60,
                  window_info.y + 20,
                  window_info.x + 75,
                  window_info.y + 27,
                  0),
              18)
        << "Expected DrawImage chain output to be visible before EraseImage";

    EXPECT_EQ(ReadPixel(window_info.x + 90, window_info.y + 20), 2)
        << "Expected IDS_SELECTED to merge edge pixels with the existing border pens";
    EXPECT_EQ(ReadPixel(window_info.x + 91, window_info.y + 21), 3)
        << "Expected IDS_SELECTED to merge interior pixels with the existing background pen";

    bool saw_after_erase = false;
    for (int i = 0; i < 40; i++) {
        RunCyclesWithVBlank(2, 50000);
        output = GetOutput();
        if (output.find("READY: after erase") != std::string::npos) {
            saw_after_erase = true;
            break;
        }
    }

    ASSERT_TRUE(saw_after_erase) << output;

    lxa_flush_display();

    EXPECT_EQ(CountContentPixels(
                  window_info.x + 60,
                  window_info.y + 20,
                  window_info.x + 67,
                  window_info.y + 27,
                  0),
              0)
        << "Expected EraseImage to clear the image area";

    EXPECT_GT(CountContentPixels(
                  window_info.x + 72,
                  window_info.y + 22,
                  window_info.x + 75,
                  window_info.y + 25,
                  0),
              0)
        << "Expected trailing chained image pixels to remain after EraseImage";

    EXPECT_TRUE(lxa_wait_exit(10000));

    output = GetOutput();
    EXPECT_NE(output.find("PASS: drawing_helpers all tests completed"), std::string::npos)
        << output;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
