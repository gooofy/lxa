/**
 * smoke_gtest.cpp — Phase 144: Startup smoke test suite
 *
 * Parameterised test that, for each known app binary in lxa-apps/, loads the
 * app, runs 100 VBlank settle iterations, and asserts:
 *   (1) no SIGABRT (lxa did not call abort())
 *   (2) no "PANIC" string in output
 *   (3) no rv=26 unless the app is on the allowlist
 *
 * A missing app binary results in GTEST_SKIP() rather than a test failure, so
 * the suite is self-contained and CI-safe when only a subset of app bundles is
 * present.
 *
 * Apps that legitimately exit immediately (command-line tools, or apps that
 * need RTG hardware not yet present) are added to the allowlist below.
 */

#include "lxa_test.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

using namespace lxa::testing;

/* -------------------------------------------------------------------------
 * Smoke test parameters
 * -------------------------------------------------------------------------
 * path        — APPS:-relative path to the binary
 * allowlist_rv26 — true if this app is known to exit with rv=26 legitimately
 *                  (e.g. because it needs RTG hardware or external hardware)
 * extra_assign — optional "ASSIGN=PATH" string to add before loading
 *                (relative to the app's base directory under lxa-apps/)
 * ------------------------------------------------------------------------- */
struct SmokeAppParam {
    const char* name;           /* short human-readable name for test ID */
    const char* binary_path;    /* APPS:-relative path to the executable */
    bool        allowlist_rv26; /* app may exit with rc>=20 legitimately */
    const char* extra_assign;   /* optional "ASSIGN:appdir_subpath" or nullptr */
};

/* Apps that probe for RTG / Picasso96 and exit because it is not yet present
 * are allowlisted until Phase 155 (RTG app validation). */
static const SmokeAppParam kSmokeApps[] = {
    { "SysInfo",        "APPS:SysInfo/bin/SysInfo/SysInfo",               false, nullptr },
    { "DevPac",         "APPS:DevPac/bin/Devpac/Devpac",                  false, nullptr },
    { "ASM-One",        "APPS:Asm-One/bin/ASM-One/ASM-One_V1.48",         false, nullptr },
    { "KickPascal2",    "APPS:kickpascal2/bin/KP2/KP",                    false, nullptr },
    { "MaxonBASIC",     "APPS:MaxonBASIC/bin/MaxonBASIC/MaxonBASIC",       false, nullptr },
    { "BlitzBasic2",    "APPS:BlitzBasic2/blitz2",                         false, nullptr },
    { "Cluster2",       "APPS:Cluster2/bin/Cluster2/Editor",               false, nullptr },
    { "DPaintV",        "APPS:DPaintV/bin/DPaintV/DPaint",                 false, nullptr },
    { "DirectoryOpus",  "APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus",      false, nullptr },
    { "SIGMAth2",       "APPS:SIGMAth2/SIGMAth_2",                         false, nullptr },
    { "Vim",            "APPS:vim-5.3/Vim",                                 false, nullptr },
    { "FinalWriter",    "APPS:FinalWriter_D/FinalWriter",                   false, nullptr },
    { "PPaint",         "APPS:ppaint/ppaint",                               true,  "PPaint:ppaint" },
    { "Typeface",       "APPS:Typeface/Typeface",                           false, nullptr },
    { "ProWrite",       "APPS:ProWrite/ProWrite",                           false, nullptr },
    { "Sonix",          "APPS:Sonix 2/Sonix",                               false, nullptr },
};

/* -------------------------------------------------------------------------
 * Helper — resolve a binary path under APPS:
 * Returns empty string when the binary does not exist on disk.
 * ------------------------------------------------------------------------- */
static std::string ResolveBinaryPath(const char* apps_path,
                                     const char* binary_path)
{
    /* binary_path starts with "APPS:" — strip it */
    const char* rel = binary_path;
    if (strncmp(rel, "APPS:", 5) == 0) {
        rel += 5;
    }

    std::filesystem::path full = std::filesystem::path(apps_path) / rel;
    if (std::filesystem::exists(full)) {
        return full.string();
    }
    return {};
}

/* -------------------------------------------------------------------------
 * Fixture
 * ------------------------------------------------------------------------- */
class SmokeTest : public ::testing::TestWithParam<SmokeAppParam> {
protected:
    std::string ram_dir_;
    std::string t_dir_;
    std::string env_dir_;

    void SetUp() override
    {
        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);
    }

    void TearDown() override
    {
        lxa_shutdown();

        auto cleanup = [](std::string& dir) {
            if (!dir.empty()) {
                std::string cmd = "rm -rf " + dir;
                (void)system(cmd.c_str());
                dir.clear();
            }
        };
        cleanup(ram_dir_);
        cleanup(t_dir_);
        cleanup(env_dir_);
    }

    /* Initialise emulator + assigns, mirroring LxaTest::SetUp(). */
    bool InitEmulator()
    {
        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = true;

        config.rom_path = FindRomPath();
        if (!config.rom_path) {
            ADD_FAILURE() << "Could not find lxa.rom";
            return false;
        }

        if (lxa_init(&config) != 0) {
            ADD_FAILURE() << "lxa_init() failed";
            return false;
        }

        const char* samples = FindSamplesPath();
        if (samples) lxa_add_assign("SYS", samples);

        const char* sys_base = FindSystemBasePath();
        if (sys_base) lxa_add_assign_path("SYS", sys_base);

        const char* tp_libs = FindThirdPartyLibsPath();
        if (tp_libs) lxa_add_assign("LIBS", tp_libs);

        const char* libs = FindSystemLibsPath();
        if (libs) lxa_add_assign_path("LIBS", libs);

        const char* cmds = FindCommandsPath();
        if (cmds) lxa_add_assign("C", cmds);

        const char* syspath = FindSystemPath();
        if (syspath) lxa_add_assign("System", syspath);

        const char* gadgets = FindGadgetsPath();
        if (gadgets) lxa_add_assign("GADGETS", gadgets);

        const char* apps = FindAppsPath();
        if (apps) lxa_add_assign("APPS", apps);

        /* RAM: */
        char ram_buf[] = "/tmp/lxa_smoke_RAM_XXXXXX";
        if (mkdtemp(ram_buf)) {
            ram_dir_ = ram_buf;
            lxa_add_drive("RAM", ram_buf);
        }

        /* T: */
        char t_buf[] = "/tmp/lxa_smoke_T_XXXXXX";
        if (mkdtemp(t_buf)) {
            t_dir_ = t_buf;
            lxa_add_assign("T", t_buf);
        }

        /* ENV: / ENVARC: */
        char env_buf[] = "/tmp/lxa_smoke_ENV_XXXXXX";
        if (mkdtemp(env_buf)) {
            env_dir_ = env_buf;
            lxa_add_assign("ENV", env_buf);
            lxa_add_assign("ENVARC", env_buf);
        }

        return true;
    }

    /* Add an extra assign of the form "ASSIGN:appdir_subpath".
     * The path after the colon is treated as relative to the app directory
     * rooted at APPS:/<app_dir_prefix>.  For example "PPaint:ppaint" maps
     * APPS:ppaint → PPaint: .
     */
    void AddExtraAssign(const char* apps_path, const char* extra_assign)
    {
        if (!extra_assign || !apps_path) return;

        /* Split "NAME:rel_path" */
        const char* colon = strchr(extra_assign, ':');
        if (!colon) return;

        std::string assign_name(extra_assign, colon - extra_assign);
        std::string rel_path(colon + 1);

        std::filesystem::path full = std::filesystem::path(apps_path) / rel_path;
        if (std::filesystem::exists(full)) {
            lxa_add_assign(assign_name.c_str(), full.c_str());
        }
    }
};

/* -------------------------------------------------------------------------
 * The smoke test itself
 * ------------------------------------------------------------------------- */
TEST_P(SmokeTest, StartsWithoutPanicOrAbort)
{
    const SmokeAppParam& param = GetParam();

    const char* apps_path = FindAppsPath();
    if (!apps_path) {
        GTEST_SKIP() << "lxa-apps directory not found — skipping " << param.name;
    }

    /* Verify binary exists before initialising the emulator */
    std::string binary = ResolveBinaryPath(apps_path, param.binary_path);
    if (binary.empty()) {
        GTEST_SKIP() << param.name << " binary not found ("
                     << param.binary_path << ") — skipping";
    }

    ASSERT_TRUE(InitEmulator()) << "Emulator initialisation failed";

    /* Add any app-specific assign (e.g. PPaint:) */
    AddExtraAssign(apps_path, param.extra_assign);

    /* Load the program */
    ASSERT_EQ(lxa_load_program(param.binary_path, ""), 0)
        << param.name << ": lxa_load_program() failed";

    /* Run 100 VBlank settle iterations (50 K cycles each = 5 M total).
     * This is enough for most GUI apps to open their main window and reach
     * their event loop. */
    for (int i = 0; i < 100; ++i) {
        if (!lxa_is_running()) break;
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }

    /* ---- Assertions ---- */

    char output_buf[65536];
    lxa_get_output(output_buf, sizeof(output_buf));
    const std::string output(output_buf);

    /* (1) No PANIC in output */
    EXPECT_EQ(output.find("PANIC"), std::string::npos)
        << param.name << " emitted a PANIC during startup:\n" << output;

    /* (2) Exit code check — rv>=20 is an error return on Amiga.
     *     Only check when the app has already exited; if it is still running
     *     we cannot get a meaningful exit code yet. */
    if (!lxa_is_running()) {
        int rv = lxa_get_exit_code();
        if (!param.allowlist_rv26) {
            EXPECT_LT(rv, 20)
                << param.name << " exited with rv=" << rv
                << " (rc>=20 = error) during startup settle.\nOutput:\n"
                << output;
        }
    }
}

/* -------------------------------------------------------------------------
 * Instantiate
 * ------------------------------------------------------------------------- */
INSTANTIATE_TEST_SUITE_P(
    AllApps,
    SmokeTest,
    ::testing::ValuesIn(kSmokeApps),
    [](const ::testing::TestParamInfo<SmokeAppParam>& info) {
        /* GTest parameter names must be alphanumeric */
        std::string name(info.param.name);
        std::replace_if(name.begin(), name.end(),
                        [](char c){ return !isalnum(c); }, '_');
        return name;
    });

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
