# Tray interface completion - 1.4.0 development

This work addresses the 2026-11-01 gap analysis. It retains the single Windhawk
translation unit, XAML thread ownership, shell replay semantics, and unload
lifetime rules. Direct taskbar dragging remains optional; Arrange supplies the
reliable movement and ordering path (decision 98).

| Gap | Implemented behavior | Evidence |
| --- | --- | --- |
| Chevron and empty handle | Focusable named buttons with Invoke and focus visuals | Test_TrayControlsAreInvokableButtons |
| Split Tray menu from keyboard | Ctrl+Shift+F10; application menu remains Shift+F10 | Key handlers; existing menu/callback tests |
| Live overflow | Existing cells update names/tooltips/pictures without replacement; steady pictures reused | Test_OpenOverflowUpdatesItsExistingCells; Test_OverflowReusesPicturesAndInheritsDirection |
| Reliable ordering | Arrange Move up/down, Ctrl+Up/Down, shared persisted order; no reorder while filtered | Test_ReorderPreservesAbsentIcons; private-desktop management tests |
| Floating keyboard entry | Ctrl+Alt+Shift+T cycles floating trays; Ctrl+Alt+T opens Arrange | Worker hotkey handling; private-desktop/UIA focus paths |
| Floating consistency | Shared order/hidden choices, native overflow actions, per-icon rule reset | Test_FloatingTraysUseRememberedOrderAndOverflow |
| Space and many trays | Embedded row budget, bounded flyout, resizable Arrange grid and scroll, keyboard scroll reveal | Test_TrayWidthProtectsTaskbarSpace; private-desktop display-bounds checks |
| Theme, direction, pointer input | Theme-aware hover/press colors; system contrast; inherited RTL; native pointer coordinates; hold without tap | Hosted XAML pointer/hold/direction tests; physical limits in UNFINISHED.md |
| Management and fallback | Search, individual Follow rules, tray labels and explanatory Tray status | Configuration-independent manager integration tests |
| Portability | Validated settings/state export/import, optional file picker, disabled-mod requirement, recovery and rollback | tests/regression/test_configuration.py, disposable HKCU registry |
| Performance | Coalesced wakeups, cached display geometry, picture reuse, timed/resource-bounded sustained probe | tests/performance/tray_stress.cpp |

## Verification

The recorded commands and measured results follow below. The build
includes regression tests at -O0 and -O2, private-desktop integration, hosted
XAML, configuration tests, syntax/symbol/settings/catalog checks and DLL exports.

## Performance acceptance

`powershell.exe -NoProfile -File tools/performance.ps1 -Runs 3` compiles the
production source in a real hosted XAML fixture. Each run sends 4,800 updates in
240 batches with 24 icons and an open overflow. Each batch must produce exactly
one queued refresh. Budgets: p95 refresh <=16.7 ms, maximum <=250 ms, and growth
of no more than 16 GDI, USER or kernel handles after warm-up. The process has a
30-second self-ending watchdog. It does not modify the user's live tray/state.

`-ObserveOnly` collects a historical comparison without applying new budgets;
it still enforces queue correctness. The baseline is local commit 5d6336a.
Comparisons use the same probe, compiler flags, host and three repeated runs.

## Deliberate boundaries

Windows owns tray 1's visual order/overflow. A fixed refresh frame cap is not
needed to claim the measured probe budgets; no universal CPU guarantee is made.
No physical touch, pen, ARM64 or full RTL/high-contrast Windows verification is
claimed. The configuration picker is a checkout tool, not an in-mod dialog.
No source change was made for the unrelated/unconfirmed stray dot.

## Recorded results (2026-11-01T06:12:55+00:00)

- `powershell.exe -NoProfile -File tools/build.ps1`: full build passed, including
  1,380 regression checks at both -O0 and -O2. The final source/help DLL rebuild
  (`-SkipTests`) also passed syntax, settings, symbols, catalog and export checks.
- `powershell.exe -NoProfile -File tools/coverage.ps1`: final suites passed:
  1,380 regression, 440 private-desktop integration, 377 hosted-XAML checks.
  Regions **85.41%**, branches **74.26%**, functions **90.50%**, lines **86.61%**.
  All native thresholds met. LLVM retains its known warning about seven
  functions compiled differently with and without XAML across the suites.
- `python tests/regression/test_configuration.py`: **4 tests passed**, including
  round trip, rejection before mutation, duplicate/enabled-mod rejection, and
  preservation of the installed retired diagnostic setting.
- structure scanner: **89.4/100**. It inspected Python tooling, not the C++ translation
  unit; testability was 15.4%, below its target. This is not full structure compliance.
- No mutation score was measured in this pass.

### Controlled refresh comparison

Same host, same probe and -O1 flags; baseline commit `5d6336a` preserved in a
build-only export. One warm-up per variant, followed by three alternating pairs.
Each run: 24 icons, an open overflow, 4,800 notification updates in 240 batches.
Every batch queued exactly one refresh in both variants.

| Run | Baseline p95 ms | Current p95 ms | Baseline CPU ms | Current CPU ms |
| --- | ---: | ---: | ---: | ---: |
| 1 | 5.811 | 0.543 | 1203.125 | 312.500 |
| 2 | 4.759 | 0.498 | 812.500 | 296.875 |
| 3 | 5.662 | 0.430 | 1421.875 | 343.750 |

Current maximum refresh was 1.210 ms or less. Each run in both variants grew by
3 GDI, 1 USER, and 1 kernel handle after warm-up, within the <=16 budgets.
Current p95 remained below 16.7 ms and maximum below 250 ms. This establishes
the hosted workload's result, not a universal Explorer/system performance claim.

### Live development deployment

- Catalog `split-tray` disabled; `local@split-tray` enabled.
- Loaded DLL: `local@split-tray_1.4.0_197991.dll`.
- Built and installed SHA-256: `07072B4A763309FDB7FABEC17384377B10D8BDD0D2245EB89CE490B92379520F`.
- Explorer PID 11016 retained; no Explorer restart.
- All 25 existing settings/state values matched the backup.
  No hidden-state value existed before deployment; none was lost.
- Live log: tray 2 attached to the taskbar and rebuilt with 7 shown icons.
- Explorer responded through the subsequent ten-second sample, using
  125 ms CPU. This is an observation only.
- Deployment backups and raw logs remain in this chat's `work/` folder.

### Stray dot observation

The dot persisted while the old development mod was disabled, without an
Explorer restart. Brandon later reported it gone before this 1.4.0 build was
installed. A later repaint associated with toggling may have cleared it, but
there is no evidence establishing Split Tray as its renderer. No speculative
pixel workaround was added.
