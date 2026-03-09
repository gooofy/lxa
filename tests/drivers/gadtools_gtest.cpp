#include "lxa_test.h"

using namespace lxa::testing;

class GadToolsTest : public LxaTest {
protected:
    void RunGadToolsTest(const char *name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/GadTools/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        std::string output = GetOutput();

        if (result != 0 || output.find("FAIL:") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        EXPECT_NE(output.find("PASS:"), std::string::npos)
            << "Test " << name << " did not report success";
    }
};

TEST_F(GadToolsTest, GadgetAttrs) {
    RunGadToolsTest("GadgetAttrs");
}

TEST_F(GadToolsTest, ContextRefresh) {
    RunGadToolsTest("ContextRefresh");
}

TEST_F(GadToolsTest, DrawBevelBox) {
    RunGadToolsTest("DrawBevelBox");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
