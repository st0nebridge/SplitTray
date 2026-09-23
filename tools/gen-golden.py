#!/usr/bin/env python3
"""Regenerate tests/regression/golden_payloads.h from a probe capture.

Usage:
    python tools/gen-golden.py [tests/probe/probe-output-26100.txt]

The probe (tests/probe/shell32_wire_probe.cpp) prints golden buffers at the end
of its run; this turns the chosen shapes into a C++ header the regression tests
include. Re-run the probe and then this script if a Windows update changes the
Shell_TrayWnd record.
"""
import io
import re
import sys

# Which probe shapes to pin, and the names they get in the header.
WANTED = [
    ("kGolden0", "kPayloadUnicodeV4",
     "V4 Unicode NIM_ADD, NIF_MESSAGE|NIF_ICON|NIF_TIP"),
    ("kGolden1", "kPayloadUnicodeV4Guid", "V4 Unicode NIM_ADD with NIF_GUID"),
    ("kGolden5", "kPayloadUnicodeV1",
     "V1 Unicode caller (cbSize 168) upconverted by shell32"),
    ("kGolden6", "kPayloadAnsiV4", "ANSI caller upconverted to UTF-16 by shell32"),
    ("kGolden8", "kPayloadSetVersion4", "NIM_SETVERSION with uVersion = 4"),
]

HEADER_PATH = "tests/regression/golden_payloads.h"


def main():
    probe_path = sys.argv[1] if len(sys.argv) > 1 else "tests/probe/probe-output-26100.txt"
    src = io.open(probe_path, encoding="utf-8", errors="replace").read()

    labels = dict(
        (name, label)
        for label, name in re.findall(
            r"// golden: (.*?) \(dwData=\d+, cbData=\d+\)\n"
            r"static const unsigned char (kGolden\d+)\[\]",
            src,
        )
    )
    arrays = {}
    for m in re.finditer(
        r"static const unsigned char (kGolden\d+)\[\] = \{(.*?)\n\};", src, re.S
    ):
        arrays[m.group(1)] = bytes(
            int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2))
        )

    out = [
        "// " + "=" * 76,
        "// Module:  tests/regression/golden_payloads.h",
        "// Purpose: Byte-for-byte Shell_TrayWnd WM_COPYDATA payloads captured from the",
        "//          real shell32 on Windows 10.0.26100 by",
        "//          tests/probe/shell32_wire_probe.cpp. These pin the wire format the",
        "//          mod parses: if a Windows update changes it, these tests fail loudly",
        "//          instead of the mod silently mirroring nothing.",
        "// Note:    GENERATED FILE - regenerate with tools/gen-golden.py after re-running",
        "//          the probe. Do not hand-edit.",
        "// " + "=" * 76,
        "",
        "#pragma once",
        "",
        "// The probe used these inputs for every shape:",
        "//   owner hWnd = 0x000304B4, uID = 4242, uCallbackMessage = 0x8123,",
        "//   hIcon = 0x0001002B, szTip as noted per shape,",
        "//   exe path = ...\\scratchpad\\wire_probe.exe",
        "constexpr unsigned long kProbeOwnerWnd = 0x000304B4;",
        "constexpr unsigned kProbeUID = 4242;",
        "constexpr unsigned kProbeCallbackMsg = 0x8123;",
        "constexpr unsigned long kProbeIconHandle = 0x0001002B;",
        "",
    ]
    for probe_name, new_name, desc in WANTED:
        if probe_name not in arrays:
            raise SystemExit("probe output has no %s" % probe_name)
        data = arrays[probe_name]
        out.append("// %s" % desc)
        out.append("// (probe shape: %s)" % labels.get(probe_name, "?"))
        out.append("static const unsigned char %s[] = {" % new_name)
        for i in range(0, len(data), 16):
            out.append("    " + "".join("0x%02X, " % c for c in data[i:i + 16]).rstrip())
        out.append("};")
        out.append("")

    io.open(HEADER_PATH, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print("wrote %s (%d payloads)" % (HEADER_PATH, len(WANTED)))


if __name__ == "__main__":
    main()
