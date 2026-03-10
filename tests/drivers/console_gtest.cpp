/**
 * console_gtest.cpp - Google Test suite for console.device
 */

#include "lxa_test.h"
#include <sys/time.h>

using namespace lxa::testing;

#define RAWKEY_A 0x20

class ConsoleTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
    }

    bool WaitForOutputContains(const char* marker, int timeout_ms = 5000) {
        struct timeval start, now;
        gettimeofday(&start, NULL);

        while (true) {
            RunCyclesWithVBlank(1);

            std::string output = GetOutput();
            if (output.find(marker) != std::string::npos) {
                return true;
            }

            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
    }

    void SlowTypeString(const char* str, int settle_vblanks = 3) {
        char ch[2] = {0, 0};

        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks);
        }
    }

    void RunConsoleTest(const char* name, int timeout_ms = 10000) {
        std::string path = "SYS:Tests/Console/" + std::string(name);
        int result = lxa_load_program(path.c_str(), "");
        ASSERT_EQ(result, 0) << "Failed to load program " << path;
        
        // Some tests need interaction
        if (strcmp(name, "kp2_test") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));

            ASSERT_TRUE(WaitForOutputContains("Injecting '1', '0', '0', Return", 8000));

            SlowTypeString("100\n");
            RunCyclesWithVBlank(20);
        } else if (strcmp(name, "input_inject") == 0 || strcmp(name, "input_console") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));

            const char* wait_marker = (strcmp(name, "input_inject") == 0)
                ? "Waiting for RAWKEY" : "Waiting for input";

            ASSERT_TRUE(WaitForOutputContains(wait_marker, 8000))
                << "Program did not print '" << wait_marker << "' within timeout";

            RunCyclesWithVBlank(10);

            if (strcmp(name, "input_inject") == 0) {
                SlowTypeString("ABC");
            } else {
                bool delivered = false;

                for (int attempt = 0; attempt < 3 && !delivered; ++attempt) {
                    SlowTypeString("A\n");
                    RunCyclesWithVBlank(20);
                    delivered = WaitForOutputContains("PASS", 3000);
                }
            }
            RunCyclesWithVBlank(20);
        } else if (strcmp(name, "con_handler") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));

            ASSERT_TRUE(WaitForOutputContains("Injecting 'test' + Return", 8000));

            RunCyclesWithVBlank(10);
            SlowTypeString("test\n");
            RunCyclesWithVBlank(30);
        }
        
        lxa_run_until_exit(timeout_ms);
        
        std::string output = GetOutput();
        if (output.find("FAIL") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_NE(output.find("PASS"), std::string::npos) << "Test " << name << " did not report success";
    }
};

TEST_F(ConsoleTest, CSICursor) { RunConsoleTest("csi_cursor"); }
TEST_F(ConsoleTest, CSIUnit) { RunConsoleTest("csi_unit"); }
TEST_F(ConsoleTest, SGRUnit) { RunConsoleTest("sgr_unit"); }
TEST_F(ConsoleTest, KeymapUnit) {
    int result = RunProgram("SYS:AskKeymap");
    EXPECT_EQ(result, 0);
}
TEST_F(ConsoleTest, ConHandler) { RunConsoleTest("con_handler"); }
TEST_F(ConsoleTest, KP2Test) { RunConsoleTest("kp2_test"); }
TEST_F(ConsoleTest, InputInject) { RunConsoleTest("input_inject"); }
TEST_F(ConsoleTest, InputConsole) { RunConsoleTest("input_console"); }

TEST_F(ConsoleTest, ConsoleAdvanced) {
    int result = RunProgram("SYS:Tests/ConsoleAdvanced/console_advanced");
    EXPECT_EQ(result, 0);
    std::string output = GetOutput();
    EXPECT_NE(output.find("PASS"), std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
