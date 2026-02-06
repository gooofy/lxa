/**
 * devices_gtest.cpp - Google Test suite for AmigaOS Devices
 */

#include "lxa_test.h"

using namespace lxa::testing;

class DeviceTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    void RunDeviceTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Devices/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        bool passed = (output.find("PASS") != std::string::npos) || 
                      (output.find("SUCCESS") != std::string::npos) ||
                      (output.find("All tests passed!") != std::string::npos) ||
                      (output.find("All Tests Completed") != std::string::npos) ||
                      (output.find("Done.") != std::string::npos);
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
    }
};

TEST_F(DeviceTest, Clipboard) { RunDeviceTest("Clipboard"); }
TEST_F(DeviceTest, ConsoleAsync) { RunDeviceTest("ConsoleAsync"); }
TEST_F(DeviceTest, Timer) { RunDeviceTest("Timer"); }
TEST_F(DeviceTest, TimerAsync) { RunDeviceTest("TimerAsync"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
