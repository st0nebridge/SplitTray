# Plan: embedding the second tray in the taskbar's XAML tree

Status: **built; kept as the design record.** This is the plan the embedded
tray was built from, when the mod had one secondary display. Where the build
departed from it, `DECISIONS.md` (24 onward) says why, and
`docs/architecture.md` describes the result, which now embeds a tray in every
display's taskbar. The measurements below are from the development machine at
the time.

## What was asked for

The secondary tray should be embedded in the secondary monitor's taskbar the way
the native tray is embedded in the primary one, and behave the same — including
the overflow chevron, drag-to-reorder, and a context menu on the tray itself.

Two approaches were put forward. The **XAML injection** route was chosen over a
docked child window, with the fragility trade-off stated and accepted: it depends
on Explorer's internal XAML structure and is expected to need maintenance across
Windows feature updates. The docked-child-window design remains the fallback if a
Windows update makes injection untenable; it is described in the question history
and is a smaller change.

## Measured facts (Windows 10.0.26100, this machine)

Taken live via UI Automation — re-measure rather than trusting these after an
update.

| | Primary (`Shell_TrayWnd`, 96 DPI) | Secondary (`Shell_SecondaryTrayWnd`, 120 DPI) |
| --- | --- | --- |
| Taskbar rect | (0,1032)-(1920,1080), 48 tall | (-1920,1140)-(0,1200), 60 tall |
| Tray icon button | 32 x 38, top inset 5 | would be 40 x 48, top inset 6 |
| `AutomationId` per icon | `NotifyItemIcon` | — none exist — |
| Overflow chevron | `SystemTrayIcon`, name `Show Hidden Icons`, 32 x 38 at x=1305 | — none — |
| Clock | `SystemTrayIcon`, 73 x 38 at x=1793 | `SystemTrayIcon`, 91 x 48 at x=-158 |
| Notifications bell | `SystemTrayIcon`, 24 x 38 at x=1870 | `SystemTrayIcon`, 30 x 48 at x=-62 |

Native window children:

```
Shell_TrayWnd            -> DesktopWindowContentBridge (XAML) + TrayNotifyWnd (legacy, 1295..1920)
Shell_SecondaryTrayWnd   -> DesktopWindowContentBridge (XAML) only
```

The secondary taskbar has **no notification area at all** — not a hidden one, not
an empty one. This is not a matter of unhiding something: the tray region has to
be added.

## Mechanism

Explorer's taskbar is system XAML (`Windows.UI.Xaml`, not WinUI 3), hosted in a
`DesktopWindowXamlSource` island. The supported-by-convention way for a Windhawk
mod to reach it is XAML Diagnostics, which is what
`windows-11-taskbar-styler.wh.cpp` (present in `%ProgramData%\Windhawk\ModsSource`)
uses and is the reference to follow:

* `InitializeXamlDiagnosticsEx` to attach to the process's XAML.
* A `VisualTreeWatcher` implementing `IVisualTreeServiceCallback2`, advised via
  `IVisualTreeService3::AdviseVisualTreeChange`, to be told about elements as
  they are created.
* `IXamlDiagnostics::GetIInspectableFromHandle` to turn an element handle into a
  live `IInspectable`, then `winrt::try_as<>` into the XAML types.
* `IXamlDiagnosticsTestHooks` if available, or elements leak.

The toolchain already has what is needed: `winrt/Windows.UI.Xaml*.h` are present
in Windhawk's compiler include directory (399 WinRT headers), and the styler
compiles against `winrt/Windows.UI.Xaml.Controls.h`, `...Hosting.h`,
`...Media.Imaging.h`, `...Markup.h`, `...Shapes.h`.

## Progress

| Step | State |
| ---- | ----- |
| 1. XAML attachment | **done** - Section 10 of the mod |
| 2. Locate the insertion point | blocked on one live tree dump |
| 3. Icon surface (HICON -> XAML) | **done** - `ReadIconPixels` / `IconToBitmap` |
| 4. Core behaviour | not started |
| 5. Overflow chevron | not started |
| 6. Drag to reorder | not started |
| 7. Context menu | not started |

Step 3 was written ahead of step 2 because it does not depend on the tree's
shape. It handles the two historical icon forms: a 32-bit icon carries its own
alpha, an older one has none and its transparency lives in a separate 1bpp mask
where a set bit means transparent. `GetDIBits` reads the colour bitmap, and if
every alpha byte comes back zero the alpha is rebuilt from the mask. The result
is premultiplied, which is what `WriteableBitmap` expects - handing it straight
alpha leaves a dark fringe on every anti-aliased edge.

To collect the dump:

```powershell
toolsedeploy.ps1 -DumpXamlTree -Seconds 40
```

It prints the tray frame subtree of **both** taskbars: the primary one is the
reference, since it is the only one with a real notification area and therefore
the only place to see which element types hold tray icons and how they nest.

## Work breakdown

1. **XAML attachment.** `VisualTreeWatcher` + `InitializeXamlDiagnosticsEx`,
   scoped to elements whose island belongs to a `Shell_SecondaryTrayWnd` on the
   configured monitor. Reuse the styler's structure; do not reuse its scope.
2. **Locate the insertion point.** The container holding the secondary taskbar's
   clock and bell. Insert before the clock, matching the primary tray's order
   (chevron, then icons, then system icons, then clock, then bell).
3. **Icon surface.** HICON -> BGRA -> `WriteableBitmap`/`SoftwareBitmapSource`
   for each mirrored icon. Needs an `HICON` -> premultiplied-BGRA converter
   (`GetIconInfo` + `GetDIBits`), which the floating renderer did not need
   because it used `DrawIconEx`.
4. **Core behaviour.** Native cell metrics, hover and press visual states,
   `ToolTipService` tooltips, and pointer events routed into the existing
   `ForwardClick()` — the tray callback protocol work is already done and tested.
5. **Overflow chevron.** A `Button` plus `Flyout` holding the icons that do not
   fit, mirroring `Show Hidden Icons`.
6. **Drag to reorder.** Pointer capture and manipulation on the icon elements,
   with the resulting order persisted through `Wh_SetStringValue`.
7. **Context menu.** `MenuFlyout` on empty tray space: open settings, and move an
   icon between trays without going through Windhawk.

## What carries over unchanged

Everything below the presentation layer is already built and tested and is not
affected by this change:

* `Shell_TrayWnd` `WM_COPYDATA` interception and the wire parser
* the routing model, stickiness, and the replay/retract mechanism
* the icon store, GUID and `(hWnd, uID)` identity, partial `NIM_MODIFY` handling
* click forwarding for version 0 and version 4 icons
* monitor selection, availability fallback, live settings, clean unload

The floating-window renderer (`Section 6`) and its layout maths (`Section 4`) are
what the XAML surface replaces.

## Testing

The existing integration test drives the mod through a fake `Shell_TrayWnd` on a
private desktop and asserts on interception, routing and click forwarding — all
of which survive. It cannot cover the XAML surface, because that requires
Explorer's own taskbar. Plan:

* keep the integration test as the contract for everything in "carries over";
* add a live smoke check driven from `tools/` that, after install, asserts via UI
  Automation that the secondary taskbar now exposes N `NotifyItemIcon`-equivalent
  elements with the expected names — the same technique used to measure the
  metrics above, which makes the result checkable rather than eyeballed.
