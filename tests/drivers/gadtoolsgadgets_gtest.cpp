/**
 * gadtoolsgadgets_gtest.cpp - Google Test driver for GadToolsGadgets sample
 *
 * Tests GadTools gadget creation with STRING_KIND, SLIDER_KIND, and BUTTON_KIND.
 * Verifies that CreateGadgetA correctly allocates StringInfo for string gadgets.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class GadToolsGadgetsTest : public LxaTest {
protected:
    std::string output;

    void SetUp() override {
        LxaTest::SetUp();

        /* This sample auto-exits after setup — run to completion */
        int result = RunProgram("SYS:GadToolsGadgets", "", 10000);
        output = GetOutput();

        /* If the program failed, print output for debugging */
        if (result != 0) {
            SCOPED_TRACE("Program output:\n" + output);
        }
        ASSERT_EQ(result, 0) << "GadToolsGadgets should exit cleanly";
    }
};

TEST_F(GadToolsGadgetsTest, AllGadgetsCreated) {
    /* Verify context creation */
    EXPECT_NE(output.find("Context created at"), std::string::npos)
        << "CreateContext() should succeed";

    /* Verify slider gadget created */
    EXPECT_NE(output.find("Slider created at"), std::string::npos)
        << "SLIDER_KIND gadget should be created";

    /* Verify all 3 string gadgets created */
    EXPECT_NE(output.find("String 1 created at"), std::string::npos)
        << "STRING_KIND gadget 1 should be created";
    EXPECT_NE(output.find("String 2 created at"), std::string::npos)
        << "STRING_KIND gadget 2 should be created";
    EXPECT_NE(output.find("String 3 created at"), std::string::npos)
        << "STRING_KIND gadget 3 should be created";

    /* Verify button gadget created */
    EXPECT_NE(output.find("Button created at"), std::string::npos)
        << "BUTTON_KIND gadget should be created";

    /* Verify all gadgets reported as created successfully */
    EXPECT_NE(output.find("All gadgets created successfully"), std::string::npos)
        << "All gadgets should be created successfully";
}

TEST_F(GadToolsGadgetsTest, WindowOpened) {
    /* Verify window was opened */
    EXPECT_NE(output.find("Window opened at"), std::string::npos)
        << "Window should open successfully";

    /* Verify window size is reported */
    EXPECT_NE(output.find("Window size:"), std::string::npos)
        << "Window size should be reported";
}

TEST_F(GadToolsGadgetsTest, GadgetsRefreshed) {
    /* Verify GT_RefreshWindow was called */
    EXPECT_NE(output.find("Gadgets refreshed"), std::string::npos)
        << "GT_RefreshWindow should complete";
}

TEST_F(GadToolsGadgetsTest, SetGadgetAttrs) {
    /* Verify GT_SetGadgetAttrs was called to set slider level */
    EXPECT_NE(output.find("Slider level set to 15"), std::string::npos)
        << "GT_SetGadgetAttrs should set slider to level 15";
}

TEST_F(GadToolsGadgetsTest, StringGadgetsVerified) {
    /* Verify all 3 string gadgets are reported in verification */
    EXPECT_NE(output.find("String gadget 1 at"), std::string::npos)
        << "String gadget 1 should be verified";
    EXPECT_NE(output.find("String gadget 2 at"), std::string::npos)
        << "String gadget 2 should be verified";
    EXPECT_NE(output.find("String gadget 3 at"), std::string::npos)
        << "String gadget 3 should be verified";

    /* Verify that the string gadget addresses are non-zero (not NULL) */
    EXPECT_EQ(output.find("String gadget 1 at 0x00000000"), std::string::npos)
        << "String gadget 1 should not be NULL";
    EXPECT_EQ(output.find("String gadget 2 at 0x00000000"), std::string::npos)
        << "String gadget 2 should not be NULL";
    EXPECT_EQ(output.find("String gadget 3 at 0x00000000"), std::string::npos)
        << "String gadget 3 should not be NULL";
}

TEST_F(GadToolsGadgetsTest, CleanShutdown) {
    /* Verify clean shutdown sequence */
    EXPECT_NE(output.find("Window closed"), std::string::npos)
        << "Window should be closed";
    EXPECT_NE(output.find("Gadgets freed"), std::string::npos)
        << "Gadgets should be freed";
    EXPECT_NE(output.find("demo complete"), std::string::npos)
        << "Demo should complete successfully";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
