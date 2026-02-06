/**
 * cluster2_gtest.cpp - Google Test version of Cluster2 IDE test
 *
 * Tests Cluster2 Oberon-2 IDE.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class Cluster2Test : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Set up Cluster assign
        lxa_add_assign("Cluster", "/home/guenter/projects/amiga/lxa-apps/Cluster2");
        
        const char* cluster2_path = "/home/guenter/projects/amiga/lxa-apps/Cluster2/bin/Cluster2/Editor";
        
        ASSERT_EQ(lxa_load_program(cluster2_path, ""), 0) 
            << "Failed to load Cluster2";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "Cluster2 window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(Cluster2Test, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(Cluster2Test, RespondsToInput) {
    // Cluster2 should be responsive
    TypeString("MODULE Test;");
    RunCyclesWithVBlank(20, 50000);
    EXPECT_TRUE(lxa_is_running());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
