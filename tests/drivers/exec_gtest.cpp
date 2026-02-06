/**
 * exec_gtest.cpp - Google Test suite for Exec library functions
 *
 * This suite ports legacy exec tests to the new Google Test infrastructure.
 * Each test runs an m68k test program and verifies its output.
 */

#include "lxa_test.h"
#include <regex>

using namespace lxa::testing;

class ExecTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    /**
     * Helper to run an exec test binary and check for PASS/FAIL
     */
    void RunExecTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Exec/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        // Output result for debugging if test fails
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        EXPECT_NE(output.find("PASS:"), std::string::npos) << "Test " << name << " did not report PASS";
        EXPECT_EQ(output.find("FAIL:"), std::string::npos) << "Test " << name << " reported one or more FAILURES";
    }
};

TEST_F(ExecTest, Lists) {
    RunExecTest("Lists");
}

TEST_F(ExecTest, Memory) {
    RunExecTest("Memory");
}

TEST_F(ExecTest, MsgPort) {
    RunExecTest("MsgPort");
}

TEST_F(ExecTest, Multitask) {
    // Multitask test waits for 0.5s and does some work, give it more time
    RunExecTest("Multitask", 10000);
}

TEST_F(ExecTest, RawDoFmt) {
    RunExecTest("RawDoFmt");
}

TEST_F(ExecTest, Resources) {
    RunExecTest("Resources");
}

TEST_F(ExecTest, SignalPingPong) {
    RunExecTest("SignalPingPong");
}

TEST_F(ExecTest, Signals) {
    RunExecTest("Signals");
}

TEST_F(ExecTest, Sync) {
    RunExecTest("Sync");
}

TEST_F(ExecTest, Tasks) {
    RunExecTest("Tasks");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
