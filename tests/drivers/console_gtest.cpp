/**
 * console_gtest.cpp - Google Test suite for console.device
 */

#include "lxa_test.h"
#include <sys/time.h>

using namespace lxa::testing;

class ConsoleTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
    }

    void RunConsoleTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Console/" + std::string(name);
        int result = lxa_load_program(path.c_str(), "");
        ASSERT_EQ(result, 0) << "Failed to load program " << path;
        
        // Some tests need interaction
        if (strcmp(name, "kp2_test") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            
            // Wait for prompt
            char buffer[16384];
            while (true) {
                lxa_run_cycles(10000);
                lxa_get_output(buffer, sizeof(buffer));
                if (strstr(buffer, "Injecting '1', '0', '0', Return")) break;
            }
            
            // Type "100" and Return
            TypeString("100\n");
            RunCyclesWithVBlank(20);
        } else if (strcmp(name, "input_inject") == 0 || strcmp(name, "input_console") == 0) {
            ASSERT_TRUE(WaitForWindows(1, 5000));
            
            /* Wait for the program to print its "Waiting for..." message, which means
             * the window is set up with IDCMP_RAWKEY and the program is ready for input.
             * input_inject prints: "Waiting for RAWKEY events..."
             * input_console prints: "Waiting for input..." */
            const char* wait_marker = (strcmp(name, "input_inject") == 0)
                ? "Waiting for RAWKEY" : "Waiting for input";
            {
                char buffer[16384];
                struct timeval start, now;
                gettimeofday(&start, NULL);
                bool ready = false;
                while (!ready) {
                    RunCyclesWithVBlank(1);
                    lxa_get_output(buffer, sizeof(buffer));
                    if (strstr(buffer, wait_marker)) {
                        ready = true;
                        break;
                    }
                    gettimeofday(&now, NULL);
                    long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                                  (now.tv_usec - start.tv_usec) / 1000;
                    if (elapsed >= 5000) break;
                }
                ASSERT_TRUE(ready) << "Program did not print '" << wait_marker << "' within timeout";
            }
            
            /* Give extra cycles for the program to enter its Wait() / DoIO(CMD_READ) */
            RunCyclesWithVBlank(5);
            
            TypeString("ABC\n");
            RunCyclesWithVBlank(20);
        } else if (strcmp(name, "con_handler") == 0) {
            // con_handler opens a CON: window and does Read() which blocks.
            // We need to wait for the CON: window to open, then inject input.
            // Wait for 2 windows: the first is the standard screen, the CON: is additional.
            ASSERT_TRUE(WaitForWindows(1, 5000));
            RunCyclesWithVBlank(20);
            TypeString("test\n");
            RunCyclesWithVBlank(20);
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
