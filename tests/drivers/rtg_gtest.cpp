/**
 * rtg_gtest.cpp - RTG screens on the host display (Phase 241)
 *
 * Runs Tests/Graphics/RtgScreen with RTG enabled (lxa_config_t.rtg, the
 * `rtg` reference profile): a Picasso96 screen of a chunky format is opened,
 * filled with p96RectFill colours and one graphics.library pen.  The host
 * display (display_update_rtg) must show those colours, at the screen's
 * size, and the screen must be capturable.  Pixel values inside the
 * emulated bitmaps are covered by the reference probes in tests/probes/rtg.
 */

#include "lxa_test.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace lxa::testing;

class RtgScreenTest : public LxaTest {
protected:
    RtgScreenTest() { config.rtg = true; }

    /* Start the program and run until it reports READY. */
    void StartScreen(const char *mode) {
        ASSERT_EQ(lxa_load_program("SYS:Tests/Graphics/RtgScreen", mode), 0);
        for (int i = 0; i < 200 && lxa_is_running(); i++) {
            RunFrames(5);
            if (GetOutput().find("READY") != std::string::npos)
                break;
        }
        ASSERT_NE(GetOutput().find("READY"), std::string::npos) << GetOutput();
        lxa_flush_display();
    }

    void ExpectRGB(int x, int y, int r, int g, int b, int tol) {
        uint8_t pr = 0, pg = 0, pb = 0;
        ASSERT_TRUE(lxa_read_pixel_rgb(x, y, &pr, &pg, &pb)) << "at " << x << "," << y;
        EXPECT_NEAR(pr, r, tol) << "red at " << x << "," << y;
        EXPECT_NEAR(pg, g, tol) << "green at " << x << "," << y;
        EXPECT_NEAR(pb, b, tol) << "blue at " << x << "," << y;
    }

    void Finish() {
        EXPECT_EQ(lxa_run_until_exit(20000), 0);
        EXPECT_NE(GetOutput().find("PASS"), std::string::npos) << GetOutput();
    }

    void CheckColours(int tol) {
        ExpectRGB(60, 60, 0xff, 0x00, 0x00, tol);
        ExpectRGB(170, 60, 0x00, 0xff, 0x00, tol);
        ExpectRGB(280, 60, 0x00, 0x00, 0xff, tol);
        ExpectRGB(390, 60, 0x40, 0x60, 0x80, tol);
        ExpectRGB(500, 60, 0x11, 0x88, 0xee, tol);   /* pen 5 via SetRGB32 */
        ExpectRGB(600, 300, 0x00, 0x00, 0x00, tol);  /* cleared background */
    }
};

/* 640x480 R5G6B5PC: 16 bit pixels are expanded to 8 bits per gun */
TEST_F(RtgScreenTest, HiColorScreenShowsChunkyPixels) {
    StartScreen("50031100");
    EXPECT_NE(GetOutput().find("depth 16 fmt 4"), std::string::npos) << GetOutput();

    int w = 0, h = 0, d = 0;
    ASSERT_TRUE(lxa_get_screen_dimensions(&w, &h, &d));
    EXPECT_EQ(w, 640);
    EXPECT_EQ(h, 480);
    CheckColours(8);
    Finish();
}

/* 640x480 B8G8R8A8: exact colours */
TEST_F(RtgScreenTest, TrueColorScreenShowsExactColours) {
    StartScreen("50031303");
    EXPECT_NE(GetOutput().find(" fmt 9"), std::string::npos) << GetOutput();
    CheckColours(0);

    char path[] = "/tmp/lxa_rtg_capture_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    close(fd);
    EXPECT_TRUE(lxa_capture_screen(path));
    FILE *f = fopen(path, "rb");
    ASSERT_NE(f, nullptr);
    fseek(f, 0, SEEK_END);
    EXPECT_GT(ftell(f), 100);
    fclose(f);
    unlink(path);
    Finish();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
