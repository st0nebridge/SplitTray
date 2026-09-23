#!/usr/bin/env python3
"""Verify every symbol the mod hooks still exists in the live Windows binaries.

Symbol hooks into Explorer's private DLLs are the fragile part of the embedding
(DECISIONS.md 24). They resolve at runtime against Microsoft's PDBs, so a Windows
update that renames or removes one turns the mod into a no-op with nothing but a
log line to say so - the same silent-failure shape as an unseeded setting.

This resolves each module's PDB the way Windhawk does (CodeView debug directory
-> GUID+age -> local cache, else the public symbol server), dumps its public
symbols, and checks the mangled names are there. Run as part of the build.

Usage:
    python tools/check-symbols.py [--offline]

--offline skips downloading a PDB that is not already cached, and reports it as
skipped rather than failing - for building without a network.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile
import urllib.request

SYMBOL_CACHE = r"C:\ProgramData\Windhawk\Engine\Symbols"
SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
PDBUTIL = r"C:\Program Files\LLVM\bin\llvm-pdbutil.exe"

CLIENT_CORE = (r"C:\Windows\SystemApps"
               r"\MicrosoftWindows.Client.Core_cw5n1h2txyewy")

# module path -> the mangled names the mod hooks in it, with what each is for.
REQUIRED = {
    os.path.join(CLIENT_CORE, "SystemTray.dll"): [
        ("??0IconView@implementation@SystemTray@winrt@@QEAA@XZ",
         "every tray icon view passes through this constructor - the anchor "
         "that hands the mod a live XAML element"),
    ],
    r"C:\Windows\System32\taskbar.dll": [
        ("??_7CSecondaryTaskBand@@6BITaskListWndSite@@@",
         "vftable used to find the task band on a secondary taskbar"),
        ("?GetTaskbarHost@CSecondaryTaskBand@@UEBA?AV?$shared_ptr@VTaskbarHost@@@std@@XZ",
         "secondary taskbar window -> TaskbarHost -> its XamlRoot"),
        ("?FrameHeight@TaskbarHost@@QEBAHXZ",
         "disassembled to recover the TaskbarHost element offset"),
        ("?_Decref@_Ref_count_base@std@@QEAAXXZ",
         "releases the shared_ptr GetTaskbarHost returns"),
    ],
}


def pdb_signature(path):
    """The GUID+age string that names this binary's PDB directory."""
    with open(path, "rb") as handle:
        data = handle.read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    opt = pe + 24
    dd = opt + (112 if magic == 0x20B else 96)
    debug_rva, debug_size = struct.unpack_from("<II", data, dd + 6 * 8)

    sections = struct.unpack_from("<H", data, pe + 6)[0]
    sec_off = opt + struct.unpack_from("<H", data, pe + 20)[0]

    def to_offset(rva):
        for i in range(sections):
            s = sec_off + i * 40
            va = struct.unpack_from("<I", data, s + 12)[0]
            size = struct.unpack_from("<I", data, s + 16)[0]
            raw = struct.unpack_from("<I", data, s + 20)[0]
            if va <= rva < va + size:
                return raw + (rva - va)
        return None

    off = to_offset(debug_rva)
    if off is None:
        return None, None
    for i in range(debug_size // 28):
        entry = off + i * 28
        if struct.unpack_from("<I", data, entry + 12)[0] != 2:  # IMAGE_DEBUG_TYPE_CODEVIEW
            continue
        cv = struct.unpack_from("<I", data, entry + 24)[0]
        if data[cv:cv + 4] != b"RSDS":
            continue
        guid = data[cv + 4:cv + 20]
        age = struct.unpack_from("<I", data, cv + 20)[0]
        d1, d2, d3 = struct.unpack_from("<IHH", guid, 0)
        name_end = data.index(b"\0", cv + 24)
        pdb_name = os.path.basename(
            data[cv + 24:name_end].decode("utf-8", "replace").replace("\\", "/"))
        return pdb_name, "%08X%04X%04X%s%X" % (
            d1, d2, d3, guid[8:].hex().upper(), age)
    return None, None


def locate_pdb(module_path, offline):
    pdb_name, signature = pdb_signature(module_path)
    if not signature:
        return None, "no CodeView debug entry"

    cached = os.path.join(SYMBOL_CACHE, pdb_name, signature, pdb_name)
    if os.path.isfile(cached):
        return cached, "cached"

    if offline:
        return None, "not cached (offline)"

    url = "%s/%s/%s/%s" % (SYMBOL_SERVER, pdb_name, signature, pdb_name)
    target = os.path.join(tempfile.gettempdir(),
                          "split-tray-%s-%s" % (signature[:8], pdb_name))
    if not os.path.isfile(target):
        request = urllib.request.Request(
            url, headers={"User-Agent": "Microsoft-Symbol-Server/10.0"})
        try:
            with urllib.request.urlopen(request, timeout=120) as response, \
                    open(target, "wb") as handle:
                handle.write(response.read())
        except Exception as error:  # noqa: BLE001 - reported, not swallowed
            return None, "download failed: %s" % error
    return target, "downloaded"


def public_symbols(pdb_path):
    result = subprocess.run([PDBUTIL, "dump", "-publics", pdb_path],
                            capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return set(re.findall(r"`([^`]+)`", result.stdout))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()

    if not os.path.isfile(PDBUTIL):
        print("llvm-pdbutil not found at %s; skipping the symbol check" % PDBUTIL)
        return 0

    status = 0
    for module_path, required in REQUIRED.items():
        name = os.path.basename(module_path)
        if not os.path.isfile(module_path):
            print("%-18s MODULE MISSING at %s" % (name, module_path))
            status = 1
            continue

        pdb_path, how = locate_pdb(module_path, args.offline)
        if not pdb_path:
            if args.offline:
                print("%-18s skipped (%s)" % (name, how))
                continue
            print("%-18s FAIL: could not get symbols (%s)" % (name, how))
            status = 1
            continue

        symbols = public_symbols(pdb_path)
        if symbols is None:
            print("%-18s FAIL: llvm-pdbutil could not read %s" % (name, pdb_path))
            status = 1
            continue

        print("%s  (%s, %d public symbols)" % (name, how, len(symbols)))
        for mangled, purpose in required:
            present = mangled in symbols
            if not present:
                status = 1
            print("   %-4s %s" % ("ok" if present else "MISS", mangled))
            if not present:
                print("        needed for: %s" % purpose)

    print()
    print("symbol check: %s" % ("all present" if status == 0 else "FAILED"))
    return status


if __name__ == "__main__":
    sys.exit(main())
