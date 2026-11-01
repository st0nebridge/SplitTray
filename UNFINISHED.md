# Remaining verification and platform limits

The original 2026-11-01 Explorer freeze has no confirmed root cause. If it
recurs, capture Explorer thread stacks/wait chains while blocked, notification
arrival rates and refresh duration. Coalescing fixes a demonstrated burst
amplifier; it does not establish why Explorer froze.

The stray dot remained visible when the installed development mod was disabled
without restarting Explorer, as observed by Brandon. It does not require an
actively loaded Split Tray instance. Brandon later reported it gone before the new interface build was installed.
Toggling the old mod may have caused a later repaint; ownership is unproven.
No speculative pixel workaround was added.

Cross-tray mouse transfers now have a press-scoped observer, independent of
XAML capture, and route only to visible real trays. In-row embedded reordering
still depends on capture; Arrange remains the explicit fallback. Windows keeps
control of its primary ordering/pinning. Ambiguous native icon rectangles,
untracked icons, and system controls are deliberately left to Windows.

The desktop automation tool does not expose the taskbars as targetable windows.
Hosted-XAML and private-desktop tests exercise the drag paths. Live logs now
show two primary/secondary round trips and embedded reorder gestures; Brandon
confirmed those transfers work. Version 1.4.2's return image was confirmed working, but Brandon observed
denied feedback in both directions and an oversized/differently styled image.
Brandon confirmed the 1.4.3 actual-size/smooth image improved, but the denied
sign persisted. Diagnostics proved native source drags already used an arrow;
SystemTray draws its own CantDropHere state. Version 1.4.4 removes the API hooks
and corrects that presentation only for known valid targets. Brandon confirmed
the denied sign gone in both directions on 1.4.4. The generic XAML drag-event experiment was never installed.
Native touch/pen transfers and other Windows builds remain manual checks.
Extra floating trays and repeated feedback cleanup were tested on the private desktop.

Touch-hold state, pointer coordinates, keyboard/automation invocation, RTL
inheritance, and XAML updates have regression coverage. Physical touch/pen
hardware, a full RTL Windows installation, actual high-contrast theme switching,
and ARM64 runtime were not exercised. Those are manual/platform verification
limits, not claims of successful end-to-end tests. The live host supplies the
Windows build and installed taskbar-mod combination tested this session.

Performance results and acceptance budgets are in docs/interface-completion.md.
The isolated probe is not a measurement of native Explorer under every mod
combination. Refreshes are coalesced, but no fixed frame-rate limit is imposed;
all embedded trays are still visited in a refresh. Profiling can justify more
targeted rendering later.


The drag visual follow-up is verified through 1.4.6: Brandon confirmed the
denied sign gone both ways, the native-style insertion line matching/clearing,
and the fixed source ghost with release-only reorder working. Final build,
regression/integration/XAML suites, coverage gates and isolated storm probe are
green; details are in docs/direct-dragging.md. No open defect is known in that
requested follow-up. The hardware/platform and original-causality limits above
remain the applicable unverified areas.
