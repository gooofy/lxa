/**
 * graphics_gtest.cpp - Google Test suite for Graphics library functions
 */

#include "lxa_test.h"

using namespace lxa::testing;

class GraphicsTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    void RunGraphicsTest(const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/Graphics/" + std::string(name);
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

TEST_F(GraphicsTest, AllocRaster) { RunGraphicsTest("AllocRaster"); }
TEST_F(GraphicsTest, AreaEllipse) { RunGraphicsTest("AreaEllipse"); }
TEST_F(GraphicsTest, BltBitMap) { RunGraphicsTest("BltBitMap"); }
TEST_F(GraphicsTest, BltPattern) { RunGraphicsTest("BltPattern"); }
TEST_F(GraphicsTest, ClipBlit) { RunGraphicsTest("ClipBlit"); }
TEST_F(GraphicsTest, DrawEllipse) { RunGraphicsTest("DrawEllipse"); }
TEST_F(GraphicsTest, InitBitMap) { RunGraphicsTest("InitBitMap"); }
TEST_F(GraphicsTest, InitRastPort) { RunGraphicsTest("InitRastPort"); }
TEST_F(GraphicsTest, LayerClipping) { RunGraphicsTest("LayerClipping"); }
TEST_F(GraphicsTest, LineDraw) { RunGraphicsTest("LineDraw"); }
TEST_F(GraphicsTest, PenState) { RunGraphicsTest("PenState"); }
TEST_F(GraphicsTest, PixelOps) { RunGraphicsTest("PixelOps"); }
TEST_F(GraphicsTest, RectFill) { RunGraphicsTest("RectFill"); }
TEST_F(GraphicsTest, Regions) { RunGraphicsTest("Regions"); }
TEST_F(GraphicsTest, RPAttrs) { RunGraphicsTest("RPAttrs"); }
TEST_F(GraphicsTest, SetRast) { RunGraphicsTest("SetRast"); }
TEST_F(GraphicsTest, TextExtent) { RunGraphicsTest("TextExtent"); }
TEST_F(GraphicsTest, TextRender) { RunGraphicsTest("TextRender"); }

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
