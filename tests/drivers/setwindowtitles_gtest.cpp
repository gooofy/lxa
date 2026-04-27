/**
 * setwindowtitles_gtest.cpp - SetWindowTitles title-bar repaint regression
 */

#include "lxa_test.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace lxa::testing;

namespace {

struct TextDraw {
    std::string text;
    int x;
    int y;
};

class SetWindowTitlesTest : public LxaUITest {
protected:
    std::vector<TextDraw> text_draws;

    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        lxa_set_text_hook([](const char *s, int n, int x, int y, void *ud) {
            auto *draws = static_cast<std::vector<TextDraw> *>(ud);
            draws->push_back({std::string(s, n), x, y});
        }, &text_draws);

        ASSERT_EQ(lxa_load_program("SYS:SetWindowTitles", ""), 0)
            << "Failed to load SetWindowTitles sample";
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000));
    }

    void TearDown() override {
        lxa_clear_text_hook();
        LxaUITest::TearDown();
    }

    int CountTitleBarPixelsNotPen(int pen) {
        int count = 0;

        for (int y = window_info.y + 2; y <= window_info.y + 12; y++) {
            for (int x = window_info.x + 36; x <= window_info.x + 150; x++) {
                int value = -1;
                if (lxa_read_pixel(x, y, &value) && value != pen) {
                    count++;
                }
            }
        }

        return count;
    }

    bool WaitForOutput(const char *needle, int timeout_ms = 5000) {
        int attempts = timeout_ms / 50;

        for (int i = 0; i < attempts; i++) {
            if (GetOutput().find(needle) != std::string::npos) {
                return true;
            }
            RunCyclesWithVBlank(1, 50000);
        }

        return false;
    }

    bool SawText(const char *needle, int min_y, int max_y) const {
        return std::any_of(text_draws.begin(), text_draws.end(),
            [&](const TextDraw& draw) {
                return draw.y >= min_y && draw.y <= max_y &&
                       draw.text.find(needle) != std::string::npos;
            });
    }

    bool SawText(const char *needle) const {
        return std::any_of(text_draws.begin(), text_draws.end(),
            [&](const TextDraw& draw) {
                return draw.text.find(needle) != std::string::npos;
            });
    }

    std::vector<int> ReadTitleBarPatch() {
        std::vector<int> pixels;

        for (int y = window_info.y + 2; y <= window_info.y + 12; y++) {
            for (int x = window_info.x + 36; x <= window_info.x + 150; x++) {
                int value = -1;
                lxa_read_pixel(x, y, &value);
                pixels.push_back(value);
            }
        }

        return pixels;
    }

    int CountDifferentPixels(const std::vector<int>& a, const std::vector<int>& b) const {
        int count = 0;
        size_t n = std::min(a.size(), b.size());

        for (size_t i = 0; i < n; i++) {
            if (a[i] != b[i]) {
                count++;
            }
        }

        return count;
    }
};

TEST_F(SetWindowTitlesTest, WindowTitleBarRepaintsImmediately) {
    ASSERT_TRUE(WaitForOutput("READY: before"));
    RunCyclesWithVBlank(5, 50000);
    lxa_flush_display();

    int before_pixels = CountTitleBarPixelsNotPen(1);
    auto before_patch = ReadTitleBarPatch();
    ASSERT_GT(before_pixels, 0)
        << "Initial title bar should contain title/system gadget pixels";

    text_draws.clear();
    TypeString("t");

    ASSERT_TRUE(WaitForOutput("READY: after"));
    RunCyclesWithVBlank(10, 50000);
    ASSERT_TRUE(GetWindowInfo(0, &window_info));
    lxa_flush_display();

    int after_pixels = CountTitleBarPixelsNotPen(1);
    auto after_patch = ReadTitleBarPatch();
    EXPECT_GT(after_pixels, 0)
        << "Updated title bar should contain freshly rendered title pixels";
    EXPECT_GT(CountDifferentPixels(before_patch, after_patch), 0)
        << "SetWindowTitles should change visible title-bar pixels immediately";
    EXPECT_TRUE(SawText("After"))
        << "SetWindowTitles should cause the new window title text to render";
}

TEST_F(SetWindowTitlesTest, ScreenTitleRepaintsImmediately) {
    ASSERT_TRUE(WaitForOutput("READY: before"));
    text_draws.clear();
    TypeString("t");
    ASSERT_TRUE(WaitForOutput("READY: after"));
    RunCyclesWithVBlank(10, 50000);

    EXPECT_TRUE(SawText("ScreenAfter"))
        << "SetWindowTitles should repaint the visible screen title bar";
}

}  // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
