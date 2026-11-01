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


## Catalog 1.4.6 update (2026-11-01)

PR https://github.com/ramensoftware/windhawk-mods/pull/6830 contains the exact
released source. The required /ai-review was requested; inspect its findings,
address verified issues, then use /ready-for-reviewer. Human review/merge and
catalog publication remain external steps before Windhawk offers the update.
Do not enable the catalog copy beside the active development copy while waiting.

All catalog validation and compatibility checks are green. Current label: waiting-for-ai-review. The bot result is the next required input before maintainer handoff.


## Overflow follow-up (development 1.4.7)

Capacity Show promotion, same-tray overflow returns, chevron/popup drop targets,
and missed-release refresh recovery are implemented and tested. Final coverage
and DLL build pass; installed without restart and with settings/state preserved.
Physical popup dragging, live Show behavior, touch/pen and ARM64 runtime still
need confirmation. Capacity limits are preserved; Show promotes an icon rather
than increasing the visible count. The specific historic Unsloth occurrences
were not captured, so no single cause is asserted for every occurrence.
This build is local; release tag1.4.6 and catalog PR6830 were not changed.

1.4.7 live confirmation received: both transfer directions and Show on the tray work. That requested overflow interaction is verified; touch/pen, ARM64 and longer-term recurrence remain outside the observed result.

Catalog PR6830 review returned at 2026-11-01T09:57:49Z: three requested changes remain before maintainer handoff: make global hotkeys configurable/opt-in; honor swapped physical mouse buttons in gesture polling; make the new native drag symbol hooks optional without disabling the existing IconView hook. Review: https://github.com/ramensoftware/windhawk-mods/pull/6830#issuecomment-6556420407. These are recorded follow-up work; /ready-for-reviewer has not been sent. The optional maxVisibleIcons setting-description correction is also relevant: its count applies to floating trays, and zero still retains the embedded width budget.

## 1.4.7 catalog review remediation

All three requested fixes and the visible-count description correction are implemented. Global shortcuts now opt in with Ctrl+Shift+F11/F12; swapped-button polling is mapped and floating capture owns releases; drag symbols are optional. Final validation/publication pending. Optional broad formatting, shared-order refactoring, delayed native icon lookup and sorting optimizations are deferred to avoid mixing unrelated behavior changes into this release.

## 1.4.7 release candidate — awaiting local installation decision

All review blockers are fixed. Development commits35e38da/ee61850, clean public candidate6b4cd05fbcb04dc63816f44f24603ef92617da45, local tagv1.4.7. Export build passed1599 checks at O0/O2, integration504 and XAML491. Coverage85.73/73.86/90.59/87.26; official catalog validator zero warnings; strict public tree/commit/tag and material scans clean. Negative control464 findings, exit1. Source SHA256ee06ac0d6d8f4fa9eb09ce08237dbd559b959ec87fc463a92d9711e6c18cd29f.

Export stress probe:4800 notifications,240 refreshes, p95 0.289ms, max0.508ms, GDI/USER/handle growth3/1/1. No universal performance claim.

Windows administrator launch returned operation canceled before the deployment script started. Installed DLL remains local@split-tray_1.4.7_100125.dll; no installed settings changed. A question is pending: retry the administrator prompt or publish without local reinstall. No remote push, release publication, PR metadata update or new review request has happened. Catalog checkout has verified single-file commit55e4d273178fa8dd9f67ff915efb5f77a8027f56 on existing branchcodex/split-tray-1.4.6, not pushed.

Resume files in the chat work directory: release-1.4.7/manifest.json, material/, export-final/, export-build.log, export-performance.log; review-pr-body.md; review-pr-reply.md (ends in /ai-review); deploy_review.ps1, verify_review.ps1, activate_review.ps1 (all preflighted). Keep catalog copy disabled. If retry is approved, run activate_review.ps1 elevated, verify result/preservation, then atomic push public branch plus tag, create draft release with source/checksum assets, download/verify, publish, push catalog commit, update PR6830 title/body and post review reply. Human review/merge remains external.

## 1.4.7 published and catalog update submitted

The installation retry succeeded as local@split-tray_1.4.7_885989.dll, DLL SHA25674C1A1470971304A9B48E70E37F0F38BE66C93C345F7BC3ABF0AF79DB82F2575. Explorer11016 stayed running and responsive;26/26 settings/state entries were preserved, catalog disabled, dev enabled; CPU265.625ms over10seconds.

Public main and annotated v1.4.7 point to6b4cd05fbcb04dc63816f44f24603ef92617da45, built from development ee61850. Published2026-11-01T20:08:19Z: https://github.com/st0nebridge/SplitTray/releases/tag/v1.4.7. Source/checksum assets were downloaded and verified before and after publication. Export build1599/O0+O2,504integration,491XAML checks green; all coverage gates passed; strict release scans clean.

PR6830 now contains exact released source at55e4d273178fa8dd9f67ff915efb5f77a8027f56. Updated title/body and replied to all three required findings, requested /ai-review: https://github.com/ramensoftware/windhawk-mods/pull/6830#issuecomment-6563226891. CI and the new review are pending. Do not use /ready-for-reviewer until the review is inspected and verified findings addressed; maintainer merge/catalog publication remain external.

Supersedes the earlier pending-installation/publication status. Optional broad formatting, shared-order refactoring, native lookup/sort optimization remain deferred; ARM64 hardware and physical touch/pen remain untested.

Latest catalog state: all five CI checks passed; waiting-for-ai-review on55e4d27. Inspect the new result when posted, then address verified findings or send /ready-for-reviewer. Publication and live installation are complete.


## Latest catalog review follow-up

The review on head55e4d27 reports no blocking issues and accepts the three
input/hook fixes: https://github.com/ramensoftware/windhawk-mods/pull/6830#issuecomment-6563281246.
Its optional follow-up is implemented locally on codex/review-polish-1.4.7:
strong insertion visual separated from weak/POD drag state, overflow content
detached during unload, owned Arrange handle reused, geometry comment restored,
and existing reveal/promote semantics explicitly documented in both READMEs.
Popup hover reads one presenter's screen rectangle and live cell transforms,
including viewport rejection, rather than querying automation bounds per cell.
Validation results are recorded in CHANGES.md under the catalog review cleanup.

These follow-up edits have not been installed, published, pushed to PR6830 or
submitted for human handoff. The installed and published release stays1.4.7,
and the catalog PR head stays55e4d27. A future publication needs a new release
version and the usual verified export; do not replace the existing1.4.7 assets
or tag. ARM64 hardware and physical touch/pen remain untested.


## Local icon-order correction

Supersedes the local-installation status above: review cleanup a5af7c6 was installed as local@split-tray_1.4.7_831545.dll. The user then reported unconditional move promotion resetting icons to the far left. Decision 115 restores ordinary tray moves to remembered order while preserving explicit Show and overflow-to-bar promotion. The corrected build is installed as local@split-tray_1.4.7_967769.dll, SHA256 3AF0AE3C267B3714603641BB73EA4A95979F0C5587BD17037AB131ABFC8D46BF. All 27 settings/state values were preserved; Explorer 11016 stayed responsive without restart. Public release and PR remain unchanged.
