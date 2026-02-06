/**
 * updatestrgad_gtest.cpp - Google Test version of UpdateStrGad test
 *
 * Tests string gadget updates via GT_SetGadgetAttrs.
 * The sample uses test_inject.h for self-testing.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class UpdateStrGadTest : public LxaUITest {
protected:
    std::string program_output;
    
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:UpdateStrGad", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Wait for program to complete its self-tests
        bool exited = lxa_wait_exit(10000);
        ASSERT_TRUE(exited) << "Program should complete self-tests and exit";
        
        // Store output for all tests
        program_output = GetOutput();
    }
};

TEST_F(UpdateStrGadTest, ProgrammaticUpdates) {
    EXPECT_NE(program_output.find("Interactive testing complete"), std::string::npos) 
        << "Expected test completion";
    EXPECT_NE(program_output.find("All updates completed successfully"), std::string::npos) 
        << "Expected successful updates";
}

TEST_F(UpdateStrGadTest, CompletesSuccessfully) {
    EXPECT_TRUE(!program_output.empty()) << "Expected output from string gadget updates";
    EXPECT_NE(program_output.find("Closing window"), std::string::npos) 
        << "Expected window close message";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
