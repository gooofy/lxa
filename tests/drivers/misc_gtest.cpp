/**
 * misc_gtest.cpp - Google Test suite for miscellaneous libraries and stress tests
 *
 * Covers Icon, IffParse, DataTypes and Stress tests.
 */

#include "lxa_test.h"
#include <fstream>

using namespace lxa::testing;

class MiscTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }

    void RunMiscTest(const char* category, const char* name, int timeout_ms = 5000) {
        std::string path = "SYS:Tests/" + std::string(category) + "/" + std::string(name);
        int result = RunProgram(path.c_str(), "", timeout_ms);
        
        std::string output = GetOutput();
        
        if (result != 0 || output.find("FAIL") != std::string::npos) {
            SCOPED_TRACE("Test Output:\n" + output);
        }

        EXPECT_EQ(result, 0) << "Test " << name << " exited with non-zero code";
        
        bool passed = (output.find("PASS") != std::string::npos) || 
                      (output.find("Success") != std::string::npos) ||
                      (output.find("complete") != std::string::npos) ||
                      (output.find("done") != std::string::npos);
        
        EXPECT_TRUE(passed) << "Test " << name << " did not report success";
    }
};

TEST_F(MiscTest, IconToolType) {
    RunMiscTest("Icon", "ToolType");
}

TEST_F(MiscTest, IffParseBasic) {
    // We need to provide the test.iff file in T:
    std::string src_path = std::string(FindSamplesPath()) + "/Tests/IffParse/T:test.iff";
    std::string dst_path = t_dir_path + "/test.iff";
    
    // Copy the file manually to the T: directory created by LxaTest
    std::ifstream src(src_path, std::ios::binary);
    std::ofstream dst(dst_path, std::ios::binary);
    dst << src.rdbuf();
    
    RunMiscTest("IffParse", "Basic");
}

TEST_F(MiscTest, DataTypesBasic) {
    RunMiscTest("DataTypes", "Basic");
}

TEST_F(MiscTest, StressFilesystem) {
    RunMiscTest("Stress", "Filesystem", 30000);
}

TEST_F(MiscTest, StressMemory) {
    RunMiscTest("Stress", "Memory", 30000);
}

TEST_F(MiscTest, StressTasks) {
    RunMiscTest("Stress", "Tasks", 30000);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
