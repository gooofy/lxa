/**
 * layers_gtest.cpp - Google Test suite for layers.library
 */

#include "lxa_test.h"

using namespace lxa::testing;

class LayerTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    void RunLayerTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Layers/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        bool passed = (output.find("PASS") != std::string::npos) || 
                      (output.find("SUCCESS") != std::string::npos) ||
                      (output.find("All tests passed!") != std::string::npos) ||
                      (output.find("Done.") != std::string::npos);
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
    }
};

TEST_F(LayerTest, ClipRects) { RunLayerTest("ClipRects"); }
TEST_F(LayerTest, LayerInfo) { RunLayerTest("LayerInfo"); }
TEST_F(LayerTest, Locking) { RunLayerTest("Locking"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
