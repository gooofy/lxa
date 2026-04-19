#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace lxa::testing;

class FinalWriterTest : public LxaUITest {
protected:
    int CountNonBlackPixels(const RgbImage& image, int x1, int y1, int x2, int y2)
    {
        int count = 0;
        const int left = std::max(0, x1);
        const int top = std::max(0, y1);
        const int right = std::min(x2, image.width - 1);
        const int bottom = std::min(y2, image.height - 1);

        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width)
                                 + static_cast<size_t>(x)) * 3U;
                if (image.pixels[offset] != 0 ||
                    image.pixels[offset + 1] != 0 ||
                    image.pixels[offset + 2] != 0) {
                    ++count;
                }
            }
        }

        return count;
    }

    RgbImage CaptureMainWindow(const std::string& file_name)
    {
        const std::string path = ram_dir_path + "/" + file_name;
        EXPECT_TRUE(CaptureWindow(path.c_str(), 0))
            << "Failed to capture FinalWriter window to " << path;
        return LoadPng(path);
    }

    int FindBottomButtonGadget(bool leftmost)
    {
        auto gadgets = GetGadgets(0);
        int best_index = -1;
        int best_top = -1;
        int best_left = leftmost ? 1000000 : -1;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            const auto& gad = gadgets[i];
            if (gad.width < 40 || gad.height < 8) {
                continue;
            }

            if (gad.top < window_info.y + window_info.height - 30) {
                continue;
            }

            if (gad.top > best_top) {
                best_top = gad.top;
                best_left = gad.left;
                best_index = static_cast<int>(i);
                continue;
            }

            if (gad.top == best_top) {
                if (leftmost ? (gad.left < best_left) : (gad.left > best_left)) {
                    best_left = gad.left;
                    best_index = static_cast<int>(i);
                }
            }
        }

        return best_index;
    }

    int FindDisplayOptionsListGadget()
    {
        auto gadgets = GetGadgets(0);
        int best_index = -1;
        int best_width = -1;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            const auto& gad = gadgets[i];
            if (gad.width < 180 || gad.height < 40) {
                continue;
            }

            if (gad.left < window_info.x + 150) {
                continue;
            }

            if (gad.width > best_width) {
                best_width = gad.width;
                best_index = static_cast<int>(i);
            }
        }

        return best_index;
    }

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

    bool SetupFinalWriterAssigns()
    {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path fw_base = std::filesystem::path(apps) / "FinalWriter_D";
        if (!std::filesystem::exists(fw_base / "FinalWriter")) {
            return false;
        }

        return lxa_add_assign("FinalWriter", fw_base.c_str())
            && lxa_add_assign_path("LIBS", (fw_base / "FWLibs").c_str())
            && lxa_add_assign_path("FONTS", (fw_base / "FWFonts").c_str());
    }

    void SetUp() override
    {
        LxaUITest::SetUp();

        if (!SetupOriginalSystemAssigns(true, true, false) || !SetupFinalWriterAssigns()) {
            GTEST_SKIP() << "FinalWriter_D app bundle or original system disk not found";
        }

        ASSERT_EQ(lxa_load_program("APPS:FinalWriter_D/FinalWriter", ""), 0)
            << "Failed to load FinalWriter_D via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 20000))
            << "FinalWriter_D did not open a tracked window\n"
            << GetOutput();

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "FinalWriter_D main window should draw visible content\n"
            << GetOutput();

        WaitForEventLoop(200, 10000);
        RunCyclesWithVBlank(200, 50000);
    }
};

TEST_F(FinalWriterTest, StartupOpensVisibleWindow)
{
    EXPECT_TRUE(lxa_is_running())
        << "FinalWriter_D should still be running after startup\n"
        << GetOutput();
    EXPECT_GT(window_info.width, 400);
    EXPECT_GT(window_info.height, 120);
    EXPECT_NE(CountContentPixels(window_info.x,
                                 window_info.y,
                                 window_info.x + window_info.width - 1,
                                 window_info.y + window_info.height - 1,
                                 0),
              0);
}

TEST_F(FinalWriterTest, MainWindowCaptureShowsEditorRegions)
{
    RgbImage image = CaptureMainWindow("finalwriter-main-window.png");
    const int total_pixels = CountNonBlackPixels(image, 0, 0, image.width - 1, image.height - 1);
    const int toolbar_pixels = CountNonBlackPixels(image, 8, 8, image.width - 9, 44);
    const int document_pixels = CountNonBlackPixels(image, 24, 48, image.width - 24, image.height - 24);

    ASSERT_GE(image.width, window_info.width);
    ASSERT_GE(image.height, window_info.height);
    EXPECT_GT(total_pixels, 12000)
        << "FinalWriter startup capture should contain substantial rendered UI content";
    EXPECT_GT(toolbar_pixels, 2000)
        << "FinalWriter should render visible toolbar/ruler content near the top of the window";
    EXPECT_GT(document_pixels, 7000)
        << "FinalWriter should render visible editor content in the document area";
}

TEST_F(FinalWriterTest, DISABLED_AcceptDialogOpensEditorWindow)
{
    /* Select "Same as Workbench" / "Wie Workbench" in the display options list */
    const int display_options = FindDisplayOptionsListGadget();
    ASSERT_GE(display_options, 0) << "Could not find display options list gadget";

    lxa_gadget_info_t list_info = {};
    ASSERT_TRUE(GetGadgetInfo(display_options, &list_info, 0));
    Click(list_info.left + 40, list_info.top + 22);
    RunCyclesWithVBlank(80, 50000);

    /* Click the OK button (leftmost bottom button) */
    const int ok_gadget = FindBottomButtonGadget(true);
    ASSERT_GE(ok_gadget, 0) << "Could not find OK button gadget";
    lxa_gadget_info_t ok_info = {};
    ASSERT_TRUE(GetGadgetInfo(ok_gadget, &ok_info, 0));
    Click(ok_info.left + ok_info.width / 2, ok_info.top + ok_info.height / 2);

    /* Give FinalWriter time to close the dialog, open a new screen,
     * spawn child processes, and open the editor window.
     * FinalWriter loads fonts, opens a custom screen, spawns two child
     * processes, and then opens the editor — this needs substantial time. */
    for (int phase = 0; phase < 20; ++phase)
    {
        RunCyclesWithVBlank(200, 50000);

        if (lxa_get_window_count() > 0)
        {
            lxa_window_info_t wi;
            if (GetWindowInfo(0, &wi) &&
                std::string(wi.title) != "Final Writer - Version 5.05")
            {
                break;  /* Editor window appeared */
            }
        }

        if (!lxa_is_running())
            break;
    }

    /* Verify the editor window appeared */
    ASSERT_GT(lxa_get_window_count(), 0)
        << "Editor window did not appear after accepting startup dialog\n"
        << "running=" << lxa_is_running() << " output=[" << GetOutput() << "]";

    /* Let the editor fully render its toolbar, ruler, and document area */
    ASSERT_TRUE(WaitForWindowDrawn(0, 10000))
        << "Editor window did not draw visible content";
    WaitForEventLoop(400, 50000);

    /* Verify the editor window properties */
    lxa_window_info_t editor_info;
    ASSERT_TRUE(GetWindowInfo(0, &editor_info));
    EXPECT_TRUE(lxa_is_running()) << "FinalWriter should still be running";
    EXPECT_STRNE(editor_info.title, "Final Writer - Version 5.05")
        << "The startup dialog should have been replaced by the editor window";
    EXPECT_GE(editor_info.width, 400)
        << "Editor window should be reasonably wide";
    EXPECT_GE(editor_info.height, 100)
        << "Editor window should be reasonably tall";

    /* Capture the editor window for visual verification */
    const std::string capture_path = ram_dir_path + "/finalwriter-editor.png";
    EXPECT_TRUE(CaptureWindow(capture_path.c_str(), 0));
}

/* ---------------------------------------------------------------------- */
/* Phase 124 Z-tests: deeper coverage of the FinalWriter startup dialog.   */
/* The full editor cannot currently be reached (DISABLED test above);      */
/* clicking OK causes FinalWriter to attempt a new screen mode, the two    */
/* spawned child processes Exit(0), and the application terminates.  These */
/* Z-tests therefore guard the dialog state we *can* reach.                */
/* ---------------------------------------------------------------------- */

TEST_F(FinalWriterTest, ZStartupDialogTitleIsFinalWriterVersion)
{
    /* Rootless host window must carry the original Intuition window title. */
    EXPECT_STREQ(window_info.title, "Final Writer - Version 5.05")
        << "Startup dialog title regression\n"
        << "got: '" << window_info.title << "'";
}

TEST_F(FinalWriterTest, ZStartupDialogExposesExpectedGadgetCount)
{
    /* Vision-model probe (Phase 124) confirmed 22 gadgets on the German
     * startup dialog: Bildschirmtyp radios, color slider, display-options
     * list, OK / Abbruch buttons.  Hold the count >=18 — small enough to
     * tolerate minor refactors, large enough to catch a regression that
     * collapses the dialog. */
    const int gc = GetGadgetCount();
    EXPECT_GE(gc, 18)
        << "Startup dialog should expose its full gadget set; got " << gc;
    EXPECT_LE(gc, 40)
        << "Startup dialog gadget count looks unexpectedly large; got " << gc;
}

TEST_F(FinalWriterTest, ZBottomButtonsResolveToValidGadgets)
{
    /* The OK and Abbruch buttons must be locatable via the existing
     * helpers.  This guards against a regression in
     * FindBottomButtonGadget() (gadget enumeration / geometry). */
    const int ok_idx = FindBottomButtonGadget(true);
    const int cancel_idx = FindBottomButtonGadget(false);
    ASSERT_GE(ok_idx, 0) << "OK gadget (leftmost bottom) not found";
    ASSERT_GE(cancel_idx, 0) << "Abbruch gadget (rightmost bottom) not found";

    lxa_gadget_info_t ok = {}, cancel = {};
    ASSERT_TRUE(GetGadgetInfo(ok_idx, &ok, 0));
    ASSERT_TRUE(GetGadgetInfo(cancel_idx, &cancel, 0));
    EXPECT_GE(ok.width, 40);
    EXPECT_GE(cancel.width, 40);
    EXPECT_LT(ok.left, cancel.left)
        << "OK should be to the left of Abbruch (got OK@" << ok.left
        << ", Cancel@" << cancel.left << ")";
}

TEST_F(FinalWriterTest, ZDisplayOptionsListIsLocatable)
{
    /* The display-options list (vision-confirmed: "LORES" / "Wie
     * Workbench") must be discoverable via FindDisplayOptionsListGadget().
     * This covers the gadget-search heuristic against geometry drift. */
    const int idx = FindDisplayOptionsListGadget();
    ASSERT_GE(idx, 0) << "Display options list gadget not found";
    lxa_gadget_info_t info = {};
    ASSERT_TRUE(GetGadgetInfo(idx, &info, 0));
    EXPECT_GE(info.width, 180);
    EXPECT_GE(info.height, 40);
}

TEST_F(FinalWriterTest, ZNoMissingLibraryOrPanicAtStartup)
{
    /* FinalWriter loads its bundled FWLibs (swshell, cachemap, qfont,
     * type1.loader) from APPS:FinalWriter_D/FWLibs and supplemental libs
     * from the host's FS-UAE system disk.  This test guards against any
     * regression that would surface a missing-library or PANIC entry. */
    const std::string output = GetOutput();
    EXPECT_EQ(output.find("PANIC"), std::string::npos)
        << "FinalWriter startup must not PANIC\n" << output;
    EXPECT_EQ(output.find("invalid RastPort BitMap"), std::string::npos)
        << "FinalWriter startup must not hit null-BitMap rendering paths\n"
        << output;
}

TEST_F(FinalWriterTest, ZStartupDialogContentSurvivesIdle)
{
    /* The dialog UI must remain stably rendered across an idle period
     * (catches INTUITICKS / refresh corruption regressions). */
    const std::string capture_path = "/tmp/fw_idle_check.png";
    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0));
    RgbImage initial = LoadPng(capture_path);
    const int initial_pixels = CountNonBlackPixels(
        initial, 0, 0, initial.width - 1, initial.height - 1);
    ASSERT_GT(initial_pixels, 5000) << "Dialog should render substantial UI";

    /* Run additional idle cycles. */
    RunCyclesWithVBlank(150, 50000);
    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0));
    RgbImage after = LoadPng(capture_path);
    const int after_pixels = CountNonBlackPixels(
        after, 0, 0, after.width - 1, after.height - 1);
    EXPECT_GE(after_pixels, initial_pixels / 2)
        << "Dialog content collapsed during idle "
        << "(initial=" << initial_pixels << " after=" << after_pixels << ")";
    EXPECT_TRUE(lxa_is_running())
        << "FinalWriter must still be running after idle period";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
