/**
 * commands_gtest.cpp - Google Test suite for DOS Commands
 *
 * This suite ports legacy command tests to the new Google Test infrastructure.
 * Each test runs an m68k test program that exercises a specific DOS command.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class CommandTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    /**
     * Helper to run a command test binary and check for PASS/FAIL
     */
    void RunCommandTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Commands/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        // Output result for debugging if test fails
        if (result != 0 || output.find("FAIL:") != std::string::npos || output.find("FAIL") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        // Check for various success markers
        bool passed = (output.find("PASS:") != std::string::npos) || 
                      (output.find("PASS\n") != std::string::npos) ||
                      (output.find("All tests passed!") != std::string::npos) ||
                      (output.find("SUCCESS") != std::string::npos) ||
                      (output.find("Done.") != std::string::npos);
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
        EXPECT_EQ(output.find("FAIL:"), std::string::npos) << "Test " << name << " reported one or more FAILURES";
    }
};

TEST_F(CommandTest, Assign) {
    RunCommandTest("Assign");
}

TEST_F(CommandTest, Avail) {
    RunCommandTest("Avail");
}

TEST_F(CommandTest, Break) {
    RunCommandTest("Break");
}

TEST_F(CommandTest, Copy) {
    RunCommandTest("Copy");
}

TEST_F(CommandTest, Delete) {
    RunCommandTest("Delete");
}

TEST_F(CommandTest, Dir) {
    RunCommandTest("Dir");
}

TEST_F(CommandTest, Eval) {
    RunCommandTest("Eval");
}

TEST_F(CommandTest, FileNote) {
    RunCommandTest("FileNote");
}

TEST_F(CommandTest, Join) {
    RunCommandTest("Join");
}

TEST_F(CommandTest, List) {
    RunCommandTest("List");
}

TEST_F(CommandTest, MakeDir) {
    RunCommandTest("MakeDir");
}

TEST_F(CommandTest, Protect) {
    RunCommandTest("Protect");
}

TEST_F(CommandTest, Rename) {
    RunCommandTest("Rename");
}

TEST_F(CommandTest, Run) {
    RunCommandTest("run_test");
}

TEST_F(CommandTest, Search) {
    RunCommandTest("Search");
}

TEST_F(CommandTest, SetEnv) {
    RunCommandTest("SetEnv");
}

TEST_F(CommandTest, Sort) {
    RunCommandTest("Sort");
}

TEST_F(CommandTest, Status) {
    RunCommandTest("Status");
}

TEST_F(CommandTest, SysCmd) {
    RunCommandTest("SysCmd");
}

TEST_F(CommandTest, Type) {
    RunCommandTest("Type");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
