/**
 * determinism_gtest.cpp - Phase 201: deterministic virtual time
 *
 * liblxa runs on a virtual clock by default: VBlank, timer.device and
 * DateStamp are derived only from emulated cycles.  These tests prove that
 *   - the clock advances consistently for applications (VClock program),
 *   - the deterministic epoch is honoured,
 *   - two runs of the same interactive scenario are byte-identical
 *     (window capture, program output, cycle and frame counts, event log),
 *   - idle time is skipped rather than slept,
 *   - the wall-clock mode stays available as an opt-in.
 */

#include "lxa_test.h"

#include <fstream>
#include <iterator>
#include <sstream>

using namespace lxa::testing;

/* ------------------------------------------------------------------ */
/* Virtual clock as seen by an application                             */
/* ------------------------------------------------------------------ */

class VirtualClockTest : public LxaTest {
};

TEST_F(VirtualClockTest, IsDeterministicByDefault)
{
    EXPECT_TRUE(lxa_is_deterministic());
}

TEST_F(VirtualClockTest, ApplicationTimeSourcesAreConsistent)
{
    int rc = RunProgram("SYS:Tests/Devices/VClock", "", 20000);
    std::string out = GetOutput();
    EXPECT_EQ(rc, 0) << out;
    EXPECT_NE(out.find("PASS: virtual clock consistent"), std::string::npos) << out;
    EXPECT_EQ(out.find("FAIL"), std::string::npos) << out;
}

TEST_F(VirtualClockTest, DefaultEpochIs2024)
{
    /* 1978-01-01 .. 2024-01-01 = 46 years incl. 11 leap days */
    int rc = RunProgram("SYS:Tests/Devices/VClock", "", 20000);
    std::string out = GetOutput();
    EXPECT_EQ(rc, 0) << out;
    EXPECT_NE(out.find("INFO: boot day 16801\n"), std::string::npos) << out;
}

TEST_F(VirtualClockTest, IdleTimeIsSkipped)
{
    /* The VClock program sleeps ~2 s of emulated time in Delay()/timer
     * waits; almost all of that must be accounted as skipped idle time. */
    uint64_t c0 = lxa_get_emulated_cycles();
    uint64_t i0 = lxa_get_idle_cycles();
    ASSERT_EQ(RunProgram("SYS:Tests/Devices/VClock", "", 20000), 0) << GetOutput();
    uint64_t total = lxa_get_emulated_cycles() - c0;
    uint64_t idle  = lxa_get_idle_cycles() - i0;
    EXPECT_GT(total, 2ull * 25000000ull) << "expected >2 s of emulated time";
    EXPECT_GT(idle * 10, total * 8) << "expected >80% of the run to be idle-skipped";
}

TEST_F(VirtualClockTest, RunFramesAdvancesExactFrameCount)
{
    ASSERT_EQ(lxa_load_program("SYS:Tests/Devices/VClock", ""), 0);
    uint64_t f0 = lxa_get_frame_count();
    uint64_t c0 = lxa_get_emulated_cycles();
    lxa_run_frames(10);
    EXPECT_EQ(lxa_get_frame_count() - f0, 10u);
    /* 10 PAL frames of 500000 cycles (25 MHz); the CPU may overshoot a
     * boundary by the length of the last instruction */
    uint64_t d = lxa_get_emulated_cycles() - c0;
    EXPECT_GE(d, 9u * 500000u);
    EXPECT_LE(d, 10u * 500000u + 200u);
}

/* ------------------------------------------------------------------ */
/* Byte-identical replay                                               */
/* ------------------------------------------------------------------ */

struct ScenarioResult {
    std::string capture;
    std::string output;
    uint64_t cycles = 0;
    uint64_t frames = 0;
    std::string events;
};

class DeterminismReplayTest : public LxaTest {
protected:
    ScenarioResult RunScenario(const std::string &capture_path)
    {
        ScenarioResult r;

        EXPECT_EQ(lxa_load_program("SYS:SimpleGad", ""), 0);
        EXPECT_TRUE(lxa_wait_windows(1, 10000));
        lxa_run_frames(25);

        /* interact: click every application (non-system) gadget once */
        int n = lxa_get_gadget_count(0);
        for (int i = 0; i < n; i++) {
            lxa_gadget_info_t g;
            if (lxa_get_gadget_info(0, i, &g) && g.width > 0 && g.height > 0 &&
                !(g.gadget_type & 0x8000 /* GTYP_SYSGADGET */)) {
                lxa_inject_mouse_click(g.left + g.width / 2,
                                       g.top + g.height / 2, LXA_MOUSE_LEFT);
                lxa_run_frames(5);
            }
        }
        lxa_inject_string("abc");
        lxa_run_frames(25);

        EXPECT_TRUE(lxa_capture_window(0, capture_path.c_str()));
        std::ifstream f(capture_path, std::ios::binary);
        r.capture.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());

        r.output = GetOutput();
        r.cycles = lxa_get_emulated_cycles();
        r.frames = lxa_get_frame_count();

        lxa_intui_event_t ev[256];
        int ne = lxa_drain_intui_events(ev, 256);
        std::ostringstream es;
        for (int i = 0; i < ne; i++)
            es << ev[i].type << ":" << ev[i].window_index << ":" << ev[i].title << ";";
        r.events = es.str();
        return r;
    }
};

TEST_F(DeterminismReplayTest, SameScenarioTwiceIsByteIdentical)
{
    std::string p1 = ram_dir_path + "/run1.png";
    std::string p2 = ram_dir_path + "/run2.png";

    ScenarioResult a = RunScenario(p1);

    /* full emulator restart between the runs */
    std::string keep_ram = ram_dir_path;
    ram_dir_path.clear();           /* keep the RAM: dir across the restart */
    TearDown();
    SetUp();
    std::string p2_alt = keep_ram + "/run2.png";

    ScenarioResult b = RunScenario(p2_alt);

    ASSERT_FALSE(a.capture.empty());
    EXPECT_EQ(a.cycles, b.cycles);
    EXPECT_EQ(a.frames, b.frames);
    EXPECT_EQ(a.output, b.output);
    EXPECT_EQ(a.events, b.events);
    EXPECT_TRUE(a.capture == b.capture) << "window captures differ: " << p1 << " vs " << p2_alt;

    std::string cmd = "rm -rf " + keep_ram;
    system(cmd.c_str());
    (void)p2;
}

/* ------------------------------------------------------------------ */
/* Host stdin is detached from the emulated console                     */
/* ------------------------------------------------------------------ */

TEST_F(VirtualClockTest, ConsoleInputNeverBlocksOnHostStdin)
{
    /* Replace our stdin with a pipe whose write end stays open: a host
     * read() on it would block forever (this hung vim under ctest). */
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);
    int saved = dup(0);
    dup2(fds[0], 0);

    int rc = RunProgram("SYS:Tests/Dos/StdinEOF", "", 5000);
    std::string out = GetOutput();

    dup2(saved, 0);
    close(saved);
    close(fds[0]);
    close(fds[1]);

    EXPECT_EQ(rc, 0) << out;
    EXPECT_NE(out.find("PASS: stdin_eof"), std::string::npos) << out;
}

/* ------------------------------------------------------------------ */
/* Wall-clock opt-in                                                    */
/* ------------------------------------------------------------------ */

class RealtimeClockTest : public LxaTest {
protected:
    RealtimeClockTest() { config.realtime_clock = true; }
};

TEST_F(RealtimeClockTest, RealtimeClockIsOptIn)
{
    EXPECT_FALSE(lxa_is_deterministic());
    int rc = RunProgram("SYS:Tests/Devices/VClock", "", 20000);
    std::string out = GetOutput();
    EXPECT_EQ(rc, 0) << out;
    EXPECT_NE(out.find("PASS: virtual clock consistent"), std::string::npos) << out;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
