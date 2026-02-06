/**
 * dos_gtest.cpp - Google Test suite for DOS library functions
 *
 * This suite ports legacy dos tests to the new Google Test infrastructure.
 * Each test runs an m68k test program and verifies its output.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class DosTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    /**
     * Helper to run a dos test binary and check for PASS/FAIL
     */
    void RunDosTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Dos/" + std::string(name);
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
                      (output.find("All Tests Completed") != std::string::npos) ||
                      (output.find("All character I/O tests completed") != std::string::npos) ||
                      (output.find("All VPrintf tests completed") != std::string::npos) ||
                      (output.find("Done.") != std::string::npos) ||
                      (output.find("Failed: 0") != std::string::npos) ||
                      (output.find("Hello, World!") != std::string::npos && std::string(name) == "HelloWorld");
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
        EXPECT_EQ(output.find("FAIL:"), std::string::npos) << "Test " << name << " reported one or more FAILURES";
    }
};

TEST_F(DosTest, CharIO) {
    RunDosTest("CharIO");
}

TEST_F(DosTest, CreateDir) {
    RunDosTest("CreateDir");
}

TEST_F(DosTest, FHUtils) {
    RunDosTest("FHUtils");
}

TEST_F(DosTest, FileIO) {
    RunDosTest("FileIO");
}

TEST_F(DosTest, HelloWorld) {
    RunDosTest("HelloWorld");
}

TEST_F(DosTest, LockShared) {
    RunDosTest("LockShared");
}

TEST_F(DosTest, MatchFirst) {
    RunDosTest("MatchFirst");
}

TEST_F(DosTest, PathNavigation) {
    RunDosTest("PathNavigation");
}

TEST_F(DosTest, PathUtils) {
    RunDosTest("PathUtils");
}

TEST_F(DosTest, Patterns) {
    RunDosTest("Patterns");
}

TEST_F(DosTest, ReadArgsEmpty) {
    RunDosTest("ReadArgsEmpty");
}

TEST_F(DosTest, ReadArgsFull) {
    // ReadArgsFull might take a bit longer as it tests many combinations
    RunDosTest("ReadArgsFull", 10000);
}

TEST_F(DosTest, ProcessAdvanced) {
    // ProcessAdvanced runs multiple commands, give it more time
    RunDosTest("ProcessAdvanced", 90000);
}

TEST_F(DosTest, VPrintf) {
    RunDosTest("VPrintf");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
