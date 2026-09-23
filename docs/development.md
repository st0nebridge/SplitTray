# Developing Split Tray

The mod is one file, [`src/split-tray.wh.cpp`](../src/split-tray.wh.cpp), because
Windhawk compiles a mod from a single translation unit. Everything else in the
repository exists to build it, test it, or keep a record of why it is the way
it is.

## What you need

* **Windows 11** with **[Windhawk](https://windhawk.net)** installed. Its bundled
  clang (`C:\Program Files\Windhawk\Compiler`) is the compiler for everything
  here, so the build uses the same toolchain Windhawk does.
* **[LLVM](https://llvm.org)** in `C:\Program Files\LLVM`, for two tools:
  `llvm-dlltool` builds an import library for `windhawk.dll` so the mod DLL can
  be linked outside Windhawk, and `llvm-pdbutil` reads Microsoft's symbol files
  for the symbol check.
* **Python 3**, for the lint and mutation tools. `pyyaml` is optional. With it,
  the settings lint also checks that the settings block is valid YAML.
* **PowerShell** for the build scripts.

## Building

```powershell
tools\build.ps1              # everything: checks, both test suites, tools, mod DLL
tools\build.ps1 -SkipTests   # checks and the DLL only
tools\run-tests.ps1          # checks and tests, no DLL
tools\run-tests.ps1 -Mutate  # ...plus the mutation check
```

`build.ps1` runs these steps in order and stops at the first failure:

1. **Source hygiene** (`tools/check-sources.py`) looks for stray control bytes,
   which shell heredocs have introduced before (DECISIONS 31).
2. **Settings lint** (`tools/check-settings.py`) checks that every setting the
   code reads is declared in the mod's settings block, and the reverse. An
   undeclared setting reads back as 0 with no error at all.
3. **Symbol check** (`tools/check-symbols.py`) confirms that every private
   symbol the mod hooks is still in this machine's copy of Explorer's DLLs,
   using their PDBs from Microsoft's symbol server (DECISIONS 28).
4. **Compile check** runs with the flags Windhawk's own editor uses, so a clean
   result here means Windhawk will compile the mod too.
5. **Regression tests**, then the **integration test** (below).
6. **Diagnostic tools**: the log capture and the wire probe.
7. **The mod DLL**, in `build\split-tray.dll`.

## The tests

### Regression tests

`tests/regression/split_tray_tests.cpp` includes the mod's source directly and
compiles it against the stub Windhawk headers in `tests/harness`. The code under
test is therefore the code that ships, not a copy of it. The XAML half of the
mod is left out with `SPLITTRAY_NO_XAML`; everything else is covered:

* the wire format, against bytes captured from the real shell32
  (`golden_payloads.h`)
* settings
* routing
* the trays and their layout
* the icon store, placements and replays

Displays are supplied by the test (`g_enumerateMonitors`), so the results do not
depend on the machine they run on.

### Integration test

`tests/integration/mod_integration_test.cpp` runs the whole mod against the real
`shell32` without going near the real shell:

1. It starts a copy of itself on a private desktop.
2. There, it creates its own window of class `Shell_TrayWnd` to stand in for
   Explorer.
3. It loads the mod into itself.
4. It calls the real `Shell_NotifyIcon`.

`shell32` sends the mod genuine notifications, and the test checks both sides:

* what the stand-in Explorer received,
* what Split Tray's trays show, and
* what an application gets back when it clicks an icon or asks where it is.

It gives itself two extra trays on the primary display, so it runs the same on a
machine with one display as on one with several.

### Wire probe

`tests/probe/shell32_wire_probe.cpp` records what `shell32` actually sends to
the tray. It uses the same private-desktop trick, and covers both the icon
record and the `Shell_NotifyIconGetRect` question. The captures are in
`tests/probe/`. If a Windows update is suspected of changing the format, re-run
`build\wire_probe.exe`, then regenerate the golden buffers with
`tools/gen-golden.py`.

### Mutation check

```powershell
python tools/mutate.py          # every mutant
python tools/mutate.py --list   # just list them
```

Each mutant is a plausible bug, and several are bugs the mod actually shipped.
They are listed in `tools/mutants.py`, and those from external reviews in
`tools/mutants_reviews.py`. Each one is injected into a scratch copy of the
source, and the suites that should catch it are run. A mutant that survives
marks a blind spot in the tests. The check fails unless every mutant is killed.

## Trying it in Explorer

The scripts below install the built DLL into the local Windhawk the way
Windhawk's editor would. They need an elevated shell, apart from the read-only
`-Status` and `-Logs`:

```powershell
tools\install.ps1 -Install -Enable -RestartExplorer
tools\install.ps1 -Status
tools\install.ps1 -Disable
tools\install.ps1 -Uninstall
```

For repeated live testing, `tools\redeploy.ps1` does one whole iteration behind
a single elevation prompt: it builds, installs, restarts Explorer, and captures
the mod's log into `build\live-log.txt`.

```powershell
tools\redeploy.ps1                                   # build, install, restart, capture
tools\redeploy.ps1 -NoBuild -Seconds 60              # reuse the last build
tools\redeploy.ps1 -Setting 'extraTrays[0].display=primary'
tools\redeploy.ps1 -Setting 'extraTrays[0].disabled=1'   # keep it, hide it
tools\redeploy.ps1 -ClearPlacements                  # forget hand-made moves first
```

`redeploy.ps1` restarts Explorer, so the mod always starts with it. That is not
how users meet it. Installing, updating and switching the mod on in Windhawk
all load it into an Explorer that is already running, with its icons already
registered. Check that too: open the arrange window, press **Disable** and
then **Enable** on the mod's page in Windhawk, and look at the log. Three
defects showed up only that way (DECISIONS 63 to 65).

The mod logs through `Wh_Log`, which reaches any debug-output listener. Only
one listener can run at a time, so close DebugView before using the capture:

```powershell
build\dbgcapture.exe --filter split-tray --seconds 30
```

With a listener attached, every log line costs tens of milliseconds, and the
mod handles every application's tray messages on Explorer's taskbar thread. So
the mod keeps its logging off the frequent paths (DECISIONS 44 and 51).

## Repository layout

```
src/split-tray.wh.cpp         the mod
tests/regression/             unit-level tests of the shipped source
tests/integration/            the whole mod against the real shell32
tests/probe/                  the wire probe and what it captured
tests/harness/                stub Windhawk headers for the tests
tests/evidence/               the log that identified why earlier versions failed
tools/                        build, install, lint, symbol and mutation tools
docs/architecture.md          how the mod works
docs/xaml-injection-plan.md   the design record for embedding in the taskbar
docs/images/                  the screenshots in the README
CHANGES.md                    every change, with its evidence
DECISIONS.md                  settled decisions and why
```

## Records

Two files are kept up to date with the code:

* **[CHANGES.md](../CHANGES.md)** gets an entry for every functional change,
  newest first. It says what was wrong or missing, what changed, and how it was
  verified: which tests, which live checks, and what was not verified. A change
  that alters behaviour names the regression tests that pin it.
* **[DECISIONS.md](../DECISIONS.md)** records each settled design decision as a
  numbered entry with its reasoning. Read it before changing an area it covers.
  A change that goes against an entry updates the entry rather than quietly
  contradicting it.
