#!/usr/bin/env python3
"""Fail the build on control characters that do not belong in source files.

Every one of these got into this repository the same way: a shell heredoc
collapsing a backslash escape, so `\\b` became a backspace byte, `\\0` became a
NUL, and the file still compiled or rendered closely enough that nobody noticed.
A NUL inside a character literal even has the right value, so the code worked and
the file was quietly corrupt.

Cheap to check, so it is checked rather than remembered.

Usage:
    python tools/check-sources.py [--fix]

--fix rewrites the obvious cases back to their escape sequences.
"""
import argparse
import io
import os
import subprocess
import sys

# byte -> (name, what it almost certainly should have been)
BAD = {
    0x00: ("NUL", r"\0"),
    0x08: ("backspace", r"\b"),
    0x0B: ("vertical tab", r"\v"),
    0x0C: ("form feed", r"\f"),
    0x1B: ("escape", r"\e"),
}

# Binary by nature; a control byte there means nothing.
SKIP_SUFFIXES = (".png", ".ico", ".dll", ".exe", ".o", ".a", ".pdb", ".zip")
SKIP_PREFIXES = (".archive/", "build/")


def tracked_files():
    result = subprocess.run(["git", "ls-files"], capture_output=True, text=True)
    if result.returncode != 0:
        return []
    return [line for line in result.stdout.splitlines() if line]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fix", action="store_true")
    args = parser.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)

    problems = 0
    scanned = 0
    for path in tracked_files():
        if path.lower().endswith(SKIP_SUFFIXES):
            continue
        if any(path.startswith(prefix) for prefix in SKIP_PREFIXES):
            continue
        if not os.path.isfile(path):
            continue

        with io.open(path, "rb") as handle:
            data = handle.read()
        scanned += 1

        found = {byte: data.count(bytes([byte])) for byte in BAD
                 if bytes([byte]) in data}
        if not found:
            continue

        for byte, count in found.items():
            name, escape = BAD[byte]
            print("%s: %d %s byte(s) - almost certainly meant %s"
                  % (path, count, name, escape))
            problems += count

        if args.fix:
            for byte in found:
                _, escape = BAD[byte]
                data = data.replace(bytes([byte]), escape.encode("ascii"))
            with io.open(path, "wb") as handle:
                handle.write(data)
            print("  rewritten")

    print()
    print("scanned %d tracked text files" % scanned)
    if problems and not args.fix:
        print("FAIL: %d stray control byte(s); re-run with --fix" % problems)
        return 1
    print("no stray control characters")
    return 0


if __name__ == "__main__":
    sys.exit(main())
