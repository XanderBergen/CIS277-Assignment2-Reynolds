#!/usr/bin/env python3
"""Public preflight for CIS-277 Assignment 2. This does not assign a grade."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent
REQUIRED = [
    "Stack.h", "MemoryPool.h", "MemoryPool.cpp", "Queue.h",
    "PacketQueue.h", "PacketQueue.cpp", "main.cpp", "README.md",
    "tests/public_tests.cpp",
]
HEADINGS = [
    "# CIS-277 Assignment 2: Network Packet Arrival Queue",
    "## Student", "## Design", "## How to Compile", "## How to Run",
    "## Tests and Results", "## Analysis Questions", "## AI Use",
]


def run_command(command, timeout):
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    except FileNotFoundError:
        print(f"INCOMPLETE: command not found: {command[0]}")
        return None
    except subprocess.TimeoutExpired:
        print(f"FAIL: timed out after {timeout} seconds: {' '.join(command)}")
        return False
    if result.returncode != 0:
        print(f"FAIL: command exited {result.returncode}: {' '.join(command)}")
        if result.stdout.strip():
            print(result.stdout[-6000:])
        if result.stderr.strip():
            print(result.stderr[-6000:])
        return False
    if result.stdout.strip():
        print(result.stdout.strip()[-3000:])
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"), help="C++23 compiler command (default: CXX or g++; current cl is also supported)")
    args = parser.parse_args()

    missing = [name for name in REQUIRED if not (ROOT / name).is_file()]
    if missing:
        print("FAIL: required files missing from the repository root:")
        for name in missing:
            print("  " + name)
        return 1
    print("PASS: required files found")

    readme = (ROOT / "README.md").read_text(encoding="utf-8-sig", errors="replace")
    missing_headings = [heading for heading in HEADINGS if heading not in readme]
    if missing_headings:
        print("FAIL: README headings missing:")
        for heading in missing_headings:
            print("  " + heading)
        return 1
    print("PASS: README headings found (content still needs instructor review)")

    compiler = shlex.split(args.compiler)
    if not compiler:
        print("INCOMPLETE: no compiler command was provided")
        return 2
    msvc = Path(compiler[0]).name.lower() in ("cl", "cl.exe")

    def compile_command(sources, target):
        if msvc:
            return compiler + ["/nologo", "/std:c++latest", "/Zc:__cplusplus", "/EHsc", "/W4", "/I", str(ROOT)] + [str(ROOT / name) for name in sources] + ["/Fo:" + str(Path(target).parent) + os.sep, "/Fe:" + target]
        return compiler + ["-std=c++23", "-Wall", "-Wextra", "-pedantic", "-I", str(ROOT)] + [str(ROOT / name) for name in sources] + ["-o", target]

    with tempfile.TemporaryDirectory(prefix="cis277-a2-") as directory:
        executable = str(Path(directory) / ("public_tests.exe" if os.name == "nt" else "public_tests"))
        command = compile_command(["tests/public_tests.cpp", "PacketQueue.cpp", "MemoryPool.cpp"], executable)
        print("Compiling independent public tests...")
        compiled = run_command(command, 60)
        if compiled is None:
            return 2
        if not compiled:
            return 1
        print("Running independent public tests...")
        passed = run_command([executable], 15)
        if passed is None:
            return 2
        if not passed:
            return 1

        demonstration = str(Path(directory) / ("packet_queue.exe" if os.name == "nt" else "packet_queue"))
        demo_command = compile_command(["main.cpp", "PacketQueue.cpp", "MemoryPool.cpp"], demonstration)
        print("Compiling student demonstration...")
        compiled = run_command(demo_command, 60)
        if compiled is None:
            return 2
        if not compiled:
            return 1

    print("PASS: public preflight checks completed. Review the full assignment before submitting.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
