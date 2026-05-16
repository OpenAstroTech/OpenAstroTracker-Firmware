import os
import sys
import subprocess
Import("env")

# Ensure the coverage flags are passed to the linker
env.Append(LINKFLAGS=["--coverage"])

def generate_coverage(*args, **kwargs):
    print("Running tests to generate coverage data...")
    # Run PlatformIO tests natively
    subprocess.run(["pio", "test", "-e", "native", "-vvv"])

    print("\nGenerating coverage report...")
    # macOS Clang uses llvm-cov, Windows/Linux use standard gcov
    # gcov_tool = "llvm-cov gcov" if sys.platform == "darwin" else "gcov"
    gcov_tool = "gcov"

    output_dir = os.path.join(".pio", env["PIOENV"], "coverage_report")
    
    # Create the output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # gcovr command to parse data and generate an HTML report
    cmd = [
        "gcovr",
        "--root", ".",                   # Ensure paths are evaluated relative to the project root
        "--gcov-executable", gcov_tool,
        "--html-details", os.path.join(output_dir, "index.html"),
        "--markdown", os.path.join(output_dir, "coverage.md"),
        
        # Explicitly INCLUDE your actual source code directories
        # Add "--filter", r"lib/.*" or others if you have code there too
        #"--filter", r".*/core/.*",
        #"--filter", r".*/src/.*",
        
        # Exclude the test code itself from the final metrics
        "--exclude", r".pio/*",
        "--exclude", r"unit_tests/*",
        
        # Ignore Unity testing framework errors
        "--gcov-ignore-errors=no_working_dir_found",
        "--gcov-ignore-errors=source_not_found",     
        "--print-summary"
    ]
    
    try:
        subprocess.run(cmd, check=True)
        print("\n✅ Coverage report successfully generated: coverage_report/index.html")
    except FileNotFoundError:
        print("\n❌ Error: 'gcovr' not found. Please install it using 'pip install gcovr'")
    except subprocess.CalledProcessError:
        print("\n❌ Error: Failed to generate coverage report.")

# Register the custom target in PlatformIO
env.AddCustomTarget(
    name="coverage",
    dependencies=None,
    actions=[generate_coverage],
    title="Coverage Report",
    description="Run native tests and generate an HTML code coverage report"
)