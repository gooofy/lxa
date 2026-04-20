/**
 * vim_gtest.cpp - Google Test coverage for vim-5.3 startup and editing.
 */

#include "lxa_test.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <time.h>

using namespace lxa::testing;

class VimTest : public LxaUITest {
protected:
    static constexpr int RAWKEY_ESCAPE = 0x45;
    long startup_ms_ = -1;  /* Phase 126: latency baseline */

    std::filesystem::path vim_base_path;
    std::filesystem::path vim_home_path;

    bool WriteHostFile(const std::filesystem::path& destination,
                       const std::string& contents) {
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            return false;
        }

        out << contents;
        return out.good();
    }

    bool WriteEnvVar(const std::string& name, const std::string& value) {
        std::ofstream out(std::filesystem::path(env_dir_path) / name,
                          std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            return false;
        }

        out << value;
        return out.good();
    }

    bool SetupOriginalSystemAssigns(bool add_libs = false) {
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

        return true;
    }

    bool SetupVimAssigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        vim_base_path = std::filesystem::path(apps) / "vim-5.3";
        if (!std::filesystem::exists(vim_base_path / "Vim")) {
            return false;
        }

        return lxa_add_assign("VIM", vim_base_path.c_str())
            && lxa_add_assign_path("LIBS", (vim_base_path / "libs").c_str());
    }

    void SlowTypeString(const char* str, int settle_vblanks = 3) {
        char ch[2] = {0, 0};

        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks, 50000);
        }
    }

    bool WaitForHostFile(const std::string& host_path, int timeout_ms = 5000) {
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            if (access(host_path.c_str(), R_OK) == 0) {
                return true;
            }

            RunCyclesWithVBlank(1, 50000);
        }

        return access(host_path.c_str(), R_OK) == 0;
    }

    bool WaitForHostFileContains(const std::string& host_path,
                                 const std::string& expected,
                                 int timeout_ms = 5000) {
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            if (WaitForHostFile(host_path, 100)) {
                std::ifstream in(host_path, std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
                if (content.find(expected) != std::string::npos) {
                    return true;
                }
            }

            RunCyclesWithVBlank(1, 50000);
        }

        return false;
    }

    bool WaitForOutputContains(const std::string& needle, int timeout_ms = 5000) {
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            if (GetOutput().find(needle) != std::string::npos) {
                return true;
            }

            RunCyclesWithVBlank(1, 50000);
        }

        return GetOutput().find(needle) != std::string::npos;
    }

    void LaunchVim(const std::string& args, const std::string& ready_marker) {
        struct timespec _t0, _t1;
        clock_gettime(CLOCK_MONOTONIC, &_t0);
        ASSERT_EQ(lxa_load_program("APPS:vim-5.3/Vim", args.c_str()), 0)
            << "Failed to load vim-5.3 via APPS: assign";

        ASSERT_TRUE(WaitForOutputContains(ready_marker, 20000))
            << "vim-5.3 did not render expected startup text\n"
            << GetOutput();
        clock_gettime(CLOCK_MONOTONIC, &_t1);
        startup_ms_ = (_t1.tv_sec - _t0.tv_sec) * 1000L +
                      (_t1.tv_nsec - _t0.tv_nsec) / 1000000L;

        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(40, 50000);
    }

    void SetUp() override {
        LxaUITest::SetUp();

        vim_home_path = std::filesystem::path(t_dir_path) / "vim-home";
        ASSERT_TRUE(std::filesystem::create_directories(vim_home_path) ||
                    std::filesystem::exists(vim_home_path));
        ASSERT_TRUE(lxa_add_drive("HOME", vim_home_path.c_str()));

        if (!SetupOriginalSystemAssigns(true) || !SetupVimAssigns()) {
            GTEST_SKIP() << "vim-5.3 app bundle or original system disk not found";
        }

        ASSERT_TRUE(WriteEnvVar("TERM", "amiga"));
        ASSERT_TRUE(WriteEnvVar("VIM", "VIM:"));
    }
};

TEST_F(VimTest, StartupOpensFileAndDrawsEditor) {
    const std::filesystem::path startup_file = std::filesystem::path(t_dir_path) / "vim-startup.txt";
    const std::string startup_contents = "alpha line\nbeta line\n";

    ASSERT_TRUE(WriteHostFile(startup_file, startup_contents));

    LaunchVim("-n -u NONE -i NONE -f T:vim-startup.txt", "2 lines, 21 characters");

    EXPECT_TRUE(lxa_is_running()) << "vim-5.3 should still be running after startup";
    EXPECT_NE(GetOutput().find("2 lines, 21 characters"), std::string::npos)
        << "vim-5.3 should report file metadata in the status line";
    EXPECT_TRUE(WaitForOutputContains("alpha line", 5000))
        << "vim-5.3 should render file contents in the editor area\n"
        << GetOutput();
}

TEST_F(VimTest, CursorAndInsertModeRedrawWindow) {
    const std::filesystem::path host_file = std::filesystem::path(ram_dir_path) / "vim-cursor.txt";

    LaunchVim("-n -u NONE -i NONE -f RAM:vim-cursor.txt", "[New File]");

    SlowTypeString("iZ", 20);
    RunCyclesWithVBlank(60, 50000);

    PressKey(RAWKEY_ESCAPE, 0);
    RunCyclesWithVBlank(40, 50000);
    SlowTypeString(":wq\n", 20);

    ASSERT_EQ(lxa_run_until_exit(10000), 0)
        << "vim-5.3 should exit cleanly after saving from insert mode\n"
        << GetOutput();

    ASSERT_TRUE(WaitForHostFileContains(host_file.string(), "Z", 5000))
        << "vim-5.3 should persist inserted text after returning to command mode";
    EXPECT_NE(GetOutput().find("written"), std::string::npos)
        << "vim-5.3 should report the write in its status area\n"
        << GetOutput();
}

TEST_F(VimTest, InsertModeTypingAndCommandModeWriteFile) {
    LaunchVim("-n -u NONE -i NONE -f RAM:vim-modecheck.txt", "[New File]");
    ClearOutput();

    SlowTypeString("iX", 20);
    RunCyclesWithVBlank(80, 50000);
    PressKey(RAWKEY_ESCAPE, 0);
    RunCyclesWithVBlank(40, 50000);
    SlowTypeString(":wq\n", 20);

    ASSERT_EQ(lxa_run_until_exit(10000), 0)
        << "vim-5.3 should exit cleanly after :wq\n"
        << GetOutput();

    const std::filesystem::path host_file = std::filesystem::path(ram_dir_path) / "vim-modecheck.txt";
    std::ifstream in(host_file, std::ios::binary);
    ASSERT_TRUE(in.good()) << "Expected :wq to create RAM:vim-modecheck.txt";

    std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(saved.find("X"), std::string::npos)
        << "Insert mode text should be persisted after switching back to command mode";
}

TEST_F(VimTest, TypingAndSavingWritesHostFile) {
    const std::filesystem::path host_file = std::filesystem::path(ram_dir_path) / "vim-phase100.txt";
    std::ifstream in;
    std::string saved;

    LaunchVim("-n -u NONE -i NONE -f RAM:vim-phase100.txt", "[New File]");

    SlowTypeString("ihi", 20);
    RunCyclesWithVBlank(80, 50000);
    PressKey(RAWKEY_ESCAPE, 0);
    RunCyclesWithVBlank(40, 50000);

    SlowTypeString(":wq\n", 20);

    ASSERT_EQ(lxa_run_until_exit(10000), 0)
        << "vim-5.3 should exit cleanly after writing the file\n"
        << GetOutput();

    ASSERT_TRUE(WaitForHostFileContains(host_file.string(), "hi", 5000))
        << "Expected Vim to write RAM:vim-phase100.txt";

    in.open(host_file);
    ASSERT_TRUE(in.good()) << "Failed to open saved Vim output file";
    saved.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    EXPECT_NE(saved.find("hi"), std::string::npos)
        << "Saved file should contain the typed buffer contents";
}

/* Phase 126: startup latency baseline sentinel */
TEST_F(VimTest, ZStartupLatency) {
    LaunchVim("-n -u NONE -i NONE -f RAM:vim-latency.txt", "[New File]");
    ASSERT_GE(startup_ms_, 0) << "Startup time was not recorded";
    EXPECT_LE(startup_ms_, 20000L)
        << "Vim startup latency " << startup_ms_ << " ms exceeds 20 s baseline";
    fprintf(stderr, "[LATENCY] Vim startup: %ld ms\n", startup_ms_);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
