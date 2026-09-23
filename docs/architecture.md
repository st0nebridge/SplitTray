# How Split Tray works

This describes the mod as it is. The reasoning behind each choice, and the
evidence for it, is in [DECISIONS.md](../DECISIONS.md), referred to below by
number. The history of how it got here is in [CHANGES.md](../CHANGES.md).

## The message, not the drawing

An application shows a notification icon by calling `Shell_NotifyIcon`. That
function runs in the application's own process and does not call into the shell
through any API. It packs the icon into a fixed record and sends it to
Explorer's tray window:

```c
hTray = FindWindowW(L"Shell_TrayWnd", NULL);
COPYDATASTRUCT cds = { .dwData = 1, .cbData = 1484, .lpData = &record };
SendMessageTimeout(hTray, WM_COPYDATA, (WPARAM)nid.hWnd, (LPARAM)&cds);
```

Split Tray runs inside Explorer and subclasses `Shell_TrayWnd`. It therefore sees
every icon that any application adds, changes or removes, and decides for each
one whether Explorer gets to see it too (DECISIONS 1).

Earlier attempts at this project hooked `Shell_NotifyIconW` inside Explorer
instead. That can only ever catch Explorer's own icons, since other processes
call their own copy of the function. `tests/evidence/` holds the log that showed
it.

### The record

The layout was captured from the real `shell32` rather than read off published
structures (DECISIONS 2). The wire probe creates a private desktop, puts its own
`Shell_TrayWnd` there, and records what arrives. Every caller is normalised into
one shape. ANSI and Unicode callers arrive the same way, as do the V1 to V4
structure sizes and 32-bit and 64-bit processes (DECISIONS 3):

```
offset  size  field
0x000   4     signature, 0x34753423
0x004   4     message             NIM_ADD / MODIFY / DELETE / SETFOCUS / SETVERSION
0x008   4     nid.cbSize = 956    whatever size the caller passed
0x00C   4     nid.hWnd            32 bits; window handles are 32-bit safe
0x010   4     nid.uID
0x014   4     nid.uFlags
0x018   4     nid.uCallbackMessage
0x01C   4     nid.hIcon
0x020   256   nid.szTip[128]      always UTF-16
0x120   4     nid.dwState
0x124   4     nid.dwStateMask
0x128   512   nid.szInfo[256]
0x328   4     nid.uVersion / uTimeout
0x32C   128   nid.szInfoTitle[64]
0x3AC   4     nid.dwInfoFlags
0x3B0   16    nid.guidItem
0x3C0   4     nid.hBalloonIcon
0x3C4   520   the sender's full executable path
              1484 bytes in all
```

The executable path at the end is what the per-application rules match against.
No process handle is ever opened (DECISIONS 4).

Only a record of exactly this shape is read. Anything else, from a future
Windows that changes it, is passed to Explorer untouched and the mod does
nothing with it: fields read from the wrong places would give an icon the
wrong identity, or replay garbage into Explorer (DECISIONS 57).

### Asking where an icon is

`Shell_NotifyIconGetRect` uses the same window with `dwData = 3` and a 40-byte
record:

* the owner window at `0x10`
* the uID at `0x14`
* the GUID at `0x18`
* at `0x04`, which part is being asked for

shell32 asks for the size first (part 2), then the position (part 1). The tray
answers each one in the `LRESULT`, packed like a mouse position with signed
halves. A size of 0 means "no such icon". The evidence is in
`tests/probe/probe-rect-output-26100.txt`.

## Trays and routing

### Trays

Tray 1 is Explorer's own. `PlanTrays` works out the rest from the displays and
the settings:

* **Display trays.** Every display other than the primary one gets a tray,
  numbered from 2 and counted from the left.
* **Extra trays.** The extra trays from the settings follow, in the order they
  are listed.

A display tray exists only while its display is connected, so unplugging one
renumbers every tray after it, the extra trays included. An extra tray keeps
its number while its own display is missing, or while it is disabled, and is
marked unavailable, so the extra trays after it keep theirs (DECISIONS 53 and
56).

### Destinations

An icon's destination is a tray number, optionally with "and the primary tray
as well". `PlanFor` turns it into what actually happens to the icon:

| Destination | Explorer gets it | Split Tray draws it |
| --- | --- | --- |
| tray 1 (`primary`) | yes | no |
| tray N | **no** | in tray N |
| tray 2 and primary (`both`) | yes | in tray 2 |
| any tray that is not connected | yes | no |

An icon Explorer does not get is really gone from the main tray. The mod does
not pass the message on, and returns `TRUE`, which is what Explorer returns
(DECISIONS 6).

### Deciding and remembering

* **Where an icon starts.** A hand-made placement decides first; otherwise the
  first matching rule; otherwise the default tray (DECISIONS 29).
* **The placement key.** It is the icon's GUID if it has a real one; otherwise
  the executable's file name and the uID (DECISIONS 38). A window handle would
  change every time the application restarts, and the placement would be lost.
* **Deciding once.** The decision is made once per icon and then sticks for its
  lifetime (DECISIONS 5).
* **Deciding late.** An icon whose first message carries no executable path, a
  modify rather than an add, is decided when a message with the path arrives
  (DECISIONS 35).
* **Asking for the path.** If a message carries no path and the icon's tray
  depends on one, the mod asks Windows which program owns the icon's window.
  An application that only ever updates its icon never sends a path while
  Explorer has the icon (DECISIONS 63).

### Moving an icon later

Moving an icon later, or a changed rule or display, is applied by **replaying**
(DECISIONS 7, 8):

* **Into Explorer's tray.** The mod keeps every message about an icon folded
  into one complete record. It replays that record into Explorer as an add,
  drawn with the mod's own copy of the picture, then replays the icon's
  negotiated version (DECISIONS 49).
* **Out of Explorer's tray.** It replays a delete.
* **Between two of Split Tray's trays.** Nothing goes to Explorer at all.

A replay is made on Explorer's taskbar thread, by the mod's own subclass, so
it never re-enters the mod's handler. Nothing is queued: the icon store says
what to do (DECISIONS 58, 66):

* **A move says where the icon should be.** It changes the icon's
  `shellTarget` and nothing else, then wakes the taskbar's thread with a
  registered message to `Shell_TrayWnd`. The message carries nothing, because
  any process on the desktop can post it.
* **The taskbar's thread settles the difference** (`SettleShellIcons`). For
  each icon whose target differs from where Explorer has it
  (`forwardedToShell`), it builds the add or the delete from the icon as it is
  at that moment and hands it over. So an update that arrived while the move
  waited is in the add, and two moves in quick succession come to whatever the
  last one said. An icon its application removed in the meantime is not there
  to be added.
* **Explorer's answer is what is recorded.** Until then, an application's own
  messages are passed on by where the icon really is. A refused add is
  followed by a modify, which Explorer takes only for an icon it has already.
  A real refusal is asked again on the next ticks, three times in all, and the
  icon is then left to its application. Its own adds and modifies go to
  Explorer, as they would with no mod, and Explorer's answers are recorded.

## Drawing the trays

### Inside a taskbar

A display tray is embedded in that display's taskbar, next to the clock, where
the native tray would be. XAML diagnostics are one consumer per process and
another taskbar mod may already hold them (DECISIONS 24). So the mod hooks two
kinds of symbol instead:

* In `SystemTray.dll`, the `IconView` constructor, which every tray element
  passes through, hands the mod live XAML elements.
* In `taskbar.dll`, a taskbar window's `TaskbarHost` gives its `XamlRoot`
  (DECISIONS 26, 27). An element belongs to a taskbar when their `XamlRoot`s are
  the same object.

The element's offset inside a `TaskbarHost` is read out of the first
instructions of `TaskbarHost::FrameHeight`. If they are not the code the mod
knows, it does not guess: the tray floats instead (DECISIONS 61).

The hook only sees elements created after it is installed. So the mod also
walks down from each taskbar's root on a timer until its panel is in place
(DECISIONS 36). The walk anchors on a tray icon, which is below the row the
panel goes into, and never on the frame above it (DECISIONS 65). The panel goes into the row that holds the clock, which is
found by the names Explorer gives its containers (DECISIONS 25).

The panel's contents:

* **Cells.** Each cell holds an image of the icon. Its size is measured from the
  taskbar it is in, so displays at different scales each get their own sizes.
* **The chevron.** Icons past the limit go behind it, into a popup of their own
  (DECISIONS 45, 48).
* **Updating in place.** A cell is updated rather than rebuilt when only its
  picture or tooltip changes (DECISIONS 46).

Every cell carries its icon's **serial number**, and everything the cell does
(clicks, menus, reordering) finds the icon by that number. It is unique for as
long as the icon exists, even between two running copies of the same
application.

### Floating

A tray that cannot be embedded is a small layered window at a corner of its
display:

* an extra tray,
* a display tray while embedding is off, or
* a display tray on a display that has no taskbar.

An empty tray still shows one cell, a handle, so there is always somewhere to
click for the mod's menu.

## Clicks, menus and questions

* **Clicks.** A click on an icon goes to the application that owns it, over the
  tray callback protocol (`TrayCallbacksFor`). That is `(uID, message)` for
  version 0 to 3 icons, and the anchor point plus `(message, uID)` for
  version 4. From version 3, a left button-up is followed by `NIN_SELECT` and a
  right one by `WM_CONTEXTMENU`, as Explorer does (DECISIONS 62). The owner is
  first allowed to take the foreground, so its menu can open.
* **Menus.** Plain right-click belongs to the application. Shift+right-click
  opens Split Tray's own menu (DECISIONS 30), and so does any click on an empty
  tray's handle.
* **Arrange window.** A window of the mod's own with a list per tray, where
  icons are moved by dragging or by pressing a tray's number (DECISIONS 40).
* **Where is my icon?** Explorer cannot answer `Shell_NotifyIconGetRect` for an
  icon it does not have, and Tauri applications ignore every click on an icon
  whose position they cannot get. So the mod answers that question itself for
  icons in its trays, with where it drew them (DECISIONS 52).

## Threads

| Thread | Runs |
| --- | --- |
| Explorer's taskbar thread | the `Shell_TrayWnd` subclass, every taskbar's XAML (DECISIONS 37), replays into Explorer |
| the mod's tray thread | a hidden controller window, the floating trays, the arrange window, the 2-second timer |

The icon store is shared between the two threads under one mutex. Neither
thread calls into the other while holding it. The tray thread only posts to
Explorer's thread and never sends, so the two cannot deadlock.

Whatever either thread draws after releasing the lock, it draws from its own
copies of the icons (DECISIONS 60). The store destroys an icon's old picture on
Explorer's thread whenever the application changes it, and the tray thread's
watchdog destroys the picture of an icon whose application has gone.

Every application's tray message is handled on Explorer's taskbar thread, and
shell32 gives up on a tray that does not answer. So nothing slow runs there,
and frequent events are not logged (DECISIONS 44, 51).

## Starting and stopping

* **Attaching.** The tray thread is started first, and the mod attaches only
  once it is running. If it cannot start, `Wh_ModInit` fails with nothing
  attached, rather than leave icons going to trays nothing draws
  (DECISIONS 67). Windhawk loads the mod before Explorer has created its
  taskbar, so attaching to `Shell_TrayWnd` is retried on the timer, never
  attempted just once (DECISIONS 18).
* **Collecting existing icons.** Icons that exist before the mod is watching are
  collected by asking applications to re-register: the `TaskbarCreated`
  broadcast that Explorer itself sends after a restart. The mod sends it only
  when Explorer will not (DECISIONS 9, 50).
* **Loading into a running Explorer.** Installing, updating and switching the
  mod on all load it into an Explorer that already holds icons, including the
  ones an earlier load put back as it unloaded. So an icon new to the mod that
  goes to one of its trays is taken back from Explorer. A modify for an icon
  the mod has never seen added is answered with failure, as Explorer answers
  one for an icon it does not have, so its application adds it again, whole
  (DECISIONS 64).
* **Unloading.** In order (DECISIONS 59, 67):
  1. In `Wh_ModBeforeUninit`, while the hooks are still in place, the mod takes
     its panels out of the taskbars on Explorer's thread.
  2. It stops the tray thread and waits for it to end. That thread installs
     hooks of its own, and must not be doing so while Windhawk removes them.
     Its window classes belong to the mod's own module, and it unregisters all
     of them as it ends: Windows keeps a class after the module that registered
     it has gone, pointing at a window procedure that is no longer there.
  3. In `Wh_ModUninit`, it asks for every icon to be in Explorer's tray, and
     settles that at once on the taskbar's thread, with any move still waiting.
  4. It removes its subclass.

  If the tray thread does not end in time, the mod keeps its module loaded
  until Explorer exits, rather than unload code that thread is still running.
