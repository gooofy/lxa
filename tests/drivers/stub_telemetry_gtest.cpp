/**
 * stub_telemetry_gtest.cpp - Phase 203: stub telemetry
 *
 * Every stub, partial implementation and empty LVO slot reports through
 * LXA_UNIMPLEMENTED.  These tests prove that
 *   - calls are recorded with library, function and hit count,
 *   - empty exec.library vectors are identified by LVO,
 *   - strict mode stops the run at the first call (exit code 125),
 *   - private/reserved slots no longer halt the emulator.
 */

#include "lxa_test.h"

#include <algorithm>

using namespace lxa::testing;

static const lxa_unimplemented_t *FindEntry(const std::vector<lxa_unimplemented_t> &log,
                                            const char *lib, const char *fn)
{
    for (const auto &e : log)
        if (strcmp(e.lib, lib) == 0 && strcmp(e.function, fn) == 0)
            return &e;
    return nullptr;
}

static std::vector<lxa_unimplemented_t> GetLog()
{
    int n = lxa_get_unimplemented_log(nullptr, 0);
    std::vector<lxa_unimplemented_t> v(n > 0 ? n : 0);
    if (n > 0)
        lxa_get_unimplemented_log(v.data(), n);
    return v;
}

class StubTelemetryTest : public LxaTest {
};

TEST_F(StubTelemetryTest, CleanProgramRecordsNothing)
{
    ASSERT_EQ(RunProgram("SYS:Tests/Dos/HelloWorld", "", 5000), 0);
    EXPECT_EQ(lxa_get_unimplemented_log(nullptr, 0), 0);
}

TEST_F(StubTelemetryTest, StubAndEmptyVectorAreRecorded)
{
    ASSERT_EQ(RunProgram("SYS:Tests/Exec/StubProbe", "", 5000), 0) << GetOutput();
    std::string out = GetOutput();
    EXPECT_NE(out.find("stub_probe: done"), std::string::npos) << out;

    auto log = GetLog();
    const lxa_unimplemented_t *cx = FindEntry(log, "commodities", "CreateCxObj");
    ASSERT_NE(cx, nullptr) << "CreateCxObj stub not recorded";
    EXPECT_EQ(cx->count, 1);
    EXPECT_EQ(strncmp(cx->detail, "stub:", 5), 0) << cx->detail;
    EXPECT_STREQ(cx->first_task, "exec bootstrap");

    const lxa_unimplemented_t *ev = FindEntry(log, "exec", "LVO -786");
    ASSERT_NE(ev, nullptr) << "empty exec vector (ObtainQuickVector) not recorded";
    EXPECT_EQ(ev->count, 1);
}

TEST_F(StubTelemetryTest, ClearResetsLog)
{
    ASSERT_EQ(RunProgram("SYS:Tests/Exec/StubProbe", "", 5000), 0);
    ASSERT_GT(lxa_get_unimplemented_log(nullptr, 0), 0);
    lxa_clear_unimplemented_log();
    EXPECT_EQ(lxa_get_unimplemented_log(nullptr, 0), 0);
}

class StrictUnimplementedTest : public LxaTest {
protected:
    StrictUnimplementedTest() { config.strict_unimplemented = true; }
};

TEST_F(StrictUnimplementedTest, StrictModeStopsAtFirstStub)
{
    int rc = RunProgram("SYS:Tests/Exec/StubProbe", "", 5000);
    std::string out = GetOutput();

    EXPECT_EQ(rc, LXA_EXIT_UNIMPLEMENTED) << out;
    EXPECT_NE(out.find("stub_probe: start"), std::string::npos) << out;
    EXPECT_EQ(out.find("after CreateCxObj"), std::string::npos)
        << "strict mode must stop before the stub returns\n" << out;
    EXPECT_FALSE(lxa_is_running());

    auto log = GetLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_STREQ(log[0].function, "CreateCxObj");
}

TEST_F(StrictUnimplementedTest, StrictModeIgnoresCleanPrograms)
{
    EXPECT_EQ(RunProgram("SYS:Tests/Dos/HelloWorld", "", 5000), 0);
    EXPECT_NE(GetOutput().find("Hello"), std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
