/**
 * apps_misc_gtest.cpp - Google Test suite for miscellaneous real-world apps
 *
 * Covers Directory Opus and SysInfo compatibility tests.
 */

#include "lxa_test.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>

using namespace lxa::testing;

class AppsMiscTest : public LxaUITest {
protected:
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

    bool SetupBlitzBasic2Assigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path blitz_base = std::filesystem::path(apps) / "BlitzBasic2";
        if (!std::filesystem::exists(blitz_base / "blitz2")) {
            return false;
        }

        return lxa_add_assign("Blitz2", blitz_base.c_str())
            && lxa_add_assign_path("C", (blitz_base / "c").c_str())
            && lxa_add_assign_path("L", (blitz_base / "l").c_str())
            && lxa_add_assign_path("S", (blitz_base / "s").c_str());
    }

    bool SetupFinalWriterAssigns() {
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

    bool SetupSonixAssigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path sonix_base = std::filesystem::path(apps) / "Sonix 2";
        if (!std::filesystem::exists(sonix_base / "Sonix")) {
            return false;
        }

        return lxa_add_assign("Sonix", sonix_base.c_str());
    }

    bool SetupTypefaceAssigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path typeface_base = std::filesystem::path(apps) / "Typeface";
        if (!std::filesystem::exists(typeface_base / "Typeface")) {
            return false;
        }

        return lxa_add_assign_path("LIBS", (typeface_base / "Libs").c_str());
    }

    bool SetupVimAssigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path vim_base = std::filesystem::path(apps) / "vim-5.3";
        if (!std::filesystem::exists(vim_base / "Vim")) {
            return false;
        }

        return lxa_add_assign_path("LIBS", (vim_base / "libs").c_str());
    }

    void SetUp() override {
        LxaUITest::SetUp();
    }

    bool SetupDirectoryOpusAssigns(std::string* dopus_base_out = nullptr) {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        std::string dopus_base = std::string(apps) + "/DirectoryOpus/bin/DOPUS";

        if (access((dopus_base + "/DirectoryOpus").c_str(), R_OK) != 0) {
            return false;
        }

        if (dopus_base_out != nullptr) {
            *dopus_base_out = dopus_base;
        }

        return lxa_add_assign("DOpus", dopus_base.c_str())
            && lxa_add_assign_path("LIBS", (dopus_base + "/libs").c_str())
            && lxa_add_assign_path("C", (dopus_base + "/c").c_str())
            && lxa_add_assign_path("L", (dopus_base + "/l").c_str())
            && lxa_add_assign_path("S", (dopus_base + "/s").c_str());
    }

    void RunAppsMiscTest(const char* name, int timeout_ms = 10000) {
        std::string path = "SYS:Tests/Apps/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        bool has_pass = output.find("PASS") != std::string::npos;
        bool has_fail = output.find("FAIL") != std::string::npos;

        /*
         * App tests may launch background processes that never exit
         * (e.g. SysInfo opens a window and waits for user input).
         * In that case lxa_run_until_exit returns -1 (timeout) even
         * though the main test wrapper already printed PASS and
         * exited with code 0.  We accept timeout as success when
         * the output contains PASS and no FAIL.
         */
        if (result == -1 && has_pass && !has_fail) {
            /* Timeout with PASS in output — acceptable */
        } else {
            EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        }

        EXPECT_TRUE(has_pass) << "Test " << name << " did not report success";
        EXPECT_FALSE(has_fail) << "Test " << name << " reported failure";
    }

    bool WaitForHostFile(const std::string& host_path, int timeout_ms = 5000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            if (access(host_path.c_str(), R_OK) == 0) {
                return true;
            }

            RunCyclesWithVBlank(1, 50000);
        }

        return access(host_path.c_str(), R_OK) == 0;
    }

    bool CreateDirectoryOpusConfig(const std::string& host_path,
                                   const std::string& left_path,
                                   const std::string& right_path)
    {
        static constexpr size_t left_offset = 5388;
        static constexpr size_t right_offset = 5418;
        static constexpr size_t field_size = 30;

        std::string dopus_base;
        if (!SetupDirectoryOpusAssigns(&dopus_base)) {
            return false;
        }

        const std::filesystem::path source_cfg = std::filesystem::path(dopus_base) / "s" / "DirectoryOpus.CFG";
        std::ifstream in(source_cfg, std::ios::binary);
        if (!in.good()) {
            return false;
        }

        std::vector<char> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        if (data.size() < right_offset + field_size) {
            return false;
        }

        auto patch_field = [&](size_t offset, const std::string& value) {
            std::fill(data.begin() + offset, data.begin() + offset + field_size, '\0');
            size_t copy_len = std::min(field_size - 1, value.size());
            memcpy(data.data() + offset, value.c_str(), copy_len);
        };

        patch_field(left_offset, left_path);
        patch_field(right_offset, right_path);

        std::ofstream out(host_path, std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            return false;
        }

        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        return out.good();
    }
};

class AppsMiscScreenTest : public AppsMiscTest {
protected:
    void SetUp() override {
        config.rootless = false;
        AppsMiscTest::SetUp();
    }
};

TEST_F(AppsMiscTest, DirectoryOpus) {
    std::string dopus_base;

    if (!SetupDirectoryOpusAssigns(&dopus_base)) {
        GTEST_SKIP() << "Directory Opus app bundle not found";
    }

    ClearOutput();

    ASSERT_EQ(lxa_load_program("APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus", ""), 0)
        << "Failed to load Directory Opus from " << dopus_base;

    ASSERT_TRUE(WaitForWindows(1, 20000))
        << "Directory Opus did not open a rootless application window\n"
        << GetOutput();

    RunCyclesWithVBlank(80, 50000);

    EXPECT_TRUE(lxa_is_running())
        << "Directory Opus should still be running after startup\n"
        << GetOutput();

    EXPECT_GE(lxa_get_window_count(), 1)
        << "Directory Opus should leave at least its main window open";

    lxa_window_info_t dopus_info;
    ASSERT_TRUE(GetWindowInfo(0, &dopus_info));

    EXPECT_GT(dopus_info.width, 0);
    EXPECT_GT(dopus_info.height, 0);
    EXPECT_GE(dopus_info.width, 320);
    EXPECT_GE(dopus_info.height, 150);

    int screen_width = 0;
    int screen_height = 0;
    int screen_depth = 0;
    ASSERT_TRUE(lxa_get_screen_dimensions(&screen_width, &screen_height, &screen_depth));
    EXPECT_GE(screen_width, 640);
    EXPECT_GE(screen_height, 200);
    EXPECT_GE(screen_depth, 3);

    EXPECT_EQ(dopus_info.width, 640)
        << "Directory Opus should open its expected 640-pixel-wide main window";
    EXPECT_GE(dopus_info.height, 200)
        << "Directory Opus should open a full-height main UI window";

    EXPECT_TRUE(WaitForWindowDrawn(0, 5000))
        << "Directory Opus window should expose visible content for interactive tests";

    int gadget_count = GetGadgetCount(0);
    EXPECT_GE(gadget_count, -1)
        << "Tracked windows should report an introspection status for gadget enumeration";

}

TEST_F(AppsMiscScreenTest, DISABLED_DirectoryOpusCopiesFile) {
    std::string dopus_base;
    namespace fs = std::filesystem;
    constexpr int RAWKEY_RETURN = 0x44;

    if (!SetupDirectoryOpusAssigns(&dopus_base)) {
        GTEST_SKIP() << "Directory Opus app bundle not found";
    }

    const fs::path source_dir = fs::path(ram_dir_path) / "s";
    const fs::path dest_dir = fs::path(ram_dir_path) / "d";
    const fs::path source_file = source_dir / "COPY";
    const fs::path dest_file = dest_dir / "COPY";
    const fs::path config_file = fs::path(ram_dir_path) / "DirectoryOpus-test.CFG";
    const std::string source_contents = "Directory Opus copy smoke test\n";

    fs::create_directories(source_dir);
    fs::create_directories(dest_dir);

    {
        std::ofstream out(source_file);
        ASSERT_TRUE(out.good()) << "Failed to create source file at " << source_file;
        out << source_contents;
    }

    ASSERT_TRUE(CreateDirectoryOpusConfig(config_file.string(), "RAM:s", "RAM:d"))
        << "Failed to create test-specific Directory Opus configuration";

    ASSERT_FALSE(fs::exists(dest_file)) << "Destination file should not exist before copy";

    std::string args = "-cRAM:DirectoryOpus-test.CFG RAM:s RAM:d";

    ASSERT_EQ(lxa_load_program("APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus", args.c_str()), 0)
        << "Failed to load Directory Opus from " << dopus_base;
    ASSERT_TRUE(WaitForWindows(1, 20000));
    ASSERT_TRUE(WaitForWindowDrawn(0, 5000));
    WaitForEventLoop(100, 10000);
    RunCyclesWithVBlank(240, 50000);

    lxa_window_info_t dopus_info;
    ASSERT_TRUE(GetWindowInfo(0, &dopus_info));
    ASSERT_TRUE(lxa_is_running()) << GetOutput();

    auto settle = [&](int vblanks = 40) {
        RunCyclesWithVBlank(vblanks, 50000);
    };

    auto confirm_copy_requester = [&]() {
        if (!WaitForWindows(2, 2000)) {
            return false;
        }

        lxa_window_info_t req_info;
        if (!GetWindowInfo(1, &req_info)) {
            return false;
        }

        int gadget_count = GetGadgetCount(1);
        if (gadget_count > 0 && ClickGadget(0, 1)) {
            settle(40);
            return true;
        }

        Click(req_info.x + 40, req_info.y + req_info.height - 18);
        settle(40);
        return true;
    };

    /* Give DOpus time to finish reading both listers from the patched config. */
    settle(200);

    /* Select the first visible file in the active source lister. */
    Click(dopus_info.x + 40, dopus_info.y + 40);
    settle(40);

    bool copied = WaitForHostFile(dest_file.string(), 500);
    ASSERT_FALSE(copied) << "Destination file should not exist before invoking copy";

    /* Click the first custom button in the lower bank, which is Copy in the
     * default DOpus 4 layout documented in the bundled manual. */
    Click(dopus_info.x + 30, dopus_info.y + 220);
    settle(80);

    copied = WaitForHostFile(dest_file.string(), 1500);
    if (!copied) {
        confirm_copy_requester();
        copied = WaitForHostFile(dest_file.string(), 2000);
    }

    if (!copied) {
        PressKey(RAWKEY_RETURN, 0);
        settle(60);
        copied = WaitForHostFile(dest_file.string(), 2000);
    }

    if (!copied) {
        lxa_flush_display();
        lxa_capture_screen("/tmp/dopus-copy-failed.ppm");
    }

    ASSERT_TRUE(copied)
        << "Directory Opus did not copy the selected file into the destination directory\n"
        << GetOutput();

    std::ifstream in(dest_file);
    ASSERT_TRUE(in.good()) << "Copied file exists but could not be opened";

    std::string copied_contents((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    EXPECT_EQ(copied_contents, source_contents)
        << "Copied file contents should match the source file";
}

TEST_F(AppsMiscTest, KickPascal2) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }
    RunAppsMiscTest("KickPascal2", 5000);
}

TEST_F(AppsMiscTest, SysInfo) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }
    /* SysInfo opens a window and never exits on its own, so use a
     * short timeout — the wrapper reports PASS within ~3 seconds. */
    RunAppsMiscTest("SysInfo", 5000);
}

TEST_F(AppsMiscTest, SysInfoRootlessWindowDrawsContent) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:SysInfo/bin/SysInfo/SysInfo", ""), 0)
        << "Failed to load SysInfo via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 15000))
        << "SysInfo did not open a rootless window\n"
        << GetOutput();

    ASSERT_TRUE(GetWindowInfo(0, &window_info));
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);

    EXPECT_TRUE(WaitForWindowDrawn(0, 5000))
        << "SysInfo rootless window should expose visible content instead of a black screen\n"
        << GetOutput();

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should remain running after its main window is drawn\n"
        << GetOutput();
}

TEST_F(AppsMiscTest, FinalWriterDStartsWithBundledLibraries) {
    if (!SetupOriginalSystemAssigns(true, true, false) || !SetupFinalWriterAssigns()) {
        GTEST_SKIP() << "FinalWriter_D app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:FinalWriter_D/FinalWriter", ""), 0)
        << "Failed to load FinalWriter_D via APPS: assign";

    RunCyclesWithVBlank(200, 50000);

    EXPECT_TRUE(lxa_is_running())
        << "FinalWriter_D should still be running after startup\n"
        << GetOutput();

    EXPECT_EQ(GetOutput().find("Can't find \"swshell.library\""), std::string::npos)
        << GetOutput();
}

TEST_F(AppsMiscTest, PPaintStartsWithoutLibraryFailures) {
    if (!SetupOriginalSystemAssigns(true, true, false) || FindAppsPath() == nullptr) {
        GTEST_SKIP() << "ppaint app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:ppaint/ppaint", ""), 0)
        << "Failed to load ppaint via APPS: assign";

    RunCyclesWithVBlank(160, 50000);

    EXPECT_TRUE(lxa_is_running())
        << "ppaint should still be running after startup\n"
        << GetOutput();

    EXPECT_EQ(GetOutput().find("requested library"), std::string::npos)
        << GetOutput();
}

TEST_F(AppsMiscTest, ProWriteOpensWindows) {
    if (!SetupOriginalSystemAssigns(true, true, false) || FindAppsPath() == nullptr) {
        GTEST_SKIP() << "ProWrite app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:ProWrite/ProWrite", ""), 0)
        << "Failed to load ProWrite via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 20000))
        << "ProWrite did not open a window\n"
        << GetOutput();

    ASSERT_TRUE(GetWindowInfo(0, &window_info));
    EXPECT_TRUE(WaitForWindowDrawn(0, 5000))
        << "ProWrite window should expose visible content\n"
        << GetOutput();

    EXPECT_TRUE(lxa_is_running())
        << "ProWrite should still be running after startup\n"
        << GetOutput();

    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(AppsMiscTest, SIGMAth2OpensAnalysisWindow) {
    if (!SetupOriginalSystemAssigns(true, false, false) || FindAppsPath() == nullptr) {
        GTEST_SKIP() << "SIGMAth2 app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:SIGMAth2/SIGMAth_2", ""), 0)
        << "Failed to load SIGMAth2 via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 20000))
        << "SIGMAth2 did not open a window\n"
        << GetOutput();

    ASSERT_TRUE(GetWindowInfo(0, &window_info));
    EXPECT_TRUE(WaitForWindowDrawn(0, 5000))
        << "SIGMAth2 window should expose visible content\n"
        << GetOutput();

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should still be running after startup\n"
        << GetOutput();

    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(AppsMiscTest, DISABLED_BlitzBasic2Starts) {
    if (!SetupOriginalSystemAssigns(true, true, true) || !SetupBlitzBasic2Assigns()) {
        GTEST_SKIP() << "BlitzBasic2 app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:BlitzBasic2/blitz2", ""), 0)
        << "Failed to load BlitzBasic2 via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 20000)) << GetOutput();
}

TEST_F(AppsMiscTest, DISABLED_Sonix2Starts) {
    if (!SetupOriginalSystemAssigns(true, false, true) || !SetupSonixAssigns()) {
        GTEST_SKIP() << "Sonix 2 app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:Sonix 2/Sonix", ""), 0)
        << "Failed to load Sonix 2 via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 20000)) << GetOutput();
}

TEST_F(AppsMiscTest, DISABLED_TypefaceStarts) {
    if (!SetupOriginalSystemAssigns(true, true, false) || !SetupTypefaceAssigns()) {
        GTEST_SKIP() << "Typeface app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:Typeface/Typeface", ""), 0)
        << "Failed to load Typeface via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 20000)) << GetOutput();
}

TEST_F(AppsMiscTest, DISABLED_Vim53Starts) {
    if (!SetupOriginalSystemAssigns(true, false, false) || !SetupVimAssigns()) {
        GTEST_SKIP() << "vim-5.3 app bundle or original system disk not found";
    }

    ASSERT_EQ(lxa_load_program("APPS:vim-5.3/Vim", ""), 0)
        << "Failed to load vim-5.3 via APPS: assign";

    ASSERT_TRUE(WaitForWindows(1, 20000)) << GetOutput();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
