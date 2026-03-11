/**
 * devices_gtest.cpp - Google Test suite for AmigaOS Devices
 */

#include "lxa_test.h"

using namespace lxa::testing;

class DeviceTest : public LxaTest {
protected:
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

    void RunTrackdiskTest(int timeout_ms = 5000) {
        char df0_dir[] = "/tmp/lxa_test_DF0_XXXXXX";
        char df1_dir[] = "/tmp/lxa_test_DF1_XXXXXX";
        ASSERT_NE(mkdtemp(df0_dir), nullptr);
        ASSERT_NE(mkdtemp(df1_dir), nullptr);
        ASSERT_TRUE(lxa_add_drive("DF0", df0_dir));
        ASSERT_TRUE(lxa_add_drive("DF1", df1_dir));

        RunDeviceTest("Trackdisk", timeout_ms);

        std::string cleanup0 = std::string("rm -rf ") + df0_dir;
        std::string cleanup1 = std::string("rm -rf ") + df1_dir;
        system(cleanup0.c_str());
        system(cleanup1.c_str());
    }
};

TEST_F(DeviceTest, Clipboard) { RunDeviceTest("Clipboard"); }
TEST_F(DeviceTest, ConsoleAsync) { RunDeviceTest("ConsoleAsync"); }
TEST_F(DeviceTest, Input) { RunDeviceTest("Input"); }
TEST_F(DeviceTest, Timer) { RunDeviceTest("Timer"); }
TEST_F(DeviceTest, TimerAsync) { RunDeviceTest("TimerAsync"); }
TEST_F(DeviceTest, Trackdisk) { RunTrackdiskTest(); }
TEST_F(DeviceTest, Audio) { RunDeviceTest("Audio"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
