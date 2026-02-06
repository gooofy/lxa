/**
 * rawkey_gtest.cpp - Google Test version of RawKey test
 *
 * Tests raw keyboard input handling via IDCMP.
 */

#include "lxa_test.h"

using namespace lxa::testing;

// Amiga raw key codes
constexpr int RAWKEY_A = 0x20;
constexpr int RAWKEY_SPACE = 0x40;
constexpr int IEQUALIFIER_LSHIFT = 0x0001;

class RawKeyTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:RawKey", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(RawKeyTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(RawKeyTest, SimpleKeyPress) {
    PressKey(RAWKEY_A, 0);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Key Down"), std::string::npos) 
        << "Expected key down event";
    EXPECT_NE(output.find("Key Up"), std::string::npos) 
        << "Expected key up event";
}

TEST_F(RawKeyTest, ShiftedKeyPress) {
    ClearOutput();
    
    PressKey(RAWKEY_A, IEQUALIFIER_LSHIFT);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_TRUE(output.find("Shift") != std::string::npos || 
                output.find("'A'") != std::string::npos) 
        << "Expected shifted key event";
}

TEST_F(RawKeyTest, SpaceKey) {
    ClearOutput();
    
    PressKey(RAWKEY_SPACE, 0);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Key Down"), std::string::npos) 
        << "Expected space key down";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
