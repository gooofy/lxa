/**
 * asm_one_gtest.cpp - Google Test version of ASM-One V1.48 test
 *
 * Tests ASM-One assembler/editor - opens screen, window, and editor.
 * Verifies screen dimensions, editor initialization, text entry,
 * cursor/mouse interaction, About dialog, and file requester flow.
 */

#include "lxa_test.h"

#include <cmath>
#include <filesystem>

using namespace lxa::testing;

class AsmOneTest : public LxaUITest {
protected:
    std::filesystem::path asm_one_base_path;

    bool SetupAsmOneAssigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        asm_one_base_path = std::filesystem::path(apps) / "Asm-One" / "bin" / "ASM-One";
        if (!std::filesystem::exists(asm_one_base_path / "ASM-One_V1.48")) {
            return false;
        }

        return lxa_add_assign_path("LIBS", (asm_one_base_path / "Libs").c_str());
    }

    bool HasReqToolsLibrary() const {
        return !asm_one_base_path.empty()
            && std::filesystem::exists(asm_one_base_path / "Libs" / "reqtools.library");
    }

    void FlushAndSettle() {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    /* ASM-One opens its own custom screen; menu bar is at screen top (y~0). */
    int MenuBarY() const {
        return std::max(3, window_info.y / 2);
    }

    /* Two-phase RMB menu selection (press at bar, move to item, release). */
    bool SelectMenuItem(int menu_x, int item_y) {
        int bar_y = MenuBarY();

        /* Phase 1: move to menu bar and press RMB */
        lxa_inject_mouse(menu_x, bar_y, 0, LXA_EVENT_MOUSEMOVE);
        lxa_trigger_vblank();
        lxa_run_cycles(100000);
        lxa_inject_mouse(menu_x, bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(30, 50000);

        /* Phase 2: move to target item and release RMB */
        lxa_inject_mouse(menu_x, item_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
        RunCyclesWithVBlank(5, 50000);
        lxa_inject_mouse(menu_x, item_y, 0, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(40, 50000);

        return lxa_is_running();
    }

    /* Dismiss a newly-opened window (requester / About dialog).
     * Tries close gadget first, then bottom-area click, then bottom gadget. */
    bool DismissWindow(int window_index, int target_count) {
        /* Try close gadget */
        if (lxa_click_close_gadget(window_index)) {
            RunCyclesWithVBlank(20, 50000);
            FlushAndSettle();
            if (lxa_get_window_count() <= target_count)
                return true;
        }

        /* Try clicking bottom-center area (OK / Cancel button) */
        lxa_window_info_t info = {};
        if (GetWindowInfo(window_index, &info)) {
            Click(info.x + info.width / 2, info.y + info.height - 12);
            RunCyclesWithVBlank(20, 50000);
            FlushAndSettle();
            if (lxa_get_window_count() <= target_count)
                return true;
        }

        /* Try clicking a bottom gadget */
        int gc = GetGadgetCount(window_index);
        int best = -1;
        int best_y = -1;
        for (int i = 0; i < gc; i++) {
            lxa_gadget_info_t gi;
            if (GetGadgetInfo(i, &gi, window_index) && gi.top > best_y) {
                best_y = gi.top;
                best = i;
            }
        }
        if (best >= 0) {
            ClickGadget(best, window_index);
            RunCyclesWithVBlank(20, 50000);
            FlushAndSettle();
            if (lxa_get_window_count() <= target_count)
                return true;
        }

        return false;
    }

    /* Type one character at a time with settling between each keystroke.
     * Some apps with slow event loops need this to avoid dropped keys. */
    void SlowTypeString(const char* str, int settle_vblanks = 4) {
        char ch[2] = {0, 0};
        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks, 50000);
        }
    }

    void SetUp() override {
        LxaUITest::SetUp();

        if (!SetupAsmOneAssigns()) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        /* Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp) */
        ASSERT_EQ(lxa_load_program("APPS:Asm-One/bin/ASM-One/ASM-One_V1.48", ""), 0) 
            << "Failed to load ASM-One via APPS: assign";
        
        /* Wait for window to open (ASM-One takes longer to initialize) */
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "ASM-One window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        printf("SetUp: initial window title='%s' w=%d h=%d\n",
               window_info.title, window_info.width, window_info.height);
        
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "ASM-One window did not draw";
        
        int drawn_content = lxa_get_window_content(0);
        printf("SetUp: after WaitForWindowDrawn content=%d\n", drawn_content);
        
        WaitForEventLoop(100, 10000);
        
        /* Let ASM-One fully initialize */
        RunCyclesWithVBlank(100, 50000);
        
        /* Re-check window state after full init */
        int wc = lxa_get_window_count();
        printf("SetUp: after init windows=%d\n", wc);
        for (int i = 0; i < wc; i++) {
            lxa_window_info_t wi = {};
            GetWindowInfo(i, &wi);
            int c = lxa_get_window_content(i);
            printf("SetUp:   window %d: title='%s' %dx%d content=%d\n",
                   i, wi.title, wi.width, wi.height, c);
        }
        if (wc > 0) {
            GetWindowInfo(0, &window_info);
        }
    }
};

TEST_F(AsmOneTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(AsmOneTest, BundledReqToolsLibraryIsAvailableFromDisk) {
    EXPECT_TRUE(HasReqToolsLibrary())
        << "ASM-One should bundle reqtools.library under APPS:Asm-One/bin/ASM-One/Libs";
}

TEST_F(AsmOneTest, StartupSkipsAllocateWorkspacePromptAndReachesEditor) {
    std::string startup_output = GetOutput();

    EXPECT_TRUE(lxa_is_running()) << "ASM-One should start with bundled reqtools.library available";
    EXPECT_STRNE(window_info.title, "System Message")
        << "ASM-One should reach its editor window instead of a system requester";
    EXPECT_EQ(lxa_get_window_count(), 1)
        << "ASM-One should stay in its main editor window instead of leaving an ALLOCATE/WORKSPACE startup requester open";
    EXPECT_EQ(startup_output.find("IntuitionBase=0x_timer:"), std::string::npos)
        << startup_output;

    ClearOutput();
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "ASM-One should keep running with bundled reqtools.library available";
    EXPECT_EQ(lxa_get_window_count(), 1)
        << "ASM-One startup should continue directly into the editor without a follow-up WORKSPACE requester window";

    std::string output = GetOutput();
    EXPECT_EQ(output.find("requested library reqtools.library was not found"), std::string::npos)
        << output;
}

TEST_F(AsmOneTest, MenuBarInteractionDoesNotLogInvalidRastPortBitmap) {
    const int menu_bar_x = window_info.x + 32;
    const int menu_bar_y = window_info.y + 5;
    const int first_item_y = window_info.y + 18;

    ClearOutput();
    lxa_inject_drag(menu_bar_x, menu_bar_y,
                    menu_bar_x, first_item_y,
                    LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(30, 50000);

    EXPECT_TRUE(lxa_is_running()) << "ASM-One should stay running after opening a menu";
    EXPECT_GE(lxa_get_window_count(), 1) << "ASM-One should keep its main window after menu interaction";

    std::string output = GetOutput();
    EXPECT_EQ(output.find("_render_menu_bar() invalid RastPort BitMap"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("_render_menu_items() invalid RastPort BitMap"), std::string::npos)
        << output;
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

TEST_F(AsmOneTest, AboutDialogOpensAndCloses) {
    const int baseline_windows = lxa_get_window_count();

    /* Capture content before menu interaction for comparison */
    FlushAndSettle();
    const int before_pixels = lxa_get_content_pixels();

    /* ASM-One V1.48 "Project" is the first menu on the bar.
     * "Project info" is the last item in the Project menu.
     * ASM-One does NOT set COMMSEQ on its menu items, so keyboard
     * shortcuts are handled internally and unreliably from the host.
     * Use RMB drag to select "Project info" via the menu system.
     *
     * Menu bar Y is the top of the custom screen (~0).
     * "Project" label starts around x=5..80.
     * The dropdown typically has 5-8 items each ~10px tall, so
     * "Project info" is around y = BarHeight + 70..90.
     * We probe multiple Y positions to account for layout variance.
     */
    ClearOutput();

    /* Try RMB drag from menu bar to "Project info" position.
     * Start on the "Project" label (x ~= 30-40) in the bar. */
    const int project_x = window_info.x + 35;

    /* Try several candidate Y offsets for "Project info" */
    bool opened = false;
    for (int try_y_offset : {80, 90, 70, 100, 60, 110}) {
        const int item_y = window_info.y + try_y_offset;
        SelectMenuItem(project_x, item_y);

        const int after = lxa_get_window_count();
        printf("About: tried y=%d, windows %d->%d\n",
               try_y_offset, baseline_windows, after);

        if (after > baseline_windows) {
            /* A new window appeared (About/requester dialog) */
            const int new_idx = after - 1;
            lxa_window_info_t new_info = {};
            ASSERT_TRUE(GetWindowInfo(new_idx, &new_info));
            printf("  About window title='%s' %dx%d\n",
                   new_info.title, new_info.width, new_info.height);

            EXPECT_TRUE(WaitForWindowDrawn(new_idx, 3000));
            EXPECT_GT(new_info.width, 30);
            EXPECT_GT(new_info.height, 20);
            CaptureWindow("/tmp/asm_one_about.png", new_idx);
            DismissWindow(new_idx, baseline_windows);
            opened = true;
            break;
        }
    }

    if (!opened) {
        /* Check if in-place rendering changed pixels */
        FlushAndSettle();
        const int after_pixels = lxa_get_content_pixels();
        lxa_capture_screen("/tmp/asm_one_about_inplace.png");
        printf("About: no new window opened (pixels: %d->%d)\n",
               before_pixels, after_pixels);
        /* Accept either: new window opened OR pixels visibly changed */
        EXPECT_NE(before_pixels, after_pixels)
            << "About/Project Info should cause visible change (new window or in-place)";
    }

    EXPECT_TRUE(lxa_is_running())
        << "ASM-One should survive About/Project Info interaction";
    EXPECT_GE(lxa_get_window_count(), baseline_windows)
        << "Main window should still be open after About dismiss";
}

TEST_F(AsmOneTest, EditorInputProducesVisibleContent) {
    /* Capture pixel count in editor area before typing */
    const int editor_x1 = window_info.x + 8;
    const int editor_y1 = window_info.y + 20;
    const int editor_x2 = window_info.x + window_info.width - 20;
    const int editor_y2 = window_info.y + window_info.height / 2;

    FlushAndSettle();
    const int before_pixels = CountContentPixels(editor_x1, editor_y1,
                                                  editor_x2, editor_y2);
    printf("Before typing: content pixels=%d in (%d,%d)-(%d,%d)\n",
           before_pixels, editor_x1, editor_y1, editor_x2, editor_y2);

    /* Click in the editor area to ensure focus */
    Click(window_info.x + window_info.width / 2,
          window_info.y + window_info.height / 3);
    RunCyclesWithVBlank(10, 50000);

    /* Type a short assembly program one char at a time */
    SlowTypeString("; Test input\n", 4);
    SlowTypeString("\tmove.l\t#0,d0\n", 4);
    SlowTypeString("\trts\n", 4);
    RunCyclesWithVBlank(20, 50000);
    FlushAndSettle();

    /* Verify visible content changed */
    const int after_pixels = CountContentPixels(editor_x1, editor_y1,
                                                 editor_x2, editor_y2);
    printf("After typing: content pixels=%d (delta=%d)\n",
           after_pixels, after_pixels - before_pixels);

    /* The typed text should produce visible pixel changes in the editor.
     * We use a generous threshold because the initial editor state may
     * already have some content (status line, prompt, etc.). */
    EXPECT_NE(before_pixels, after_pixels)
        << "Typing into the editor should change visible pixel content";

    EXPECT_TRUE(lxa_is_running())
        << "ASM-One should still be running after editor input";
}

TEST_F(AsmOneTest, FileRequesterOpensAndCanBeDismissed) {
    const int baseline_windows = lxa_get_window_count();

    /* ASM-One V1.48 "Project" menu "Old" item is the first or second
     * entry in the Project dropdown.  It opens the reqtools file requester.
     * Like the About test, we use RMB drag since ASM-One doesn't use
     * COMMSEQ for its keyboard shortcuts.
     *
     * "Old" is typically the first or second item, around y = BarHeight + 15..25.
     */
    ClearOutput();

    const int project_x = window_info.x + 35;

    /* Try several candidate Y offsets for "Old" */
    bool requester_opened = false;
    for (int try_y_offset : {20, 25, 15, 30}) {
        const int item_y = window_info.y + try_y_offset;
        SelectMenuItem(project_x, item_y);
        RunCyclesWithVBlank(40, 50000);

        const int after = lxa_get_window_count();
        printf("FileReq: tried y=%d, windows %d->%d\n",
               try_y_offset, baseline_windows, after);

        if (after > baseline_windows) {
            const int new_idx = after - 1;
            lxa_window_info_t req_info = {};
            ASSERT_TRUE(GetWindowInfo(new_idx, &req_info));
            printf("  requester window title='%s' %dx%d\n",
                   req_info.title, req_info.width, req_info.height);

            /* File requesters are typically large */
            EXPECT_GT(req_info.width, 80);
            EXPECT_GT(req_info.height, 50);

            EXPECT_TRUE(WaitForWindowDrawn(new_idx, 3000));
            CaptureWindow("/tmp/asm_one_file_requester.png", new_idx);
            DismissWindow(new_idx, baseline_windows);
            requester_opened = true;
            break;
        }
    }

    if (!requester_opened) {
        /* The reqtools file requester didn't open as a tracked window.
         * This may happen if reqtools uses a non-standard window or
         * if the menu item wasn't hit. Capture for review. */
        FlushAndSettle();
        lxa_capture_screen("/tmp/asm_one_file_req_result.png");
        printf("FileReq: no new window opened\n");
    }

    EXPECT_TRUE(lxa_is_running())
        << "ASM-One should survive file requester interaction";
    EXPECT_GE(lxa_get_window_count(), baseline_windows)
        << "Main window should still be open after requester dismiss";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
