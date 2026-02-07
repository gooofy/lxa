/**
 * apps_misc_gtest.cpp - Google Test suite for miscellaneous real-world apps
 *
 * Covers Directory Opus and SysInfo compatibility tests.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class AppsMiscTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
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
};

TEST_F(AppsMiscTest, DirectoryOpus) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }
    RunAppsMiscTest("Dopus", 5000);
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
