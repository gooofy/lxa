/**
 * shell_gtest.cpp - Google Test suite for Shell and scripting
 *
 * This suite ports legacy shell tests to the new Google Test infrastructure.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class ShellTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    /**
     * Helper to run a shell test binary and check for success
     */
    void RunShellTest(const char* name, int timeout_ms = 10000) {
        std::string path = "SYS:Tests/Shell/" + std::string(name) + "/Test";
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        // For shell tests, we check if the script completed as expected
        if (std::string(name) == "Script") {
            EXPECT_NE(output.find("Hello from script"), std::string::npos);
            EXPECT_NE(output.find("Second line"), std::string::npos);
            EXPECT_NE(output.find("Script complete"), std::string::npos);
        } else if (std::string(name) == "Alias") {
            EXPECT_NE(output.find("ALIAS: dir not found"), std::string::npos);
            EXPECT_NE(output.find("ll         DIR"), std::string::npos);
        } else if (std::string(name) == "ControlFlow") {
            EXPECT_NE(output.find("Starting test"), std::string::npos);
            EXPECT_NE(output.find("First IF failed"), std::string::npos);
            EXPECT_NE(output.find("Second IF failed (this should show)"), std::string::npos);
            EXPECT_NE(output.find("Done"), std::string::npos);
        } else if (std::string(name) == "Variables") {
            EXPECT_NE(output.find("Testing Shell Variables"), std::string::npos);
            EXPECT_NE(output.find("GlobalValue"), std::string::npos);
            EXPECT_NE(output.find("Variable tests done"), std::string::npos);
        }
    }
};

TEST_F(ShellTest, Script) {
    RunShellTest("Script");
}

TEST_F(ShellTest, Alias) {
    RunShellTest("Alias");
}

TEST_F(ShellTest, ControlFlow) {
    RunShellTest("ControlFlow");
}

TEST_F(ShellTest, Variables) {
    RunShellTest("Variables");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
