#!/usr/bin/env python3
"""Check the mod's settings block against the settings the code actually reads.

A Windhawk setting that the ==WindhawkModSettings== block does not declare reads
back as 0 or an empty string, silently. There is no compile error and no warning:
the mod just behaves as if the user never changed anything. This lints for that,
and for the reverse (a declared setting nothing reads).

Also validates the block as YAML, since a syntax error there stops the mod
compiling in the Windhawk UI with a message that does not point at the cause.

Usage:
    python tools/check-settings.py
Exits non-zero on a mismatch.
"""
import io
import os
import re
import sys

MOD = os.path.join("src", "split-tray.wh.cpp")


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)
    src = io.open(MOD, encoding="utf-8").read()

    match = re.search(
        r"// ==WindhawkModSettings==\n/\*\n(.*?)\n\*/\n// ==/WindhawkModSettings==",
        src,
        re.S,
    )
    if not match:
        print("FAIL: no ==WindhawkModSettings== block found")
        return 1
    block = match.group(1)

    try:
        import yaml
    except ImportError:
        print("note: pyyaml not installed, skipping the YAML syntax check")
        declared = set(re.findall(r"^- (\w+):", block, re.M))
        declared |= set(
            "%s[%%d].%s" % (outer, inner)
            for outer in re.findall(r"^- (\w+):\s*$", block, re.M)
            for inner in re.findall(r"^  (?:- )?- (\w+):", block, re.M)
        )
    else:
        try:
            entries = yaml.safe_load(block)
        except yaml.YAMLError as error:
            print("FAIL: the settings block is not valid YAML:\n%s" % error)
            return 1
        if not isinstance(entries, list):
            print("FAIL: the settings block must be a YAML list, got %s"
                  % type(entries).__name__)
            return 1
        declared = set()
        for entry in entries:
            if not isinstance(entry, dict):
                print("FAIL: settings entry is not a mapping: %r" % entry)
                return 1
            keys = [k for k in entry if not k.startswith("$")]
            if len(keys) != 1:
                print("FAIL: settings entry must have exactly one value key, got %r"
                      % keys)
                return 1
            name = keys[0]
            value = entry[name]
            if isinstance(value, list):
                # A list of structs: the code reads it as name[%d].field
                for item in value[0] if value else []:
                    for field in item:
                        if not field.startswith("$"):
                            declared.add("%s[%%d].%s" % (name, field))
            else:
                declared.add(name)

    # Everything the code asks Windhawk for.
    used = set()
    for call in re.findall(
        r"Wh_GetIntSetting\(\s*L\"([^\"]+)\"|"
        r"StringSetting::make\(\s*L\"([^\"]+)\"", src
    ):
        used.add(call[0] or call[1])

    missing = sorted(used - declared)
    unused = sorted(declared - used)

    print("declared in the settings block : %d" % len(declared))
    print("read by the code               : %d" % len(used))

    status = 0

    # tools/install.ps1 seeds the registry by parsing this same block with a
    # plain regex, because Windhawk's UI is not there to do it on a hand install
    # and a setting missing from the registry reads back as 0 with no error.
    # Re-run its rule here so a block it cannot parse is caught at build time
    # rather than as a feature that is silently off.
    #
    # This exists because embedInTaskbar defaulted to on in the block, was never
    # written to the registry, read back as off, and switched the entire XAML
    # attachment off without a single line in the log.
    scalars_by_yaml = set()
    for entry in entries if isinstance(entries, list) else []:
        keys = [k for k in entry if not k.startswith("$")]
        if keys and not isinstance(entry[keys[0]], list):
            scalars_by_yaml.add(keys[0])

    scalars_by_installer = set()
    unparsable = []
    for name, value in re.findall(r"^- (\w+):[ \t]*(.*)$", block, re.M):
        value = value.strip()
        if value == "":
            continue                      # a list setting, legitimately unseeded
        if value[:1] in (">", "|"):
            unparsable.append(name)       # folded scalar: the regex cannot read it
            continue
        scalars_by_installer.add(name)

    print("scalars the installer seeds    : %d of %d"
          % (len(scalars_by_installer), len(scalars_by_yaml)))

    unseeded = sorted(scalars_by_yaml - scalars_by_installer)
    if unseeded:
        status = 1
        print("\nFAIL: declared scalars that tools/install.ps1 would not seed "
              "(they would read back as 0 / empty at runtime):")
        for name in unseeded:
            note = " (folded scalar)" if name in unparsable else ""
            print("  - %s%s" % (name, note))
    if missing:
        status = 1
        print("\nFAIL: read by the code but not declared "
              "(these silently read back as 0 / empty):")
        for name in missing:
            print("  -", name)
    if unused:
        status = 1
        print("\nFAIL: declared but never read (dead setting in the UI):")
        for name in unused:
            print("  -", name)
    if status == 0:
        print("\nsettings block and code agree")
    return status


if __name__ == "__main__":
    sys.exit(main())
