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

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        EXPECT_NE(output.find("PASS"), std::string::npos) << "Test " << name << " did not report success";
    }
};

TEST_F(AppsMiscTest, DirectoryOpus) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }
    RunAppsMiscTest("Dopus", 20000);
}

TEST_F(AppsMiscTest, KickPascal2) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }
    RunAppsMiscTest("KickPascal2", 20000);
}

TEST_F(AppsMiscTest, SysInfo) {
    if (FindAppsPath() == nullptr) {
        GTEST_SKIP() << "lxa-apps directory not found";
    }
    RunAppsMiscTest("SysInfo", 20000);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
