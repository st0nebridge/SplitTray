#!/usr/bin/env python3
"""Mutation check for the split-tray test suites.

Coverage proves the tests execute the code; this proves they would notice if the
code were wrong. Each mutant, listed in tools/mutants.py, is a defect the mod
could plausibly ship - several of them are defects it actually did ship -
injected into a scratch copy of the source. A mutant that SURVIVES means the
suites have a blind spot there.

Each mutant declares which suites should be able to kill it, so the slow
integration run is only used where it is the suite that matters.

Usage:
    python tools/mutate.py            # run every mutant
    python tools/mutate.py --list     # just list them

Exits non-zero if any mutant survives.
"""
import argparse
import io
import os
import shutil
import subprocess
import sys
import tempfile

from mutants import INTEGRATION_SUITE, MUTANTS, UNIT_SUITE

CLANG = r"C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
MOD = os.path.join("src", "split-tray.wh.cpp")
UNIT = os.path.join("tests", "regression", "split_tray_tests.cpp")
INTEGRATION = os.path.join("tests", "integration", "mod_integration_test.cpp")
HARNESS = os.path.join("tests", "harness")

COMMON_FLAGS = [
    "-x", "c++", "-std=c++23", "-target", "x86_64-w64-mingw32",
    "-DUNICODE", "-D_UNICODE", "-DWINVER=0x0A00", "-D_WIN32_WINNT=0x0A00",
    "-DNTDDI_VERSION=0x0A000008", "-D__USE_MINGW_ANSI_STDIO=0", "-O0",
    "-DSPLITTRAY_NO_XAML",
]
LINK_FLAGS = ["-lcomctl32", "-lgdi32", "-luser32", "-lshell32", "-lole32",
              "-loleaut32", "-lruntimeobject", "-lshlwapi", "-static",
              "-Wno-cast-function-type-mismatch"]


def build_and_run(work_dir, source, tag, timeout):
    exe = os.path.join(work_dir, "mutant_%s.exe" % tag)
    cmd = (
        [CLANG] + COMMON_FLAGS
        + ["-I", os.path.join(work_dir, HARNESS), "-o", exe,
           os.path.join(work_dir, source)]
        + LINK_FLAGS
    )
    build = subprocess.run(cmd, capture_output=True, text=True, cwd=work_dir)
    if build.returncode != 0:
        detail = build.stderr.strip().splitlines()
        return "BUILD-FAIL", (detail[-1][:110] if detail else "no stderr")
    try:
        run = subprocess.run([exe], capture_output=True, text=True, cwd=work_dir,
                             timeout=timeout)
    except subprocess.TimeoutExpired:
        return "KILLED", "timed out"
    tail = run.stdout.strip().splitlines()[-1] if run.stdout.strip() else "no output"
    return ("KILLED" if run.returncode != 0 else "SURVIVED"), tail


SUITES = {
    UNIT_SUITE: (UNIT, 120),
    INTEGRATION_SUITE: (INTEGRATION, 240),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    if args.list:
        for description, _, _, suites in MUTANTS:
            print(" - [%s] %s" % ("+".join(suites), description))
        return 0

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)
    original = io.open(MOD, encoding="utf-8").read()

    work_dir = tempfile.mkdtemp(prefix="split-tray-mutate-")
    try:
        shutil.copytree("src", os.path.join(work_dir, "src"))
        shutil.copytree("tests", os.path.join(work_dir, "tests"))
        mutant_path = os.path.join(work_dir, MOD)

        # Sanity check: the unmutated suites must pass, or every result is noise.
        for name in sorted({s for _, _, _, suites in MUTANTS for s in suites}):
            source, timeout = SUITES[name]
            status, detail = build_and_run(work_dir, source, "baseline_" + name,
                                           timeout)
            if status != "SURVIVED":
                print("baseline %s suite does not pass (%s: %s)"
                      % (name, status, detail))
                return 1
            print("baseline %-12s %s" % (name, detail))
        print()

        print("%-10s %-14s %s" % ("result", "suite", "mutation"))
        print("-" * 100)
        killed = 0
        survived = []
        for index, (description, old, new, suites) in enumerate(MUTANTS):
            if old not in original:
                print("%-10s %-14s %s" % ("NO-MATCH", "+".join(suites), description))
                survived.append(description + " (pattern no longer in source)")
                continue
            io.open(mutant_path, "w", encoding="utf-8", newline="").write(
                original.replace(old, new, 1)
            )
            outcome = "SURVIVED"
            detail = ""
            for name in suites:
                source, timeout = SUITES[name]
                status, detail = build_and_run(work_dir, source,
                                               "%d_%s" % (index, name), timeout)
                if status != "SURVIVED":
                    outcome = status
                    break
            if outcome == "KILLED":
                killed += 1
            else:
                survived.append(description)
            print("%-10s %-14s %s   (%s)"
                  % (outcome, "+".join(suites), description, detail))

        print("-" * 100)
        print("mutation score: %d/%d = %d%%" % (killed, len(MUTANTS),
                                                100 * killed // len(MUTANTS)))
        if survived:
            print("\nsurviving mutants (blind spots in the suites):")
            for description in survived:
                print(" -", description)
            return 1
        return 0
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
