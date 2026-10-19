"""Host check of the explicit test console; no target connection."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/console"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    executable = OUT / "console.exe"
    subprocess.run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-DTDC_UNIT_TEST", "-Itests/unit/tdc_ut_console_host",
                    "tests/unit/tdc_ut_console_test.c", "-o", str(executable)],
                   cwd=ROOT, check=True, timeout=60)
    subprocess.run([str(executable)], cwd=ROOT, check=True, timeout=30)


if __name__ == "__main__":
    main()
