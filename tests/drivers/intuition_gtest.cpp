/**
 * intuition_gtest.cpp - Google Test suite for Intuition library functions
 */

#include "lxa_test.h"

using namespace lxa::testing;

class IntuitionTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    void RunIntuitionTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Intuition/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        bool passed = (output.find("PASS") != std::string::npos) || 
                      (output.find("SUCCESS") != std::string::npos) ||
                      (output.find("All tests passed!") != std::string::npos) ||
                      (output.find("All gadget class tests passed!") != std::string::npos) ||
                      (output.find("All Tests Completed") != std::string::npos) ||
                      (output.find("Done.") != std::string::npos);
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
    }
};

TEST_F(IntuitionTest, BOOPSI) { RunIntuitionTest("BOOPSI"); }
TEST_F(IntuitionTest, GadgetClick) { RunIntuitionTest("GadgetClick"); }
TEST_F(IntuitionTest, GadgetRefresh) { RunIntuitionTest("GadgetRefresh"); }
TEST_F(IntuitionTest, GadgetClass) { RunIntuitionTest("GadgetClass"); }
TEST_F(IntuitionTest, IDCMP) { RunIntuitionTest("IDCMP"); }
TEST_F(IntuitionTest, IDCMPInput) { RunIntuitionTest("IDCMPInput"); }
TEST_F(IntuitionTest, MenuSelect) { RunIntuitionTest("MenuSelect"); }
TEST_F(IntuitionTest, MenuStrip) { RunIntuitionTest("MenuStrip"); }
TEST_F(IntuitionTest, RequesterBasic) { RunIntuitionTest("RequesterBasic"); }
TEST_F(IntuitionTest, ScreenBasic) { RunIntuitionTest("ScreenBasic"); }
TEST_F(IntuitionTest, ScreenManipulation) { RunIntuitionTest("ScreenManipulation"); }
TEST_F(IntuitionTest, Validation) { RunIntuitionTest("Validation"); }
TEST_F(IntuitionTest, WindowBasic) { RunIntuitionTest("WindowBasic"); }
TEST_F(IntuitionTest, WindowManipulation) { RunIntuitionTest("WindowManipulation"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
