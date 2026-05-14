import os
import subprocess
from pathlib import Path
import sys
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    def Import(*names: str) -> tuple[Any, ...]:
        ...

def _project_root():
    return Path(__file__).resolve().parent.parent


def _native_build_dir():
    return _project_root() / ".pio" / "build" / "native"


def _has_coverage_data():
    return any(_native_build_dir().rglob("*.gcda"))


def ensure_gcovr_installed(build_env):
    """Checks if gcovr is installed, and installs it via pip if not."""
    try:
        import gcovr
    except ImportError:
        print("gcovr not found! Installing it into the PlatformIO environment...")
        # $PYTHONEXE ensures we use PlatformIO's isolated Python environment, not the system OS Python
        build_env.Execute("$PYTHONEXE -m pip install gcovr")


def generateCoverageInfo():
    if not _has_coverage_data():
        print("Skipping coverage report generation because no .gcda files were produced.")
        return

    print("Generating code coverage report...")
    gcovr_cmd = ["gcovr"]
    report_dir = _project_root()
    # Adjust this path if you are testing multiple specific folders
    subprocess.run(gcovr_cmd + ["--html-details", ".pio/coverage.html", "--filter", "src/"], check=True, cwd=report_dir)
    print(f"Coverage report generated at: .pio/coverage.html")
    subprocess.run(gcovr_cmd + ["--markdown", ".pio/coverage.md", "--filter", "src/"], check=True, cwd=report_dir)
    print(f"Coverage report generated at: .pio/coverage.md")


def configure_build():
    Import("env")
    build_env = globals()["env"]
    build_env.Append(LINKFLAGS=["--coverage"])
    ensure_gcovr_installed(build_env)


def main(argv):
    if not argv:
        print("Usage: test-coverage.py <test-program> [args...]", file=sys.stderr)
        return 2

    completed = subprocess.run(argv, cwd=_project_root(), env=os.environ.copy(), check=False)
    generateCoverageInfo()
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

configure_build()