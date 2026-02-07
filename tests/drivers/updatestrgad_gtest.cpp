/**
 * updatestrgad_gtest.cpp - Google Test version of UpdateStrGad test
 *
 * Tests string gadget interaction.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class UpdateStrGadTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:UpdateStrGad", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        RunCyclesWithVBlank(10);
    }
};

TEST_F(UpdateStrGadTest, TypeIntoGadget) {
    // String gadget is at (20, 20) in the window
    // Buffer starts with "START" (initialized in sample code)
    int clickX = window_info.x + 20 + (200 / 2);
    int clickY = window_info.y + 20 + (8 / 2);
    
    // Click to activate - cursor goes to end of existing text ("START")
    Click(clickX, clickY);
    RunCyclesWithVBlank(10);
    
    // Type "Hello" and Return - appends to existing "START" text
    ClearOutput();
    TypeString("Hello\n");
    RunCyclesWithVBlank(50);
    
    // Need additional cycles for the m68k program to wake up, GetMsg(), and printf()
    RunCyclesWithVBlank(30);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETUP: string is 'STARTHello'"), std::string::npos)
        << "Expected GADGETUP with typed string 'STARTHello' (START from initial buffer + Hello typed)";
}

TEST_F(UpdateStrGadTest, CloseWindow) {
    // Click close gadget
    Click(window_info.x + 5, window_info.y + 5);
    RunCyclesWithVBlank(20);
    
    // Program should exit now
    EXPECT_TRUE(lxa_wait_exit(2000)) << "Program should exit after closing window";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
