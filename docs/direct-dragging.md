# Direct dragging between trays

Version 1.4.2 includes target-aware transfers for primary, embedded and floating
trays. Vertical and horizontal movement both start a gesture. Dropping on the
desktop leaves placement unchanged. Moves use the same persisted placement and
shell replay path as Arrange; no application is asked to re-register.

## Input and ownership

The native NotifyIconView press hook observes application icons and always calls
the original handler. It identifies a source by a unique Shell_NotifyIconGetRect
hit among tracked shell-owned icons, using GUID or owner window plus ID.
Queries run outside the store mutex; ambiguous rectangles are rejected.
Windows keeps its own drag, reordering, pinning and overflow behavior.

A 16 ms worker timer exists only during a press, for at most 30 seconds. It
also covers embedded/overflow mouse sources when Explorer refuses or takes
XAML capture. Only serials, points and tray numbers cross threads. Release
queues a bounded internal drop record; the registered wake carries no input
pointers. Native capture must end before the source can be deleted. Retries
use a 32 ms timer, expire after 30 seconds, and stop on unload.

XAML targets are hit-tested on the taskbar thread using actual panel bounds
and rasterization scale. Floating targets use visible window rectangles.
The native primary target is TrayNotifyWnd, verified visible on this host at
(1290,1032)-(1920,1080). Disconnected targets, deleted icons, stale source trays,
Escape, pointer cancellation and unload do not commit a transfer.

Floating left clicks are deferred until release distinguishes click from drag.
Reordering within a floating panel updates shared persisted order. Right and
middle clicks continue to belong to the application. Embedded row reordering
still uses XAML capture; Arrange remains the dependable explicit order editor.

## Evidence and limits

The pre-fix hidden-island probe reproduced vertical movement becoming a click,
horizontal movement never leaving the row, and arbitrary off-row points being
saved as primary. The first vertical regression produced four failing checks.

Regression anchors cover two-axis thresholds, cancellation/timeout, native
identity ambiguity, lock-free shell queries, source/destination validation,
primary-to-secondary-to-extra-to-primary routing, capture deferral, bounded
queues, overflow click suppression, and actual panel/window target geometry.
Private-desktop input drives the floating window procedure without touching
the user's live shell.

The desktop automation tool lists Explorer folder windows but does not expose
taskbars as targetable windows. Live logs captured two primary/secondary round trips and embedded in-row
gestures after installation. Visual feedback confirmation is still pending;
native touch/pen transfers and other Windows builds/architectures remain manual checks.
Brandon confirmed the live transfers work. The follow-up feedback change carries
the icon and corrects the cursor over accepted receiving trays, as described below.

## Verified transfer results (1.4.1)

`tools/build.ps1` passed the full build. Final `tools/coverage.ps1` suites passed:
1,448 regression checks, 455 private-desktop integration checks, and 403
hosted-XAML checks. The final optimized regression run passed 1,448 checks.
`tools/build.ps1 -SkipTests` rebuilt the final 1.4.1 DLL with all lifecycle
exports, settings, source hygiene, catalog and eight native symbols checked.

Native coverage: regions 85.63%, branches 74.05%, functions 90.61%, lines
86.77%; all configured gates passed. LLVM retains the existing seven-function
mixed-suite mismatch warning. No mutation score was measured. The structure analyzer's tooling
scanner reports 89.4/100, does not inspect this C++ implementation, and fails
its Python-tool testability check (15.4%); this is not full structure compliance.

`tools/performance.ps1 -Runs 3` passed. Each run sent 4,800 notifications and
processed 240 refreshes. p95: 0.444, 0.475, 0.684 ms; max: 0.644, 0.732,
1.281 ms. GDI/USER/kernel handle growth was +3/+1/+1 on each run. These are
isolated update-probe results, not a universal Explorer-performance claim.

Installed `local@split-tray_1.4.1_816231.dll`, catalog disabled, with SHA256
`ABCE56E2BEBEACBB1161C9EE57C013BD1634AFE7B5DF4683387D58DF926F54A1`. Explorer retained PID 11016 and
its original start time; no restart. It remained responsive and used 281.25 ms
CPU in a ten-second observation.

All 21 existing settings were preserved. Display slots, last-seen state and
every previous order key were retained. Live drag input changed order and
added one explicit primary placement for the icon moved back. The strict
byte-for-byte state check flagged those live changes; a separate audit matched
the placement change to captured move logs rather than restoring over it.

Live logs confirm native hook installation and these transfers of the same
application icon: 06:52:48 primary -> tray 2; 06:52:53 tray 2 -> primary;
06:52:58 primary -> tray 2; 06:53:01 tray 2 -> primary (UTC, 2026-11-01).
Embedded reorder gestures also reached the production handler. Extra floating
trays were exercised on the private desktop; none was configured live.

Recorded 2026-11-01T06:54:56.260325+00:00.

## Drag image and accepted-target cursor (1.4.2)

A decorative layered window carries a copied source icon alongside the pointer.
The shared icon decoder preserves both alpha-channel icons and older masked
icons. Pixels are premultiplied; the image scales with monitor DPI and stays
inside the monitor at screen edges. It is transparent to input, cannot activate,
and owns no screen-DC drag lock. Windows supplies its existing image for every native-primary-source drag;
Split Tray adds its own image only for sources in its trays, including the return drag.

The existing press timer drives image movement. One coalesced hover wake reads
the latest pointer position on the taskbar thread and publishes target acceptance.
The 1.4.2/1.4.3 cursor-API experiments did not remove the visible denied sign.
Live diagnostics showed the primary-source cursor remained the stock arrow:
the sign is part of Windows' manual drag visual. The API hooks were removed.

In 1.4.4, SystemTray's BeginDrag supplies a projected NotifyIconDragVisual.
Split Tray retains only a weak Control reference on that input thread. After
Windows' DragMove, a known primary-source icon over an available secondary
tray receives the Normal visual state, replacing CantDropHere. Native internal
hit tests and DoDrop remain untouched. The uninstalled generic XAML
AcceptedOperation prototype was replaced after inspecting the native manager.
Owned return drags use an arrow throughout the gesture rather than setting a
denied cursor between targets. Final drop validation remains authoritative.

Release, Escape, capture cancellation, timeout and unload destroy the image,
release its copied icon, end feedback polling and restore the cursor. The
private-desktop regression first failed because no drag-image window existed.
It now checks the image, click-through/non-activation styles, twelve repeated
cancellations, bounded GDI/USER counts and disappearance at release.
Unit tests cover cursor scope, stale/coalesced hover,
image pixel scaling, premultiplication preservation and monitor-edge positioning.

The implementation uses the documented [layered-window renderer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)
and [cursor API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcursor).
The layered window's input transparency follows Microsoft's
[layered-window behavior](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows).

Final feedback validation and deployment results are recorded below after verification.

## Final drag-feedback verification (1.4.4)

Brandon confirmed "Gone in both directions" after testing the installed 1.4.4.
The live log recorded native visual capture and a successful CantDropHere to
Normal correction at 08:20:59.650 UTC. Return drags recorded the stock arrow
through secondary, intervening desktop and primary targets. Repeated round
trips still used the existing routing path.

`tools/build.ps1` passed 1,504 regression checks at -O0 and -O2, 487 private-
desktop integration checks, and 417 XAML checks. After exact symbol-name
validation and additional visual-capture/thread checks, the final hosted suite
passed 421 checks; `tools/build.ps1 -SkipTests` rebuilt the installed DLL.
`tools/coverage.ps1` passed: regions 85.49%, branches 74.12%, functions 89.89%,
lines 86.94% (LLVM's existing seven-function mixed-suite warning remains).
`tools/performance.ps1 -Runs 1` passed: 4,800 notifications, 240 refreshes,
p95 0.477 ms, max 0.958 ms, GDI/USER/kernel handle growth +3/+1/+1.
structure tooling fitness remains 89.4/100; its Python testability check fails at 15.4%
and it does not measure the C++ implementation. No full structure claim or mutation
score is made.

Installed local@split-tray_1.4.4_868648.dll, SHA256
1D0123AE08389C812F11EB95CB8C194A93EDE40F75D711920EB07160AD781DA5.
Explorer retained PID 11016, responded and used 93.75 ms CPU over ten seconds.
All 21 settings and four existing state entries matched immediately after
installation; later user reorder/transfer input intentionally changed state.
The catalog remains disabled and the development mod enabled.

## Reorder insertion line (1.4.5)

The vertical marker follows the existing in-tray reorder destination. Embedded
rows attach a composition SpriteVisual above the owned StackPanel; it is not
a XAML child, hit target, or extra slot. No icon or accessibility tree is
replaced. The line is one DIP wide and up to 20 DIP high, with a contrasting
light/dark colour or the system text colour in high contrast. Floating trays
paint the equivalent line at the destination cell, scaling width and height
with DPI and keeping the trailing edge inside the cell.

A click that has not crossed the drag threshold shows no line. Leaving the
source row hides it; re-entry shows the current position. Release, capture
loss, Escape/cancellation, another press, panel removal and unload clear it.
The global WinRT state is explicitly released during taskbar-thread cleanup.
The overlay adds no timer or idle refresh. Destination changes reuse the
existing pointer-move path; floating trays invalidate only when that preview
changes. Native primary tray rendering and cross-tray placement remain as
before. The line describes in-tray order, not a new cross-tray ordering policy.

Regression coverage checks layout-child count, both reorder directions,
visibility on leave/re-entry, release/capture-loss/unload removal, 96/192-DPI
geometry including row boundaries, and floating cancellation cleanup. Final
build/deployment results follow after verification.

## Fixed source ghost and release-only ordering (1.4.6)

After confirming the insertion line on 1.4.5, Brandon requested the primary
tray's fixed ghost source too. Embedded rows now leave the source at 40%
opacity in its original slot; the line alone previews order until mouse-up.
Only a valid release shifts cells and saves order. Capture loss or cancellation
keeps the original order, and an observed Escape remains cancelled even after
Escape is released before mouse-up. Cross-tray final validation observes that
same cancellation flag. Existing Arrange and keyboard ordering are unchanged.

The observer covers refused/stolen XAML capture and overflow source ghosts.
Release, cancellation, panel removal and unload restore the source. Opacity
comparisons tolerate XAML's float representation, avoiding repeated writes at
every observer tick. The moving image keeps full source opacity. Floating
trays retain release-only order and use GdiAlphaBlend with the shared decoded,
premultiplied icon pixels at alpha 102/255. A one-pixel rendering regression
checks a fully red source over black produces red 102 without changing source
pixels. Original primary tray ghosts remain Windows-owned.

The cancellation resource regression samples on the private tray's own UI
thread via a test-only thread hook. Main-thread samples were proven to observe
temporary allocations from concurrent panel paints: raw GDI17, quiescent15,
matching baseline15; USER18 stayed18 across twelve cancellations. The original
bounds and cycle count remain. This hook exists only in the isolated test
binary; the installed mod adds no SetWindowsHookEx hook.

## Final 1.4.6 verification

Brandon confirmed the insertion line matches and clears on 1.4.5, then confirmed
on 1.4.6 that the fixed dimmed source, insertion line, release-only ordering and
cleanup work. The earlier denied sign was confirmed gone in both directions
on 1.4.4 and remains corrected. Live 1.4.6 logs record completed reorders at
09:03:24.844 UTC with arrow feedback; no insertion/ghost errors were captured.

Observed commands/results on the final changes:

- `tools/build.ps1`: 1,523 regression checks at -O0 and -O2, 504 integration
  checks passed; the added Escape-before-release test then reproduced one XAML
  failure. After fixing that final drop guard, `run_drag_suite.ps1 -Suite xaml`
  passed all 461 checks and `tools/build.ps1 -SkipTests` returned Build OK.
- `tools/coverage.ps1`: all three final suites green (1,523 / 504 / 461).
  Regions 85.29%, branches 73.49%, functions 90.02%, lines 87.11%; all configured
  gates passed. LLVM's seven-function mixed-suite warning remains; no mutation
  score was measured.
- `tools/performance.ps1 -Runs 1`: 4,800 notifications, 240 refreshes, p95
  0.427 ms, max 0.662 ms, CPU 296.875 ms; GDI/USER/kernel growth +3/+1/+1.
  This is the isolated update-storm probe, not a universal performance claim.

Installed local@split-tray_1.4.6_642952.dll, SHA256
06F62C4754DDBEA1C6B25E3714BADF02E6440DA10738113B2A1E1494915EF7FE.
Explorer retained PID 11016 without restarting, responded, and used 203.125 ms
CPU over the observed ten seconds. All 21 settings and four pre-existing state
entries matched immediately after installation; later test reorders are user
changes. The catalog is disabled and development mod enabled. structure tooling
fitness remains 89.4/100 with its Python testability check failing at 15.4%; it does
not measure the C++ source and is not a full compliance claim.

No known open defect remains in these requested drag behaviors after the live
confirmation. The platform/hardware limits in UNFINISHED.md and the original
unproven freeze/stray-dot causality remain unchanged.
