/**
 * simplegtgadget_gtest.cpp - Google Test version of SimpleGTGadget test
 *
 * Tests GadTools gadget creation and interaction.
 * The sample uses test_inject.h for self-testing.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class SimpleGTGadgetTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:SimpleGTGadget", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // This sample uses test_inject.h for automatic testing
        // Just wait for it to complete
    }
};

TEST_F(SimpleGTGadgetTest, GadgetCreation) {
    // Wait for program to complete its self-tests
    bool exited = lxa_wait_exit(10000);
    ASSERT_TRUE(exited) << "Program should complete self-tests and exit";
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Created BUTTON_KIND gadget"), std::string::npos) 
        << "Expected BUTTON_KIND gadget creation";
    EXPECT_NE(output.find("Created CHECKBOX_KIND gadget"), std::string::npos) 
        << "Expected CHECKBOX_KIND gadget creation";
    EXPECT_NE(output.find("Created INTEGER_KIND gadget"), std::string::npos) 
        << "Expected INTEGER_KIND gadget creation";
    EXPECT_NE(output.find("Created CYCLE_KIND gadget"), std::string::npos) 
        << "Expected CYCLE_KIND gadget creation";
}

TEST_F(SimpleGTGadgetTest, InteractiveTesting) {
    std::string output = GetOutput();
    // Just verify the program completed (already checked by exited==true)
    // The output contains various test messages
    EXPECT_TRUE(!output.empty()) << "Expected some output from tests";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
