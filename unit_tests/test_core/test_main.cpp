#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

/**
 * Googletest event listener that generates a coverage report after all
 * tests finish.  Runs inside the test binary so `.gcda` files are
 * guaranteed to exist when gcovr is invoked.
 *
 * If gcovr is not installed the command is silently skipped — tests
 * still run and report normally.
 */
class CoverageListener : public ::testing::EmptyTestEventListener
{
  public:
    // Called exactly once when the test program exits — after ALL tests
    // have run and .gcda files are flushed to disk.
    void OnTestProgramEnd(const ::testing::UnitTest &/*unit_test*/) override
    {
        generate();
    }

  private:
    void generate()
    {
        // Quick check: if gcovr isn't on PATH, skip silently.
        if (std::system("command -v gcovr >/dev/null 2>&1") != 0)
        {
            return;
        }

        const char *gcov_tool = "gcov";
#if defined(__APPLE__)
        gcov_tool = "llvm-cov gcov";
#endif

        // Determine output dir relative to the project root.
        // PlatformIO builds tests under .pio/build/<env>/coverage_report/
        const char *env = std::getenv("PIOENV");
        if (!env)
        {
            env = "native";  // sensible default
        }

        // Ensure output directory exists before gcovr tries to write.
        char mkdir_cmd[256];
        std::snprintf(mkdir_cmd, sizeof(mkdir_cmd),
                      "mkdir -p .pio/build/%s/coverage_report", env);
        std::system(mkdir_cmd);

        std::cout << "\nGenerating coverage report...\n";

        char cmd[512];
        std::snprintf(cmd, sizeof(cmd),
                      "gcovr --root . --gcov-executable '%s' "
                      "--html-details .pio/build/%s/coverage_report/index.html "
                      "--markdown .pio/build/%s/coverage_report/coverage.md "
                      "--exclude '.pio/*' --exclude 'unit_tests/*' "
                      "--gcov-ignore-errors=no_working_dir_found "
                      "--gcov-ignore-errors=source_not_found "
                      "--print-summary",
                      gcov_tool, env, env);

        int result = std::system(cmd);
        if (result == 0)
        {
            std::cout << "\n✅ Coverage report: coverage_report/index.html\n";
        }
    }
};

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // Replace the default printer with our coverage-aware listener.
    ::testing::TestEventListeners &listeners =
        ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new CoverageListener());

    return RUN_ALL_TESTS();
}
