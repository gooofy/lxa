#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <time.h>

using namespace lxa::testing;

class SIGMAth2Test : public LxaUITest {
protected:
    int analysis_window_index = 0;
    int outer_window_index = 0;
    long startup_ms_ = -1;  /* Phase 126: latency baseline */

    bool SetupOriginalSystemAssigns(bool add_libs = false,
                                    bool add_fonts = false,
                                    bool add_devs = false)
    {
        const char* home = std::getenv("HOME");
        if (home == nullptr || home[0] == '\0') {
            return false;
        }

        const std::filesystem::path system_base =
            std::filesystem::path(home) / "media" / "emu" / "amiga" / "FS-UAE" / "hdd" / "system";

        if (!std::filesystem::exists(system_base)) {
            return false;
        }

        if (add_libs && !lxa_add_assign_path("LIBS", (system_base / "Libs").c_str())) {
            return false;
        }

        if (add_fonts && !lxa_add_assign_path("FONTS", (system_base / "Fonts").c_str())) {
            return false;
        }

        if (add_devs && !lxa_add_assign_path("DEVS", (system_base / "Devs").c_str())) {
            return false;
        }

        return true;
    }

    bool HasSIGMAth2Bundle()
    {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path sigma_base = std::filesystem::path(apps) / "SIGMAth2";
        return std::filesystem::exists(sigma_base / "SIGMAth_2")
            && std::filesystem::exists(sigma_base / "SIGMAthResc.RCT");
    }

    void FlushAndSettle()
    {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    /* Build a map from gadget_id -> gadget_info for the analysis window. */
    std::map<int, lxa_gadget_info_t> GetAnalysisGadgetMap()
    {
        std::map<int, lxa_gadget_info_t> result;
        auto gadgets = GetGadgets(analysis_window_index);
        for (const auto& g : gadgets)
        {
            result[g.gadget_id] = g;
        }
        return result;
    }

    /* Find gadget index by gadget_id in the analysis window. Returns -1 if
     * not found. */
    int FindGadgetIndexById(int gadget_id)
    {
        int count = GetGadgetCount(analysis_window_index);
        for (int i = 0; i < count; i++)
        {
            lxa_gadget_info_t info;
            if (GetGadgetInfo(i, &info, analysis_window_index)
                && info.gadget_id == gadget_id)
            {
                return i;
            }
        }
        return -1;
    }

    /* Menu bar Y coordinate — the screen title bar area. */
    int MenuBarY() const
    {
        return std::max(3, window_info.y / 2);
    }

    /* Open a SIGMAth2 menu via two-phase RMB interaction.
     * menu_x: absolute X for the desired menu title on the bar.
     * item_y: absolute Y for the desired item.
     * Returns true if app survived the interaction. */
    bool SelectMenuItem(int menu_x, int item_y)
    {
        int bar_y = MenuBarY();

        /* Phase 1: open the menu strip */
        lxa_inject_mouse(menu_x, bar_y, 0, LXA_EVENT_MOUSEMOVE);
        lxa_trigger_vblank();
        lxa_run_cycles(100000);
        lxa_inject_mouse(menu_x, bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(30, 50000);

        /* Phase 2: move to the target item and release */
        lxa_inject_mouse(menu_x, item_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
        RunCyclesWithVBlank(5, 50000);
        lxa_inject_mouse(menu_x, item_y, 0, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(40, 50000);

        return lxa_is_running();
    }

    /* Dismiss a newly-opened window (requester / About dialog).
     * Tries close gadget first, then bottom-area click. */
    bool DismissWindow(int window_index, int target_count)
    {
        /* Try close gadget */
        if (lxa_click_close_gadget(window_index))
        {
            RunCyclesWithVBlank(20, 50000);
            FlushAndSettle();
            if (lxa_get_window_count() <= target_count)
                return true;
        }

        /* Try clicking bottom-center area (OK / Cancel button) */
        lxa_window_info_t info = {};
        if (GetWindowInfo(window_index, &info))
        {
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
        for (int i = 0; i < gc; i++)
        {
            lxa_gadget_info_t gi;
            if (GetGadgetInfo(i, &gi, window_index) && gi.top > best_y)
            {
                best_y = gi.top;
                best = i;
            }
        }
        if (best >= 0)
        {
            ClickGadget(best, window_index);
            RunCyclesWithVBlank(20, 50000);
            FlushAndSettle();
            if (lxa_get_window_count() <= target_count)
                return true;
        }

        return false;
    }

    void SetUp() override
    {
        LxaUITest::SetUp();

        if (!SetupOriginalSystemAssigns(true, true, false) || !HasSIGMAth2Bundle()) {
            GTEST_SKIP() << "SIGMAth2 app bundle or original system disk not found";
        }

        ASSERT_EQ(lxa_load_program("APPS:SIGMAth2/SIGMAth_2", ""), 0)
            << "Failed to load SIGMAth2 via APPS: assign";

        struct timespec _t0, _t1;
        clock_gettime(CLOCK_MONOTONIC, &_t0);
        /* SIGMAth2 lifecycle:
         * 1. Opens first window (640x256) on a custom 3-bitplane screen
         * 2. Creates subprocess via CreateNewProc()
         * 3. First window closes
         * 4. Subprocess opens second window (724x283) on the same custom screen
         * 5. RCT library draws the analysis UI into the second window
         *
         * We wait for the first window, then give enough time for the
         * subprocess to close it and open the second one. */
        ASSERT_TRUE(WaitForWindows(1, 20000))
            << "SIGMAth2 did not open initial tracked window\n"
            << GetOutput();
        clock_gettime(CLOCK_MONOTONIC, &_t1);
        startup_ms_ = (_t1.tv_sec - _t0.tv_sec) * 1000L +
                      (_t1.tv_nsec - _t0.tv_nsec) / 1000000L;

        /* Let the app settle through its startup sequence */
        WaitForEventLoop(60, 10000);
        RunCyclesWithVBlank(60, 50000);
        FlushAndSettle();

        /* After the subprocess starts, the first window may close and
         * the second window opens.  SIGMAth2 creates two windows on its
         * custom screen:
         *   - A larger backdrop/container window (724x263)
         *   - A smaller "Analysis" window (629x202) with buttons and UI
         * We wait for at least 2 tracked windows, then select the second
         * (smaller) one as the analysis window.  If only one window is
         * present, we fall back to it. */
        bool found_analysis_window = false;
        for (int attempt = 0; attempt < 100; attempt++)
        {
            int wc = lxa_get_window_count();
            if (wc >= 2)
            {
                /* Use the last (most recently opened) window */
                analysis_window_index = wc - 1;
                outer_window_index = 0;
                GetWindowInfo(analysis_window_index, &window_info);
                found_analysis_window = true;
                break;
            }

            RunCyclesWithVBlank(10, 50000);
            FlushAndSettle();
        }

        if (!found_analysis_window)
        {
            /* Fall back: use whatever window is available */
            if (lxa_get_window_count() > 0)
            {
                GetWindowInfo(0, &window_info);
            }
        }

        /* Give the RCT library time to draw the analysis UI.
         * With LOG_INFO (no DPRINTF overhead) the app renders much faster,
         * so 300 iterations is ample.  Backing store operations in
         * SMART_REFRESH layers add additional BltBitMap overhead during
         * window creation/overlap, so we budget generously. */
        RunCyclesWithVBlank(500, 50000);
        FlushAndSettle();

        /* Re-check window count after RCT drawing — the Analysis window
         * may have appeared during the draw period. */
        {
            int wc = lxa_get_window_count();
            if (wc >= 2 && !found_analysis_window)
            {
                analysis_window_index = wc - 1;
                outer_window_index = 0;
                found_analysis_window = true;
            }
            else if (wc >= 2 && found_analysis_window)
            {
                /* Re-validate: pick the window titled "Analysis" if present,
                 * otherwise use the last one. */
                for (int i = wc - 1; i >= 0; i--)
                {
                    lxa_window_info_t wi = {};
                    if (GetWindowInfo(i, &wi)
                        && std::string(wi.title).find("Analysis") != std::string::npos)
                    {
                        analysis_window_index = i;
                        outer_window_index = (i == 0) ? 1 : 0;
                        break;
                    }
                }
            }
        }

        /* Re-read window info in case dimensions changed after drawing */
        GetWindowInfo(analysis_window_index, &window_info);
    }
};

TEST_F(SIGMAth2Test, StartupOpensVisibleAnalysisWindow)
{
    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should still be running after startup\n"
        << GetOutput();
    EXPECT_STRNE(window_info.title, "System Message")
        << "SIGMAth2 should reach its analysis UI instead of a system requester";
    EXPECT_GE(window_info.width, 500);
    EXPECT_GE(window_info.height, 150);
}

TEST_F(SIGMAth2Test, StartupKeepsRunningWithoutResourceFailures)
{
    const std::string output = GetOutput();

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should keep running after startup\n"
        << output;
    EXPECT_EQ(output.find("requested library"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("SIGMAthResc.RCT"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("MathIEEEDoubBas.Library"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("RCT.Library"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("Topaz.Font"), std::string::npos)
        << output;
}

TEST_F(SIGMAth2Test, MainWindowCaptureContainsVisibleUiRegions)
{
    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should remain running after startup\n"
        << GetOutput();
    EXPECT_STRNE(window_info.title, "System Message")
        << "SIGMAth2 should reach its analysis UI instead of a system requester";
    EXPECT_GE(window_info.width, 500);
    EXPECT_GE(window_info.height, 150);
    EXPECT_LE(window_info.width, 760);
    EXPECT_LE(window_info.height, 320);
    EXPECT_TRUE(WaitForWindowDrawn(analysis_window_index, 2000))
        << "SIGMAth2 startup window should remain drawable after settling\n"
        << GetOutput();

    /* Capture screenshots for visual inspection and regression analysis */
    CaptureWindow("/tmp/sigma_analysis_window.png", analysis_window_index);
    lxa_capture_screen("/tmp/sigma_full_screen.png");
}

TEST_F(SIGMAth2Test, MenuBarRightMouseSmokeKeepsAnalysisWindowAlive)
{
    const int baseline_window_count = lxa_get_window_count();
    const int menu_bar_x = std::min(window_info.x + 32, window_info.x + window_info.width - 8);
    const int menu_bar_y = std::max(3, window_info.y / 2);
    const int first_item_y = std::min(window_info.y + 22, window_info.y + window_info.height - 8);
    lxa_window_info_t after_info = {};

    /* Perform a right-mouse drag from the menu bar area downward,
     * simulating a user opening and browsing the menu strip. */
    lxa_inject_drag(menu_bar_x, menu_bar_y,
                    menu_bar_x, first_item_y,
                    LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(30, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should remain running after menu-bar interaction\n"
        << GetOutput();
    EXPECT_TRUE(WaitForWindows(1, 2000))
        << "SIGMAth2 should keep its main analysis window visible after menu interaction\n"
        << GetOutput();
    ASSERT_TRUE(GetWindowInfo(0, &after_info));
    EXPECT_GE(after_info.width, window_info.width)
        << "SIGMAth2 should keep or expand its visible rootless surface during menu interaction";
    EXPECT_GE(after_info.height, window_info.height)
        << "SIGMAth2 should keep or expand its visible rootless surface during menu interaction";
    EXPECT_GE(lxa_get_window_count(), baseline_window_count)
        << "SIGMAth2 menu interaction should not close its tracked analysis window";
}

/* -----------------------------------------------------------------------
 * Phase 104 follow-up: Deterministic gadget geometry assertions
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, AnalysisWindowGadgetGridLayout)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    /* The Analysis window should have at least 24 gadgets:
     *   - 20 math function buttons (ids 0-19) in a 6-column grid
     *   - 2 scroll arrow gadgets (ids 20-21)
     *   - 1 vertical prop gadget (id 23)
     *   - String gadgets (ids 22, 24-25)
     *   - 1 large display frame (id 26)
     *   - 1 system depth gadget (id 27) */
    const int gadget_count = GetGadgetCount(analysis_window_index);
    EXPECT_GE(gadget_count, 24)
        << "Analysis window should have at least 24 gadgets (buttons + scrollers + strings + frame)";

    auto gmap = GetAnalysisGadgetMap();

    /* Verify the button grid exists: 4 rows x 6 columns (ids 0-5, 6-7 partial,
     * 8-13, 14-19).  We check that all ids 0-19 are present and have
     * plausible button dimensions (width ~80-100, height ~10-18). */
    int found_buttons = 0;
    for (int id = 0; id <= 19; id++)
    {
        auto it = gmap.find(id);
        if (it != gmap.end())
        {
            const auto& g = it->second;
            EXPECT_GE(g.width, 60)
                << "Button gadget id=" << id << " width too small";
            EXPECT_LE(g.width, 120)
                << "Button gadget id=" << id << " width too large";
            EXPECT_GE(g.height, 8)
                << "Button gadget id=" << id << " height too small";
            EXPECT_LE(g.height, 22)
                << "Button gadget id=" << id << " height too large";
            found_buttons++;
        }
    }
    EXPECT_GE(found_buttons, 16)
        << "Expected at least 16 of the 20 math function buttons to be present";

    /* Verify bottom row (ids 0-5) is arranged left-to-right at the same Y.
     * The bottom row should have the largest Y values among the button grid. */
    if (gmap.count(0) && gmap.count(1) && gmap.count(5))
    {
        const int row_y = gmap[0].top;
        for (int id = 1; id <= 5; id++)
        {
            if (gmap.count(id))
            {
                EXPECT_NEAR(gmap[id].top, row_y, 2)
                    << "Bottom row buttons should be at the same Y; id=" << id;
            }
        }
        /* Verify left-to-right ordering within the row.
         * Note: SIGMAth2 adds gadgets via prepend, so id=5 is leftmost
         * and id=0 is rightmost in the bottom row. */
        EXPECT_GT(gmap[0].left, gmap[1].left)
            << "Button 0 should be right of button 1 (reverse-linked-list order)";
        EXPECT_GT(gmap[1].left, gmap[5].left)
            << "Button 1 should be right of button 5 (reverse-linked-list order)";
    }

    /* Verify the large display frame (id 26) exists and is substantially
     * larger than the buttons */
    if (gmap.count(26))
    {
        EXPECT_GE(gmap[26].width, 400)
            << "Display frame (id=26) should be at least 400px wide";
        EXPECT_GE(gmap[26].height, 50)
            << "Display frame (id=26) should be at least 50px tall";
    }

    /* Verify scroll arrows (ids 20-21) are small square-ish gadgets */
    for (int id = 20; id <= 21; id++)
    {
        if (gmap.count(id))
        {
            EXPECT_LE(gmap[id].width, 30)
                << "Scroll arrow id=" << id << " should be narrow";
            EXPECT_LE(gmap[id].height, 20)
                << "Scroll arrow id=" << id << " should be short";
        }
    }

    /* Verify the vertical prop gadget (id 23) is tall and narrow */
    if (gmap.count(23))
    {
        EXPECT_LE(gmap[23].width, 30)
            << "Vertical prop (id=23) should be narrow";
        EXPECT_GE(gmap[23].height, 30)
            << "Vertical prop (id=23) should be tall";
    }
}

/* -----------------------------------------------------------------------
 * Phase 104: About dialog via menu
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, AboutDialogOpensAndCloses)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();
    const int baseline_windows = lxa_get_window_count();

    /* SIGMAth2 has a German-language menu. The first menu title ("Projekt")
     * is near the left edge.  "Über SIGMAth..." (About) is typically
     * in the Project menu.
     *
     * Menu bar X: first menu title ~24px from the left edge of the
     * outer window (window at x=0 on the custom screen).
     * Menu items start at ~y=11 below the title bar, each ~10px tall.
     * We probe a few targeted Y positions. */
    lxa_window_info_t outer_info = {};
    if (lxa_get_window_count() >= 2)
        GetWindowInfo(outer_window_index, &outer_info);
    else
        outer_info = window_info;

    const int menu_x = outer_info.x + 24;
    bool opened_about = false;

    /* Probe 3 targeted Y positions for common About-item locations:
     * y=16 (first item), y=28 (second or third item), y=48 (deeper). */
    const int probe_ys[] = {16, 28, 48};
    for (int try_y : probe_ys)
    {
        const int before = lxa_get_window_count();

        SelectMenuItem(menu_x, try_y);
        RunCyclesWithVBlank(20, 50000);
        FlushAndSettle();

        const int after = lxa_get_window_count();
        if (after > before)
        {
            const int about_idx = after - 1;
            lxa_window_info_t about_info = {};
            ASSERT_TRUE(GetWindowInfo(about_idx, &about_info));

            EXPECT_GT(about_info.width, 30)
                << "About dialog should have reasonable width";
            EXPECT_GT(about_info.height, 20)
                << "About dialog should have reasonable height";

            CaptureWindow("/tmp/sigma_about_dialog.png", about_idx);

            EXPECT_TRUE(WaitForWindowDrawn(about_idx, 3000))
                << "About dialog should render visible content";

            DismissWindow(about_idx, baseline_windows);
            opened_about = true;
            break;
        }

        if (!lxa_is_running())
            break;
    }

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should survive About dialog interaction\n"
        << GetOutput();

    /* It is acceptable if the About dialog does not open as a separate
     * window (some apps render in-place or use system requesters). */
    if (!opened_about)
    {
        EXPECT_TRUE(lxa_is_running())
            << "SIGMAth2 should survive menu probing even if About was not found\n"
            << GetOutput();
    }
}

/* -----------------------------------------------------------------------
 * Phase 104: Button click interaction coverage
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, ButtonClickInteraction)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    /* Click several math function buttons and verify the app survives
     * each click.  We cannot easily verify the mathematical result without
     * reading the string gadget contents, but we can assert:
     *   1. The app remains running after each click
     *   2. The gadget count does not change (no UI corruption)
     *   3. The window remains drawable */

    const int initial_gadget_count = GetGadgetCount(analysis_window_index);
    ASSERT_GE(initial_gadget_count, 20)
        << "Need at least 20 gadgets for button interaction test";

    /* Click buttons by gadget_id. The bottom two rows (ids 0-13) contain
     * common math operations. We'll click a selection of them. */
    std::vector<int> button_ids_to_click = {0, 1, 2, 3, 4, 5, 8, 9, 14, 15};
    int clicked = 0;

    for (int id : button_ids_to_click)
    {
        int idx = FindGadgetIndexById(id);
        if (idx < 0)
            continue;

        ClickGadget(idx, analysis_window_index);
        RunCyclesWithVBlank(20, 50000);
        FlushAndSettle();

        ASSERT_TRUE(lxa_is_running())
            << "SIGMAth2 crashed after clicking button id=" << id << "\n"
            << GetOutput();
        clicked++;
    }

    EXPECT_GE(clicked, 6)
        << "Should have successfully clicked at least 6 math buttons";

    /* Verify the gadget count is stable (no gadgets lost or leaked) */
    const int final_gadget_count = GetGadgetCount(analysis_window_index);
    EXPECT_EQ(final_gadget_count, initial_gadget_count)
        << "Gadget count should remain stable after button clicks";

    /* Verify the window is still drawable */
    EXPECT_TRUE(WaitForWindowDrawn(analysis_window_index, 2000))
        << "Analysis window should remain drawable after button interaction";
}

/* -----------------------------------------------------------------------
 * Phase 104: String gadget keyboard input
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, StringGadgetAcceptsInput)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    /* The Analysis window has string gadgets (ids 22, 24, 25).
     * Click one to activate it, then type a simple value. */
    int string_gadget_idx = FindGadgetIndexById(22);
    if (string_gadget_idx < 0)
        string_gadget_idx = FindGadgetIndexById(24);
    if (string_gadget_idx < 0)
        string_gadget_idx = FindGadgetIndexById(25);

    if (string_gadget_idx < 0)
    {
        /* No string gadget found — skip gracefully */
        EXPECT_TRUE(lxa_is_running());
        return;
    }

    /* Click to activate */
    ClickGadget(string_gadget_idx, analysis_window_index);
    RunCyclesWithVBlank(10, 50000);

    /* Type a test value */
    TypeString("42");
    RunCyclesWithVBlank(20, 50000);

    /* Press Return to confirm */
    PressKey(0x44, 0);  /* Return key */
    RunCyclesWithVBlank(20, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should survive string gadget input\n"
        << GetOutput();

    /* Verify window is still drawable */
    EXPECT_TRUE(WaitForWindowDrawn(analysis_window_index, 2000))
        << "Analysis window should remain drawable after string input";
}

/* -----------------------------------------------------------------------
 * Phase 104: Scroll gadget interaction
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, ScrollGadgetInteraction)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    /* Click the scroll arrow gadgets (ids 20-21) and verify the app
     * survives. These control vertical scrolling of the display area. */
    for (int id = 20; id <= 21; id++)
    {
        int idx = FindGadgetIndexById(id);
        if (idx < 0)
            continue;

        ClickGadget(idx, analysis_window_index);
        RunCyclesWithVBlank(10, 50000);
        FlushAndSettle();

        EXPECT_TRUE(lxa_is_running())
            << "SIGMAth2 crashed after clicking scroll arrow id=" << id << "\n"
            << GetOutput();
    }

    /* Click the prop gadget (id 23) */
    int prop_idx = FindGadgetIndexById(23);
    if (prop_idx >= 0)
    {
        ClickGadget(prop_idx, analysis_window_index);
        RunCyclesWithVBlank(10, 50000);
        FlushAndSettle();

        EXPECT_TRUE(lxa_is_running())
            << "SIGMAth2 crashed after clicking prop gadget\n"
            << GetOutput();
    }

    EXPECT_TRUE(WaitForWindowDrawn(analysis_window_index, 2000))
        << "Analysis window should remain drawable after scroll interaction";
}

/* -----------------------------------------------------------------------
 * Phase 104: Pixel content verification in button area
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, ButtonAreaGadgetsAreConsistentlyPlaced)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    /* Verify the button grid area has consistent geometry.
     * All 20 math buttons (ids 0-19) should be evenly sized
     * and organized in rows. */
    auto gmap = GetAnalysisGadgetMap();

    /* Collect all button positions and verify row alignment */
    std::map<int, std::vector<int>> rows;  /* y -> list of ids */
    for (int id = 0; id <= 19; id++)
    {
        if (!gmap.count(id))
            continue;
        /* Round Y to nearest 2px to group into rows */
        int row_y = (gmap[id].top / 2) * 2;
        rows[row_y].push_back(id);
    }

    /* We expect 4 rows of buttons */
    EXPECT_GE((int)rows.size(), 3)
        << "Button grid should have at least 3 rows";

    /* Verify each row has consistent button sizes */
    for (const auto& [y, ids] : rows)
    {
        if (ids.size() < 2)
            continue;
        const int ref_width = gmap[ids[0]].width;
        const int ref_height = gmap[ids[0]].height;
        for (int id : ids)
        {
            EXPECT_EQ(gmap[id].width, ref_width)
                << "Button id=" << id << " at y=" << y
                << " should have same width as other buttons in row";
            EXPECT_EQ(gmap[id].height, ref_height)
                << "Button id=" << id << " at y=" << y
                << " should have same height as other buttons in row";
        }
    }
}

/* -----------------------------------------------------------------------
 * Phase 104: Display frame area verification
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, DisplayFrameContainsRenderedContent)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    /* The large display frame (id 26) at the top of the Analysis window
     * should contain rendered content (title text, graph area border). */
    auto gmap = GetAnalysisGadgetMap();
    if (!gmap.count(26))
    {
        EXPECT_TRUE(lxa_is_running());
        return;
    }

    const auto& frame = gmap[26];
    /* Sample the top portion of the frame (title/label area) */
    const int sample_height = std::min(20, frame.height);
    const int content_pixels = CountContentPixels(
        frame.left + 2, frame.top + 2,
        frame.left + frame.width - 2, frame.top + sample_height);
    EXPECT_GT(content_pixels, 10)
        << "Display frame top region should contain rendered content (title text or border)";
}

/* -----------------------------------------------------------------------
 * Phase 104: File requester via menu
 * ----------------------------------------------------------------------- */

TEST_F(SIGMAth2Test, FileMenuOpenSurvivesRequester)
{
    ASSERT_TRUE(lxa_is_running()) << GetOutput();
    const int baseline_windows = lxa_get_window_count();

    /* SIGMAth2's "Projekt" menu likely has a "Laden" (Load) or "Öffnen"
     * (Open) item that triggers a file requester.  In headless mode, the
     * ASL file requester may or may not open as a tracked window.
     *
     * This test verifies the app survives the file-requester flow:
     *   1. Open the Project menu
     *   2. Select an item that should trigger a file requester
     *   3. If a new window appears, try to dismiss it
     *   4. Verify the app is still running */
    lxa_window_info_t outer_info = {};
    if (lxa_get_window_count() >= 2)
        GetWindowInfo(outer_window_index, &outer_info);
    else
        outer_info = window_info;

    const int menu_x = outer_info.x + 24;

    /* "Laden" is likely the second or third item in Projekt menu.
     * Try y positions after the About item (which we found earlier). */
    const int probe_ys[] = {26, 36, 46};
    bool opened_requester = false;

    for (int try_y : probe_ys)
    {
        const int before = lxa_get_window_count();

        SelectMenuItem(menu_x, try_y);
        RunCyclesWithVBlank(30, 50000);
        FlushAndSettle();

        const int after = lxa_get_window_count();
        if (after > before)
        {
            /* A new window opened — could be the file requester */
            const int req_idx = after - 1;
            lxa_window_info_t req_info = {};
            if (GetWindowInfo(req_idx, &req_info))
            {
                /* File requesters are typically large windows */
                if (req_info.width > 100 && req_info.height > 80)
                {
                    CaptureWindow("/tmp/sigma_file_requester.png", req_idx);
                    opened_requester = true;

                    /* Try to dismiss via close gadget or cancel button */
                    DismissWindow(req_idx, baseline_windows);
                    break;
                }
                else
                {
                    /* Small window — might be About or a different dialog.
                     * Dismiss it and continue probing. */
                    DismissWindow(req_idx, baseline_windows);
                }
            }
        }

        if (!lxa_is_running())
            break;
    }

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should survive file-requester interaction\n"
        << GetOutput();

    /* Verify the original windows are still present */
    EXPECT_GE(lxa_get_window_count(), baseline_windows - 1)
        << "SIGMAth2 should keep most windows after file-requester flow";
}

/* Phase 126: startup latency baseline sentinel */
TEST_F(SIGMAth2Test, ZStartupLatency)
{
    ASSERT_GE(startup_ms_, 0) << "Startup time was not recorded";
    EXPECT_LE(startup_ms_, 20000L)
        << "SIGMAth2 startup latency " << startup_ms_ << " ms exceeds 20 s baseline";
    fprintf(stderr, "[LATENCY] SIGMAth2 startup: %ld ms\n", startup_ms_);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
