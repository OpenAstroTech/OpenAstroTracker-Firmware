import subprocess
Import("env")

env.Append(LINKFLAGS=["--coverage"])

def ensure_gcovr_installed():
    """Checks if gcovr is installed, and installs it via pip if not."""
    try:
        import gcovr
    except ImportError:
        print("gcovr not found! Installing it into the PlatformIO environment...")
        # $PYTHONEXE ensures we use PlatformIO's isolated Python environment, not the system OS Python
        env.Execute("$PYTHONEXE -m pip install gcovr")

def generateCoverageInfo(source, target, env):
    print("Generating code coverage report...")
    # Adjust this path if you are testing multiple specific folders
    subprocess.run(["gcovr", "--html-details", f".pio/coverage.html", "--filter", "src/"], check=False)
    print(f"Coverage report generated at: .pio/coverage.html")

ensure_gcovr_installed()

# Trigger the report generation after the test program runs
env.AddPostAction(".pio/build/native/program", generateCoverageInfo)