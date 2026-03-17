/**
 * devices_gtest.cpp - Google Test suite for AmigaOS Devices
 */

#include "lxa_test.h"

using namespace lxa::testing;

#define RAWKEY_A      0x20
#define RAWKEY_LSHIFT 0x60
#define RESET_WARNING 0x78
#define QUAL_LSHIFT   0x0001

class DeviceTest : public LxaTest {
protected:
    bool WaitForOutputContains(const char* marker, int timeout_ms = 5000) {
        int elapsed = 0;

        while (elapsed < timeout_ms) {
            RunCyclesWithVBlank(1);
            if (GetOutput().find(marker) != std::string::npos) {
                return true;
            }
            elapsed += 16;
        }

        return false;
    }

    void RunKeyboardDeviceTest(int timeout_ms = 10000) {
        int result = lxa_load_program("SYS:Tests/Devices/Keyboard", "");
        ASSERT_EQ(result, 0) << "Failed to load keyboard device test";

        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(WaitForOutputContains("Waiting for SHIFT key...", 5000));

        ASSERT_TRUE(lxa_inject_key(RAWKEY_LSHIFT, QUAL_LSHIFT, true));
        RunCyclesWithVBlank(10);

        ASSERT_TRUE(WaitForOutputContains("Waiting for keypad 1 and A...", 5000));

        ASSERT_TRUE(lxa_inject_key(0x1d, QUAL_LSHIFT, true));
        RunCyclesWithVBlank(10);

        ASSERT_TRUE(lxa_inject_key(RAWKEY_A, QUAL_LSHIFT, true));
        RunCyclesWithVBlank(10);

        ASSERT_TRUE(WaitForOutputContains("Waiting for reset-warning rawkey...", 5000));

        ASSERT_TRUE(lxa_inject_key(RESET_WARNING, QUAL_LSHIFT, true));
        RunCyclesWithVBlank(20);

        result = lxa_run_until_exit(timeout_ms);

        std::string output = GetOutput();
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test Keyboard exited with non-zero code";
        EXPECT_NE(output.find("PASS: keyboard.device tests passed"), std::string::npos)
            << "Keyboard device test did not report success";
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
TEST_F(DeviceTest, Gameport) { RunDeviceTest("Gameport"); }
TEST_F(DeviceTest, Input) { RunDeviceTest("Input"); }
TEST_F(DeviceTest, Keyboard) { RunKeyboardDeviceTest(); }
TEST_F(DeviceTest, Narrator) { RunDeviceTest("Narrator"); }
TEST_F(DeviceTest, Parallel) { RunDeviceTest("Parallel"); }
TEST_F(DeviceTest, Printer) { RunDeviceTest("Printer"); }
TEST_F(DeviceTest, Ramdrive) { RunDeviceTest("Ramdrive"); }
TEST_F(DeviceTest, Scsi) { RunDeviceTest("Scsi"); }
TEST_F(DeviceTest, Serial) { RunDeviceTest("Serial"); }
TEST_F(DeviceTest, Timer) { RunDeviceTest("Timer"); }
TEST_F(DeviceTest, TimerAsync) { RunDeviceTest("TimerAsync"); }
TEST_F(DeviceTest, Trackdisk) { RunTrackdiskTest(); }
TEST_F(DeviceTest, Audio) { RunDeviceTest("Audio"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
