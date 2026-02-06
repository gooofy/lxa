/**
 * samples_gtest.cpp - Google Test suite for RKM Samples
 *
 * This suite ports remaining RKM samples to the new Google Test infrastructure.
 * Each test runs an m68k sample program and verifies its basic execution.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class SampleTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    void RunSample(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Sample " << name << " exited with non-zero code";
    }
};

// Note: Many samples are interactive or wait for user input.
// These are standard "run and exit" samples or those that can be tested non-interactively.

TEST_F(SampleTest, A2D) { RunSample("A2D"); }
TEST_F(SampleTest, AllocEntry) { RunSample("AllocEntry"); }
TEST_F(SampleTest, AskKeymap) { RunSample("AskKeymap"); }
TEST_F(SampleTest, AvailFonts) { RunSample("AvailFonts"); }
TEST_F(SampleTest, BuildList) { RunSample("BuildList"); }
TEST_F(SampleTest, DeviceUse) { RunSample("DeviceUse"); }
TEST_F(SampleTest, FFPExample) { RunSample("FFPExample"); }
TEST_F(SampleTest, FFPTrans) { RunSample("FFPTrans"); }
TEST_F(SampleTest, FindBoards) { RunSample("FindBoards"); }
TEST_F(SampleTest, Hooks1) { RunSample("Hooks1"); }
TEST_F(SampleTest, IEEEDPExample) { RunSample("IEEEDPExample"); }
TEST_F(SampleTest, IStr) { RunSample("IStr"); }
TEST_F(SampleTest, MeasureText) { RunSample("MeasureText"); }
TEST_F(SampleTest, Semaphore) { RunSample("Semaphore"); }
TEST_F(SampleTest, Signals) { RunSample("Signals"); }
TEST_F(SampleTest, SimpleTask) { RunSample("SimpleTask"); }
TEST_F(SampleTest, Tag1) { RunSample("Tag1"); }
TEST_F(SampleTest, TaskList) { RunSample("TaskList"); }
TEST_F(SampleTest, TimerSoftInt) { RunSample("TimerSoftInt"); }
TEST_F(SampleTest, Uptime) { RunSample("Uptime"); }
TEST_F(SampleTest, WBClone) { RunSample("WBClone"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
