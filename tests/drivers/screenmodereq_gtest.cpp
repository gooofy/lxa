#include "lxa_test.h"

using namespace lxa::testing;

class ScreenModeReqTest : public LxaUITest {
protected:
    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:ScreenModeReq", ""), 0)
            << "Failed to load ScreenModeReq";
        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Screen mode requester did not open within 5 seconds";
        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";

        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(30, 50000);
    }
};

TEST_F(ScreenModeReqTest, RequesterOpens) {
    EXPECT_GE(window_info.width, 240);
    EXPECT_GE(window_info.height, 120);
}

TEST_F(ScreenModeReqTest, CancelButton) {
    int cancel_x = window_info.x + window_info.width - 8 - 60 + 30;
    int cancel_y = window_info.y + window_info.height - 20 - 14 + 7;

    Click(cancel_x, cancel_y);
    RunCyclesWithVBlank(20, 50000);

    bool exited = lxa_wait_exit(5000);
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }

    EXPECT_TRUE(exited);

    std::string output = GetOutput();
    EXPECT_NE(output.find("User cancelled"), std::string::npos)
        << output;
}

TEST_F(ScreenModeReqTest, OKButtonReturnsSelection) {
    int list_x = window_info.x + 24;
    int list_y = window_info.y + 36;
    int ok_x = window_info.x + 8 + 30;
    int ok_y = window_info.y + window_info.height - 20 - 14 + 7;

    Click(list_x, list_y);
    RunCyclesWithVBlank(10, 50000);

    Click(ok_x, ok_y);
    RunCyclesWithVBlank(20, 50000);

    bool exited = lxa_wait_exit(5000);
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }

    EXPECT_TRUE(exited);

    std::string output = GetOutput();
    EXPECT_NE(output.find("User selected mode"), std::string::npos)
        << output;
    EXPECT_NE(output.find("DisplayID=0x"), std::string::npos)
        << output;
    EXPECT_NE(output.find("Width="), std::string::npos)
        << output;
    EXPECT_NE(output.find("Height="), std::string::npos)
        << output;
    EXPECT_NE(output.find("Depth="), std::string::npos)
        << output;
    EXPECT_NE(output.find("BitMapWidth="), std::string::npos)
        << output;
    EXPECT_NE(output.find("BitMapHeight="), std::string::npos)
        << output;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
