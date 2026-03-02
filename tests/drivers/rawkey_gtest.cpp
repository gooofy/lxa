/**
 * rawkey_gtest.cpp - Google Test version of RawKey test
 *
 * Tests raw keyboard input handling via IDCMP.
 * Verifies key down/up events, key-to-ASCII conversion, shift qualifier,
 * and space key code.
 */

#include "lxa_test.h"

using namespace lxa::testing;

/* Amiga raw key codes */
constexpr int RAWKEY_A = 0x20;
constexpr int RAWKEY_SPACE = 0x40;
constexpr int IEQUALIFIER_LSHIFT = 0x0001;

class RawKeyTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:RawKey", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 10000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        WaitForEventLoop(100, 10000);
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
    for (int i = 0; i < 50; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Key Down"), std::string::npos) 
        << "Expected key down event";
    EXPECT_NE(output.find("Key Up"), std::string::npos) 
        << "Expected key up event";
    /* The 'a' key should map to ASCII 'a' */
    EXPECT_TRUE(output.find("'a'") != std::string::npos ||
                output.find("= 'a'") != std::string::npos)
        << "Expected key converted to 'a'";
}

TEST_F(RawKeyTest, ShiftedKeyPress) {
    ClearOutput();
    
    PressKey(RAWKEY_A, IEQUALIFIER_LSHIFT);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 50; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_TRUE(output.find("LShift") != std::string::npos || 
                output.find("RShift") != std::string::npos ||
                output.find("Shift") != std::string::npos) 
        << "Expected shift qualifier detected";
    /* Shift+a should produce 'A' */
    EXPECT_TRUE(output.find("'A'") != std::string::npos ||
                output.find("= 'A'") != std::string::npos)
        << "Expected key converted to 'A'";
}

TEST_F(RawKeyTest, SpaceKey) {
    ClearOutput();
    
    PressKey(RAWKEY_SPACE, 0);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 50; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Key Down"), std::string::npos) 
        << "Expected space key down";
    /* Space is ASCII 32 = 0x20 */
    EXPECT_TRUE(output.find("code= 32") != std::string::npos ||
                output.find("($20)") != std::string::npos)
        << "Expected space key code (32/0x20)";
}

TEST_F(RawKeyTest, CloseGadgetExit) {
    ClearOutput();
    
    lxa_click_close_gadget(0);
    
    if (lxa_wait_exit(5000)) {
        std::string output = GetOutput();
        EXPECT_NE(output.find("Close gadget clicked"), std::string::npos)
            << "Expected close event recognized";
        EXPECT_NE(output.find("Window closed"), std::string::npos)
            << "Expected window closed cleanly";
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
