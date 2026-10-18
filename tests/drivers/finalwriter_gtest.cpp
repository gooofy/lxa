#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <time.h>

using namespace lxa::testing;

class FinalWriterTest : public LxaUITest {
protected:
    long startup_ms_ = -1;  /* Phase 126: latency baseline */

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

    /* runs after the assigns, before FinalWriter is loaded */
    virtual void BeforeLoad() {}

    void SetUp() override
    {
        LxaUITest::SetUp();

        if (!SetupOriginalSystemAssigns(true, true, false) || !SetupFinalWriterAssigns()) {
            GTEST_SKIP() << "FinalWriter_D app bundle or original system disk not found";
        }
        BeforeLoad();
        if (HasFatalFailure())
            return;

        struct timespec _t0, _t1;
        clock_gettime(CLOCK_MONOTONIC, &_t0);
        ASSERT_EQ(lxa_load_program("APPS:FinalWriter_D/FinalWriter", ""), 0)
            << "Failed to load FinalWriter_D via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 20000))
            << "FinalWriter_D did not open a tracked window\n"
            << GetOutput();
        clock_gettime(CLOCK_MONOTONIC, &_t1);
        startup_ms_ = (_t1.tv_sec - _t0.tv_sec) * 1000L +
                      (_t1.tv_nsec - _t0.tv_nsec) / 1000000L;

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
    /* AmigaOS 3.1 reference (app-finalwriter-d): the startup dialog is
     * 117 pixels high (WBorTop 2 + topaz 8 title bar) */
    EXPECT_GE(window_info.height, 117);
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

/* Phase 243: the `rtg` reference profile - Picasso96 with a uaegfx board,
 * the Workbench in UAE 800x600 8 bit (ENV:Sys/screenmode.prefs, installed
 * by C:IPrefs at boot as tools/refsys/install_p96.py does on the reference).
 * There "Workbench verwenden" + OK opens the editor on the Workbench
 * (scenario finalwriter-rtg, golden tests/golden/FinalWriter_D). */
class FinalWriterRtgTest : public FinalWriterTest {
protected:
    FinalWriterRtgTest() { config.rtg = true; }

    static void Be32(std::vector<uint8_t>& v, uint32_t x)
    {
        v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
    }

    static void Be16(std::vector<uint8_t>& v, uint16_t x)
    {
        v.push_back(x >> 8); v.push_back(x);
    }

    void BeforeLoad() override
    {
        ASSERT_FALSE(env_dir_path.empty());
        const std::filesystem::path sys = std::filesystem::path(env_dir_path) / "Sys";
        std::filesystem::create_directories(sys);

        /* FORM PREF { PRHD, SCRM } - struct ScreenModePrefs: 4 reserved
         * longs, DisplayID 0x50041000 (UAE 800x600 8 bit), 800 x 600,
         * depth 8, control 1 (autoscroll) */
        std::vector<uint8_t> scrm;
        for (int i = 0; i < 4; ++i) Be32(scrm, 0);
        Be32(scrm, 0x50041000);
        Be16(scrm, 800); Be16(scrm, 600); Be16(scrm, 8); Be16(scrm, 1);

        std::vector<uint8_t> body = {'P', 'R', 'E', 'F', 'P', 'R', 'H', 'D'};
        Be32(body, 6);
        body.insert(body.end(), 6, 0);
        body.insert(body.end(), {'S', 'C', 'R', 'M'});
        Be32(body, scrm.size());
        body.insert(body.end(), scrm.begin(), scrm.end());
        std::vector<uint8_t> form = {'F', 'O', 'R', 'M'};
        Be32(form, body.size());
        form.insert(form.end(), body.begin(), body.end());

        FILE *f = fopen((sys / "screenmode.prefs").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        fwrite(form.data(), 1, form.size(), f);
        fclose(f);
    }

    /* the "Workbench verwenden" radio button: the topmost wide, 16 pixel
     * high boolean gadget at the left of the dialog */
    int FindUseWorkbenchGadget()
    {
        auto gadgets = GetGadgets(0);
        int best_index = -1;
        int best_top = 1000000;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            const auto& gad = gadgets[i];
            if (gad.width < 150 || gad.height != 16 || gad.left > window_info.x + 100)
                continue;
            if (gad.top < best_top) {
                best_top = gad.top;
                best_index = static_cast<int>(i);
            }
        }
        return best_index;
    }
};

TEST_F(FinalWriterRtgTest, AcceptDialogOpensEditorWindow)
{
    /* AmigaOS 3.1 rtg reference: swshell.library centres the dialog in
     * the DisplayClip of the 800x600 Workbench (probe rtg/wbscreen) */
    EXPECT_EQ(window_info.x, 163);
    EXPECT_EQ(window_info.y, 139);

    const int use_wb = FindUseWorkbenchGadget();
    ASSERT_GE(use_wb, 0) << "Could not find the \"Workbench verwenden\" gadget";
    lxa_gadget_info_t wb_info = {};
    ASSERT_TRUE(GetGadgetInfo(use_wb, &wb_info, 0));
    Click(wb_info.left + 10, wb_info.top + wb_info.height / 2);
    RunFrames(25);

    const int ok_gadget = FindBottomButtonGadget(true);
    ASSERT_GE(ok_gadget, 0) << "Could not find OK button gadget";
    lxa_gadget_info_t ok_info = {};
    ASSERT_TRUE(GetGadgetInfo(ok_gadget, &ok_info, 0));
    Click(ok_info.left + ok_info.width / 2, ok_info.top + ok_info.height / 2);

    /* FinalWriter loads its fonts and spawns two child processes before
     * the editor window opens */
    bool editor = false;
    EmuDeadline deadline(60000);
    while (!deadline.Expired() && lxa_is_running()) {
        RunFrames(10);
        lxa_window_info_t wi;
        if (lxa_get_window_count() > 0 && GetWindowInfo(0, &wi) &&
            std::string(wi.title).rfind("Unbenannt - Hauptteil", 0) == 0) {
            editor = true;
            break;
        }
    }
    ASSERT_TRUE(editor) << "Editor window did not appear after accepting startup dialog\n"
                        << "running=" << lxa_is_running() << " output=[" << GetOutput() << "]";
    ASSERT_TRUE(WaitForWindowDrawn(0, 10000)) << "Editor window did not draw visible content";
    WaitForStableContent(500);

    /* reference: the editor fills the Workbench below its title bar */
    lxa_window_info_t editor_info;
    ASSERT_TRUE(GetWindowInfo(0, &editor_info));
    EXPECT_TRUE(lxa_is_running()) << "FinalWriter should still be running";
    EXPECT_EQ(editor_info.width, 800);
    EXPECT_EQ(editor_info.height, 588);

    const std::string capture_path = ram_dir_path + "/finalwriter-editor.png";
    EXPECT_TRUE(CaptureWindow(capture_path.c_str(), 0));
}

/* ---------------------------------------------------------------------- */
/* Phase 124 Z-tests: deeper coverage of the FinalWriter startup dialog   */
/* on the classic (aga) Workbench; the editor is reached in the rtg       */
/* profile by FinalWriterRtgTest above (Phase 243).                       */
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

/* Phase 126: startup latency baseline sentinel */
TEST_F(FinalWriterTest, ZStartupLatency)
{
    ASSERT_GE(startup_ms_, 0) << "Startup time was not recorded";
    EXPECT_LE(startup_ms_, 20000L)
        << "FinalWriter startup latency " << startup_ms_ << " ms exceeds 20 s baseline";
    fprintf(stderr, "[LATENCY] FinalWriter startup: %ld ms\n", startup_ms_);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
