// ==WindhawkMod==
// @id              split-tray
// @name            Split Tray
// @description     A notification area on every display's taskbar: choose, per application, which tray its icon shows in
// @version         1.0.0
// @author          Brandon Stonebridge
// @github          https://github.com/st0nebridge
// @homepage        https://github.com/st0nebridge/SplitTray
// @include         explorer.exe
// @compilerOptions -lcomctl32 -lgdi32 -luser32 -lole32 -loleaut32 -lruntimeobject -lshlwapi
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Split Tray

Windows 11 shows the notification area - the system tray - on the main
display only. Split Tray puts one on the taskbar of **every other display**,
and lets you choose which tray each application's icon lives in.

![Tray 2 in the second display's taskbar: the chevron, five icons, and the clock](https://raw.githubusercontent.com/st0nebridge/SplitTray/main/docs/images/tray-2.png)

## What you get

* A tray inside each extra display's taskbar, beside the clock, where the
  native one would be. Icons are clickable exactly as in the real tray: left,
  right, double and middle clicks, context menus, and tooltips.
* Per-application rules - "this program's icon goes to tray 3" - and a
  default for everything else.
* Move any icon by hand: **Shift+right-click** an icon in one of Split Tray's
  trays for its menu, or open **Arrange icons** to drag icons between every
  tray. Where you put an icon is remembered.
* An overflow chevron for the icons you would rather not see all the time.
* Unplug a display and its icons go back to the main tray; plug it back in
  and they return. With more than one of Split Tray's trays, see the notes
  below.
* Extra floating trays anywhere you like - for a display without a taskbar, or
  to try the mod out with one display.

## Trays are numbered

Tray 1 is Windows' own tray on the main display. Split Tray's trays are 2 and
up: one for each other display, left to right, then any extra trays in the
order they are listed in the settings. Rules and the menus use these numbers.

## How it works

An application puts an icon in the tray by calling `Shell_NotifyIcon`, which
sends a `WM_COPYDATA` message to Explorer's `Shell_TrayWnd`. Split Tray
subclasses that window, so it sees every icon from every process and can pass
each one on to the real tray, keep it for one of its own, or both. An icon sent
to another tray is really gone from the main one - it is not an overlay.

## Notes

* Icons that already existed when the mod loaded are collected by asking
  applications to re-register (the standard `TaskbarCreated` broadcast). A few
  ignore it; their icons appear the next time they update.
* Balloon notifications work only for icons in the main tray: Windows shows
  them only for icons it has. Send an application whose notifications matter
  to "both tray 1 and tray 2" rather than to tray 2 alone.
* Trays are numbered by where the displays are. Unplugging one renumbers every
  tray after it, the other displays' and the extra trays alike, until it is
  back. An icon shows in whichever tray has its number now, and waits in the
  main tray if no tray does.
* Screen readers cannot reach Split Tray's trays yet.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- defaultTray: primary
  $name: Default tray
  $description: >-
    Where an icon goes when no rule below matches it. Tray 1 is Windows' own;
    Split Tray's are 2 and up, one for each other display from left to right,
    then the extra trays.
  $options:
  - primary: Tray 1 (Windows' own)
  - secondary: Tray 2
  - tray3: Tray 3
  - tray4: Tray 4
  - tray5: Tray 5
  - tray6: Tray 6
  - both: Both tray 1 and tray 2
- perProcessRouting:
  - - exe: ""
      $name: Executable name
      $description: >-
        For example discord.exe - matched case-insensitively against the file
        name. Include a backslash to match against the full path instead.
    - destination: secondary
      $name: Tray
      $options:
      - primary: Tray 1 (Windows' own)
      - secondary: Tray 2
      - tray3: Tray 3
      - tray4: Tray 4
      - tray5: Tray 5
      - tray6: Tray 6
      - both: Both tray 1 and tray 2
  $name: Per-application rules
  $description: >-
    Rules are evaluated top to bottom; the first match wins. An icon you move
    by hand stays where you put it, whatever the rules say.
- extraTrays:
  - - display: ""
      $name: Display
      $description: >-
        primary, or a display number counted from the left (1 is the leftmost).
        Leave it empty for no extra tray.
    - corner: bottomLeft
      $name: Corner
      $options:
      - bottomRight: Bottom right
      - bottomLeft: Bottom left
      - topRight: Top right
      - topLeft: Top left
    - disabled: false
      $name: Disabled
      $description: >-
        Keep this entry but show no tray. It keeps its number, so the trays
        after it do not shift, and icons meant for it wait in tray 1.
  $name: Extra trays
  $description: >-
    Floating trays in addition to the one on each display's taskbar - for a
    display without a taskbar, a second tray on the same display, or trying
    Split Tray out with one display. They are numbered after the displays'
    trays.
- trayPosition: bottomRight
  $name: Floating position
  $description: >-
    Where a display's tray floats when it cannot sit in that display's
    taskbar: with embedding off, or on a display that shows no taskbar.
  $options:
  - bottomRight: Bottom right
  - bottomLeft: Bottom left
  - topRight: Top right
  - topLeft: Top left
- offsetX: 8
  $name: Horizontal offset
  $description: Distance in pixels of a floating tray from its corner of the work area.
- offsetY: 8
  $name: Vertical offset
  $description: Distance in pixels of a floating tray from its corner of the work area.
- iconSize: 16
  $name: Icon size
  $description: Icon size in a floating tray, in pixels at 100% scaling.
- cellSize: 28
  $name: Cell size
  $description: Size of the clickable square around each icon in a floating tray, in pixels at 100% scaling.
- maxColumns: 12
  $name: Icons per row
  $description: A floating tray wraps onto more rows once this many icons are shown.
- backgroundColor: "202020"
  $name: Background colour
  $description: A floating tray's background, as hex RRGGBB.
- opacity: 235
  $name: Opacity
  $description: A floating tray's opacity, 0 (invisible) to 255 (opaque).
- alwaysOnTop: true
  $name: Keep floating trays above other windows
- showTooltips: true
  $name: Show tooltips
- mirrorHiddenIcons: true
  $name: Show icons the application marked as hidden
  $description: >-
    Applications can ask for an icon to be hidden (NIS_HIDDEN). Turn this off
    to respect that in Split Tray's trays too.
- embedInTaskbar: true
  $name: Embed in each display's taskbar
  $description: >-
    Put each display's tray inside that display's taskbar, where the native
    tray sits, instead of drawing it as a floating panel. This reaches the
    taskbar by hooking symbols in Explorer's own DLLs, so it coexists with
    other taskbar mods. Turn it off to fall back to floating panels if a
    Windows update moves those symbols.
- maxVisibleIcons: 8
  $name: Icons before the overflow chevron
  $description: >-
    How many icons a tray in a taskbar shows before the rest move behind a
    "show hidden icons" chevron, as the native tray does. 0 shows all of them.
- dumpXamlTree: false
  $name: Log the taskbar's XAML tree
  $description: >-
    Diagnostic. Prints the structure of a taskbar's tray area to the mod log
    once, which is how the elements this mod attaches to are identified after
    a Windows update changes them. Leave it off otherwise: the dump holds up
    the taskbar for seconds while Explorer starts, and applications whose
    icons arrive in that time can lose them.
- repopulateOnLoad: true
  $name: Collect existing icons on load
  $description: >-
    Asks already-running applications to re-register their icons (the standard
    TaskbarCreated broadcast) so they can be routed. Turn this off if an
    application misbehaves when it is asked to re-register.
*/
// ==/WindhawkModSettings==

// ============================================================================
// Implementation
//
// Windhawk compiles a mod from a single translation unit, so this file is
// organised into clearly separated sections rather than separate modules:
//
//   1. Wire protocol   - parsing Explorer's Shell_TrayWnd WM_COPYDATA payload
//   2. Settings        - the model, and loading it from Windhawk
//   3. Routing         - resolving a notification to a destination
//   4. Monitors/layout - where the secondary tray goes and how big it is
//   5. Icon store      - the mirrored icons and their sticky routing decisions
//   6. Secondary tray  - the window, its thread, painting and hit testing
//   7. Click forwarding- the tray callback protocol back to the owning app
//   8. Interception    - the Shell_TrayWnd subclass, and replaying decisions
//   9. Lifecycle       - Wh_ModInit / AfterInit / SettingsChanged / Uninit
//  10. XAML           - attaching to Explorer's taskbar XAML tree
//
// Sections 1-4 are pure functions of their inputs and are covered by
// tests/regression, which compiles this file against stub Windhawk headers so
// that the tested code is literally the shipped code.
// ============================================================================

#include <windhawk_utils.h>

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Section 10 lives below, but the tray thread's retry timer in section 6
// drives it, so it is declared here.
#ifndef SPLITTRAY_NO_XAML
namespace SplitTrayXaml {
void EnsureTaskbarXamlHooked();
void OnIconStoreChanged();
// Must run on the taskbar's UI thread. Finds each display's tray row by walking
// down from its taskbar's XamlRoot, rather than waiting to be handed an element.
void TryAttachEmbeddedTray();
// Safe from any thread: marshals a redraw onto the taskbar's UI thread.
void RequestEmbeddedRefresh();
// Must run on the taskbar's UI thread. Where the icon with this serial is
// drawn, in screen pixels: its cell, or its tray's chevron when it is in the
// overflow.
bool IconScreenRect(uint64_t serial, RECT* out);
// Whether a display's tray still has to be put into its taskbar, so the tray
// thread keeps asking the taskbar's thread to try. Safe from any thread.
bool AnyDisplayTrayWaitingToEmbed();
// Must run on the taskbar's UI thread: takes every panel back out and lets go
// of every XAML object the mod holds, for unloading.
void RemoveEverything();
}  // namespace SplitTrayXaml
#endif

namespace SplitTray {

// ============================================================================
// Section 1 - Wire protocol
//
// Shell_NotifyIcon does not call into the shell through an API. shell32 builds a
// fixed-layout record and sends it to the tray window:
//
//     hTray = FindWindowW(L"Shell_TrayWnd", NULL);
//     COPYDATASTRUCT cds = { .dwData = 1, .cbData = 1484, .lpData = &record };
//     SendMessageTimeout(hTray, WM_COPYDATA, (WPARAM)nid.hWnd, (LPARAM)&cds);
//
// Every caller is normalised into one layout before it reaches the wire: ANSI
// callers are converted to UTF-16, and a caller passing any historical cbSize
// (V1/V2/V3/V4, 32-bit or 64-bit) arrives with cbSize == 956. Handle fields are
// 32 bits wide in both bitnesses, because USER handles are 32-bit safe.
//
// The offsets below were captured from the real shell32 on Windows 10.0.26100 by
// tests/probe/shell32_wire_probe.cpp, which puts its own Shell_TrayWnd on a
// private desktop and dumps what shell32 sends it. See
// tests/probe/probe-output-26100.txt for the raw evidence. Everything is bounds
// checked against the received cbData so that a future OS that grows or shrinks
// the record degrades instead of reading out of bounds.
// ============================================================================

// cds.dwData for a notification-area message. Other values carry appbar and
// in-proc-load requests, which this mod must pass through untouched.
constexpr ULONG_PTR kTrayCopyDataId = 1;
constexpr DWORD kTrayDataSignature = 0x34753423;

namespace wire {
constexpr size_t kSignature = 0x000;    // DWORD, kTrayDataSignature
constexpr size_t kMessage = 0x004;      // DWORD, NIM_*
constexpr size_t kNidCbSize = 0x008;    // DWORD, 956
constexpr size_t kOwnerWnd = 0x00C;     // DWORD, HWND of the icon owner
constexpr size_t kUID = 0x010;          // DWORD
constexpr size_t kFlags = 0x014;        // DWORD, NIF_*
constexpr size_t kCallbackMsg = 0x018;  // DWORD
constexpr size_t kIcon = 0x01C;         // DWORD, HICON
constexpr size_t kTip = 0x020;          // WCHAR[128]
constexpr size_t kTipChars = 128;
constexpr size_t kState = 0x120;      // DWORD, NIS_*
constexpr size_t kStateMask = 0x124;  // DWORD
[[maybe_unused]] constexpr size_t kInfo = 0x128;       // WCHAR[256]
constexpr size_t kVersion = 0x328;    // DWORD, uVersion / uTimeout
[[maybe_unused]] constexpr size_t kInfoTitle = 0x32C;  // WCHAR[64]
[[maybe_unused]] constexpr size_t kInfoFlags = 0x3AC;  // DWORD
constexpr size_t kGuid = 0x3B0;       // GUID
[[maybe_unused]] constexpr size_t kBalloonIcon = 0x3C0;  // DWORD, HICON
constexpr size_t kExePath = 0x3C4;      // WCHAR[260], owner's image path
constexpr size_t kExePathChars = 260;
constexpr size_t kTotalSize = 0x5CC;  // 1484

// The shortest prefix that still carries an identifiable notification.
constexpr size_t kMinUsableSize = kIcon + sizeof(DWORD);
constexpr DWORD kExpectedNidCbSize = 956;
}  // namespace wire

// A notification decoded off the wire. Handles are widened from the 32-bit wire
// fields; strings are copied out of the fixed-size, possibly unterminated
// buffers.
struct TrayNotification {
    DWORD message = 0;  // NIM_ADD / NIM_MODIFY / NIM_DELETE / ...
    HWND ownerWnd = nullptr;
    UINT uID = 0;
    UINT flags = 0;
    UINT callbackMessage = 0;
    HICON icon = nullptr;
    DWORD state = 0;
    DWORD stateMask = 0;
    DWORD version = 0;
    GUID guid = {};
    bool hasGuid = false;
    std::wstring tip;
    std::wstring exePath;
};

inline DWORD ReadDword(const BYTE* data, size_t offset) {
    DWORD value = 0;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

// Copies a fixed-width WCHAR field, tolerating a missing terminator.
inline std::wstring ReadFixedString(const BYTE* data,
                                    size_t offset,
                                    size_t maxChars) {
    const wchar_t* p = reinterpret_cast<const wchar_t*>(data + offset);
    size_t len = 0;
    while (len < maxChars && p[len] != L'\0') {
        len++;
    }
    return std::wstring(p, len);
}

// Returns false for anything that is not a tray notification, including appbar
// traffic and truncated or foreign payloads. `out` is only written on success.
bool ParseTrayNotification(ULONG_PTR copyDataId,
                           const void* payload,
                           size_t payloadSize,
                           TrayNotification* out) {
    if (copyDataId != kTrayCopyDataId || !payload || !out) {
        return false;
    }
    if (payloadSize < wire::kMinUsableSize) {
        return false;
    }

    const BYTE* data = static_cast<const BYTE*>(payload);
    if (ReadDword(data, wire::kSignature) != kTrayDataSignature) {
        return false;
    }

    // Only the record the offsets were captured from is read (DECISIONS 57). A
    // future Windows that changes it would have its fields decoded from the
    // wrong places - an identity that is not the icon's, a folded record that
    // replays garbage into Explorer - and staying within the buffer does not
    // make them right. So anything else is left to Explorer, untouched, and
    // the mod does nothing with it; the log says so once.
    const DWORD nidCbSize = payloadSize >= wire::kNidCbSize + sizeof(DWORD)
                                ? ReadDword(data, wire::kNidCbSize)
                                : 0;
    if (nidCbSize != wire::kExpectedNidCbSize || payloadSize != wire::kTotalSize) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            Wh_Log(L"unexpected tray record shape: cbData=%zu (expected %zu), "
                   L"nid.cbSize=%u (expected %u) - leaving these to Explorer",
                   payloadSize, wire::kTotalSize, nidCbSize,
                   wire::kExpectedNidCbSize);
        }
        return false;
    }

    TrayNotification n;
    n.message = ReadDword(data, wire::kMessage);
    // USER handles are 32-bit values zero-extended into 64-bit handles.
    n.ownerWnd =
        reinterpret_cast<HWND>(static_cast<ULONG_PTR>(ReadDword(data, wire::kOwnerWnd)));
    n.uID = ReadDword(data, wire::kUID);
    n.flags = ReadDword(data, wire::kFlags);
    n.callbackMessage = ReadDword(data, wire::kCallbackMsg);
    n.icon =
        reinterpret_cast<HICON>(static_cast<ULONG_PTR>(ReadDword(data, wire::kIcon)));

    if (payloadSize >= wire::kTip + wire::kTipChars * sizeof(wchar_t)) {
        n.tip = ReadFixedString(data, wire::kTip, wire::kTipChars);
    }
    if (payloadSize >= wire::kStateMask + sizeof(DWORD)) {
        n.state = ReadDword(data, wire::kState);
        n.stateMask = ReadDword(data, wire::kStateMask);
    }
    if (payloadSize >= wire::kVersion + sizeof(DWORD)) {
        n.version = ReadDword(data, wire::kVersion);
    }
    if (payloadSize >= wire::kGuid + sizeof(GUID)) {
        memcpy(&n.guid, data + wire::kGuid, sizeof(GUID));
        n.hasGuid = (n.flags & NIF_GUID) != 0;
    }
    if (payloadSize >= wire::kExePath + wire::kExePathChars * sizeof(wchar_t)) {
        n.exePath = ReadFixedString(data, wire::kExePath, wire::kExePathChars);
    }

    *out = std::move(n);
    return true;
}

inline void WriteDword(BYTE* data, size_t offset, DWORD value) {
    memcpy(data + offset, &value, sizeof(value));
}

// Rewrites dwMessage in a copy of a stored payload, so an add can be replayed as
// a delete (and vice versa) without reconstructing the record.
std::vector<BYTE> PayloadWithMessage(const std::vector<BYTE>& source, DWORD message) {
    std::vector<BYTE> copy = source;
    if (copy.size() >= wire::kMessage + sizeof(DWORD)) {
        WriteDword(copy.data(), wire::kMessage, message);
    }
    return copy;
}

// ---------------------------------------------------------------------------
// The record that recreates an icon
//
// An icon is put back into the shell - moved back from the secondary tray, or
// restored when the mod unloads - by replaying a record as NIM_ADD. That record
// used to be simply the last message seen, and the last message is usually a
// partial NIM_MODIFY: the Claude usage monitor, for one, changes its picture and
// its tooltip in separate modifies. Replayed as an add, that recreated an icon
// with a picture and nothing else - no callback, no tooltip, no executable path
// (a modify never carries one) - and the picture handle had been destroyed by
// then. Measured with real shell32: flags 0x2, callback 0, the icon dead. So
// every message is folded into one record instead, field by field, exactly as
// the shell applies a partial modify to what it already holds.
// ---------------------------------------------------------------------------

// Flags that describe the icon rather than one message about it. NIF_INFO is a
// balloon, and replaying it would show the notification again; NIF_REALTIME
// only qualifies a balloon.
constexpr UINT kLastingFlags =
    NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE | NIF_GUID | NIF_SHOWTIP;

void FoldTrayRecord(std::vector<BYTE>* state, const std::vector<BYTE>& incoming) {
    if (incoming.size() < wire::kMinUsableSize) {
        return;
    }
    const BYTE* in = incoming.data();
    const DWORD message = ReadDword(in, wire::kMessage);
    const DWORD flags = ReadDword(in, wire::kFlags);

    // An add describes the whole icon, so it starts the record afresh.
    if (message == NIM_ADD || state->size() != incoming.size()) {
        *state = incoming;
        WriteDword(state->data(), wire::kFlags, flags & kLastingFlags);
        return;
    }

    BYTE* out = state->data();
    auto copy = [&](size_t offset, size_t length) {
        if (incoming.size() >= offset + length) {
            memcpy(out + offset, in + offset, length);
        }
    };
    // Which icon it is. A GUID icon can be re-registered from a new window.
    copy(wire::kOwnerWnd, sizeof(DWORD));
    copy(wire::kUID, sizeof(DWORD));
    if (flags & NIF_MESSAGE) {
        copy(wire::kCallbackMsg, sizeof(DWORD));
    }
    if (flags & NIF_ICON) {
        copy(wire::kIcon, sizeof(DWORD));
    }
    if (flags & NIF_TIP) {
        copy(wire::kTip, wire::kTipChars * sizeof(wchar_t));
    }
    if ((flags & NIF_STATE) && incoming.size() >= wire::kStateMask + sizeof(DWORD)) {
        const DWORD mask = ReadDword(in, wire::kStateMask);
        WriteDword(out, wire::kState,
                   (ReadDword(out, wire::kState) & ~mask) |
                       (ReadDword(in, wire::kState) & mask));
        WriteDword(out, wire::kStateMask, ReadDword(out, wire::kStateMask) | mask);
    }
    if (flags & NIF_GUID) {
        copy(wire::kGuid, sizeof(GUID));
    }
    // Only an add carries the owner's path; a modify leaves the field empty,
    // and an empty one would cost the icon its identity with the shell.
    if (incoming.size() >= wire::kExePath + sizeof(wchar_t) &&
        (in[wire::kExePath] || in[wire::kExePath + 1])) {
        copy(wire::kExePath, wire::kExePathChars * sizeof(wchar_t));
    }
    WriteDword(out, wire::kFlags,
               (ReadDword(out, wire::kFlags) | flags) & kLastingFlags);
}

// The add that puts an icon back into the shell, drawn with `icon` - the mod's
// own copy - rather than whatever handle the application last sent, which it
// has usually destroyed by now. With no picture of its own to give, the mod
// gives none: the old handle may by then be another icon's (DECISIONS 72).
std::vector<BYTE> AddRecordFor(const std::vector<BYTE>& state, HICON icon) {
    std::vector<BYTE> record = PayloadWithMessage(state, NIM_ADD);
    if (record.size() >= wire::kFlags + sizeof(DWORD) &&
        record.size() >= wire::kIcon + sizeof(DWORD)) {
        const DWORD flags = ReadDword(record.data(), wire::kFlags);
        // USER handles are 32-bit values, as on the wire.
        WriteDword(record.data(), wire::kIcon,
                   static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(icon)));
        WriteDword(record.data(), wire::kFlags,
                   icon ? flags | NIF_ICON : flags & ~static_cast<DWORD>(NIF_ICON));
    }
    return record;
}

// How an add Explorer refused is asked about: the same icon as a modify, which
// Explorer takes only for an icon it has (DECISIONS 70). A balloon the add
// carried is left out, so asking does not show it, and so is the picture: it
// is asked later, and the handle the record names may be gone by then.
std::vector<BYTE> ProbeRecordFor(const std::vector<BYTE>& add) {
    std::vector<BYTE> record = PayloadWithMessage(add, NIM_MODIFY);
    if (record.size() >= wire::kFlags + sizeof(DWORD) &&
        record.size() >= wire::kIcon + sizeof(DWORD)) {
        WriteDword(record.data(), wire::kFlags,
                   ReadDword(record.data(), wire::kFlags) & kLastingFlags &
                       ~static_cast<DWORD>(NIF_ICON));
        WriteDword(record.data(), wire::kIcon, 0);
    }
    return record;
}

// A re-added icon starts at version 0, which changes the shape of every
// callback the application receives, so the version it asked for is replayed
// after the add.
std::vector<BYTE> SetVersionRecordFor(const std::vector<BYTE>& state, UINT version) {
    std::vector<BYTE> record = PayloadWithMessage(state, NIM_SETVERSION);
    if (record.size() >= wire::kVersion + sizeof(DWORD)) {
        WriteDword(record.data(), wire::kVersion, version);
    }
    return record;
}

// ---------------------------------------------------------------------------
// Shell_NotifyIconGetRect
//
// An application asks where its icon is on screen over the same window, with
// its own dwData. shell32 sends two messages and builds the RECT from the
// answers: the icon's size first, then its position, each packed like a mouse
// position in the LRESULT. A size of 0 is how the tray says it has no such
// icon, and the caller then gets E_FAIL. Captured by the wire probe, see
// tests/probe/probe-rect-output-26100.txt.
//
// It matters because Tauri's tray library - Telemachus and Desk Tray on this
// machine - asks before it handles any click on its icon, and drops the click
// when the answer is a failure. Explorer can only answer for icons it has.
// ---------------------------------------------------------------------------

constexpr ULONG_PTR kIconRectCopyDataId = 3;
constexpr DWORD kIconRectPosition = 1;
constexpr DWORD kIconRectSize = 2;

namespace rectwire {
constexpr size_t kSignature = 0x00;  // DWORD, kTrayDataSignature
constexpr size_t kPart = 0x04;       // DWORD, kIconRectPosition or kIconRectSize
constexpr size_t kOwnerWnd = 0x10;   // DWORD, HWND of the icon owner
constexpr size_t kUID = 0x14;        // DWORD
constexpr size_t kGuid = 0x18;       // GUID, all zeroes when not asked by GUID
}  // namespace rectwire

struct IconRectQuery {
    DWORD part = 0;
    HWND ownerWnd = nullptr;
    UINT uID = 0;
    GUID guid = {};
};

bool ParseIconRectQuery(ULONG_PTR copyDataId,
                        const void* payload,
                        size_t payloadSize,
                        IconRectQuery* out) {
    if (copyDataId != kIconRectCopyDataId || !payload || !out ||
        payloadSize < rectwire::kUID + sizeof(DWORD)) {
        return false;
    }
    const BYTE* data = static_cast<const BYTE*>(payload);
    if (ReadDword(data, rectwire::kSignature) != kTrayDataSignature) {
        return false;
    }
    IconRectQuery q;
    q.part = ReadDword(data, rectwire::kPart);
    if (q.part != kIconRectPosition && q.part != kIconRectSize) {
        return false;
    }
    q.ownerWnd = reinterpret_cast<HWND>(
        static_cast<ULONG_PTR>(ReadDword(data, rectwire::kOwnerWnd)));
    q.uID = ReadDword(data, rectwire::kUID);
    if (payloadSize >= rectwire::kGuid + sizeof(GUID)) {
        memcpy(&q.guid, data + rectwire::kGuid, sizeof(GUID));
    }
    *out = q;
    return true;
}

// Halves are signed, as a mouse position's are: a monitor left of the primary
// one has negative coordinates.
inline LRESULT PackScreenPair(int x, int y) {
    return static_cast<LRESULT>(static_cast<DWORD>(
        MAKELONG(static_cast<WORD>(x), static_cast<WORD>(y))));
}

LRESULT IconRectReply(const RECT& rect, DWORD part) {
    if (part == kIconRectSize) {
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        // Never report an empty rect as found: a 0 here reads as "no icon".
        if (width <= 0 || height <= 0) {
            return 0;
        }
        return PackScreenPair(width, height);
    }
    return PackScreenPair(rect.left, rect.top);
}

// ============================================================================
// Section 2 - Settings
// ============================================================================

// Where an icon belongs.
//
// Trays are numbered. Tray 1 is Explorer's own. Split Tray's trays are 2 and
// up: one for every display other than the primary one, left to right, then the
// extra trays from the settings in the order they are listed (PlanTrays).
// `alsoPrimary` is "both": shown in `tray` and kept in Explorer's tray as well.
//
// The names Primary, Secondary and Both are the destinations the mod had when
// it had exactly two trays; "secondary" is tray 2, so settings and remembered
// placements written then still mean what they meant.
struct Destination {
    int tray = 1;
    bool alsoPrimary = false;

    static const Destination Primary;
    static const Destination Secondary;
    static const Destination Both;
    static Destination Tray(int number) { return {number < 1 ? 1 : number, false}; }

    bool operator==(const Destination&) const = default;
};

const Destination Destination::Primary{1, false};
const Destination Destination::Secondary{2, false};
const Destination Destination::Both{2, true};

enum class Corner { BottomRight, BottomLeft, TopRight, TopLeft };

struct RoutingRule {
    std::wstring pattern;  // file name, or a path fragment if it has a backslash
    Destination destination = Destination::Secondary;
};

// A tray in addition to the one on every display's taskbar, drawn as a floating
// panel: for a display without a taskbar, a second tray on the same display, or
// trying the mod out with one display.
struct ExtraTray {
    int display = 0;  // 0 = the primary display, otherwise 1-based, left to right
    Corner corner = Corner::BottomLeft;
    // Off rather than on, so that a missing value - which reads as 0 - leaves
    // the tray on: an entry saved before the switch existed has none.
    bool disabled = false;
};

struct Settings {
    Destination defaultTray = Destination::Primary;
    std::vector<RoutingRule> rules;
    std::vector<ExtraTray> extraTrays;

    Corner corner = Corner::BottomRight;
    int offsetX = 8;
    int offsetY = 8;
    int iconSize = 16;
    int cellSize = 28;
    int maxColumns = 12;
    COLORREF background = RGB(0x20, 0x20, 0x20);
    int opacity = 235;
    bool alwaysOnTop = true;
    bool showTooltips = true;
    bool mirrorHiddenIcons = true;
    bool repopulateOnLoad = true;
    bool embedInTaskbar = true;
    bool dumpXamlTree = false;
    int maxVisibleIcons = 8;
};

// For the log: "primary", "tray 3", or "tray 2 and primary".
std::wstring DestinationName(Destination destination) {
    if (destination.tray <= 1) {
        return L"primary";
    }
    std::wstring name = L"tray " + std::to_wstring(destination.tray);
    if (destination.alsoPrimary) {
        name += L" and primary";
    }
    return name;
}

// A number made only of digits, or -1.
int ParseTrayNumber(std::wstring_view digits) {
    if (digits.empty() || digits.size() > 3) {
        return -1;
    }
    int number = 0;
    for (wchar_t c : digits) {
        if (c < L'0' || c > L'9') {
            return -1;
        }
        number = number * 10 + (c - L'0');
    }
    return number;
}

// The settings' names: primary, secondary (tray 2), both (tray 2 and the
// primary tray), and trayN for any tray by number.
Destination ParseDestination(PCWSTR value, Destination fallback) {
    if (!value) {
        return fallback;
    }
    const std::wstring_view text(value);
    if (text == L"primary") {
        return Destination::Primary;
    }
    if (text == L"secondary") {
        return Destination::Secondary;
    }
    if (text == L"both") {
        return Destination::Both;
    }
    if (text.rfind(L"tray", 0) == 0) {
        const int number = ParseTrayNumber(text.substr(4));
        if (number >= 1) {
            return Destination::Tray(number);
        }
    }
    return fallback;
}

Corner ParseCorner(PCWSTR value) {
    if (value) {
        if (wcscmp(value, L"bottomLeft") == 0) {
            return Corner::BottomLeft;
        }
        if (wcscmp(value, L"topRight") == 0) {
            return Corner::TopRight;
        }
        if (wcscmp(value, L"topLeft") == 0) {
            return Corner::TopLeft;
        }
    }
    return Corner::BottomRight;
}

// Accepts RRGGBB, with or without a leading '#'. Returns `fallback` on anything
// it cannot read, so a typo cannot make the tray invisible.
COLORREF ParseHexColor(PCWSTR value, COLORREF fallback) {
    if (!value) {
        return fallback;
    }
    if (*value == L'#') {
        value++;
    }
    unsigned components[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        unsigned v = 0;
        for (int d = 0; d < 2; d++) {
            wchar_t c = value[i * 2 + d];
            unsigned digit;
            if (c >= L'0' && c <= L'9') {
                digit = static_cast<unsigned>(c - L'0');
            } else if (c >= L'a' && c <= L'f') {
                digit = static_cast<unsigned>(c - L'a') + 10;
            } else if (c >= L'A' && c <= L'F') {
                digit = static_cast<unsigned>(c - L'A') + 10;
            } else {
                return fallback;
            }
            v = v * 16 + digit;
        }
        components[i] = v;
    }
    return RGB(components[0], components[1], components[2]);
}

int Clamp(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

Settings LoadSettings() {
    Settings s;

    s.defaultTray = ParseDestination(
        WindhawkUtils::StringSetting::make(L"defaultTray"), Destination::Primary);

    // The display is text so an empty one can end the list, the way an empty
    // executable ends the routing rules: "primary", or a display number.
    for (int i = 0; i <= 16; i++) {
        auto display = WindhawkUtils::StringSetting::make(L"extraTrays[%d].display", i);
        if (!display.get() || !*display.get()) {
            break;
        }
        ExtraTray extra;
        const std::wstring_view text(display.get());
        extra.display = (text == L"primary") ? 0 : ParseTrayNumber(text);
        extra.corner = ParseCorner(
            WindhawkUtils::StringSetting::make(L"extraTrays[%d].corner", i));
        extra.disabled = Wh_GetIntSetting(L"extraTrays[%d].disabled", i) != 0;
        s.extraTrays.push_back(extra);
    }

    for (int i = 0;; i++) {
        auto exe = WindhawkUtils::StringSetting::make(L"perProcessRouting[%d].exe", i);
        if (!exe.get() || !*exe.get()) {
            break;
        }
        auto dest = WindhawkUtils::StringSetting::make(
            L"perProcessRouting[%d].destination", i);
        s.rules.push_back({exe.get(), ParseDestination(dest, Destination::Secondary)});
        if (i > 256) {  // defensive: never spin on a malformed settings blob
            break;
        }
    }

    // What was actually read, not what was meant to be there. A rule that is in
    // the registry but not in the mod's hands looks exactly like a rule that
    // does not match, and the two were indistinguishable in the log.
    Wh_Log(L"settings: defaultTray=%s, %zu per-process rule(s), %zu extra tray(s)",
           DestinationName(s.defaultTray).c_str(), s.rules.size(),
           s.extraTrays.size());
    for (size_t i = 0; i < s.rules.size(); i++) {
        Wh_Log(L"settings:   rule %zu: '%s' -> %s", i, s.rules[i].pattern.c_str(),
               DestinationName(s.rules[i].destination).c_str());
    }

    s.corner = ParseCorner(WindhawkUtils::StringSetting::make(L"trayPosition"));
    s.offsetX = Clamp(Wh_GetIntSetting(L"offsetX"), -4096, 4096);
    s.offsetY = Clamp(Wh_GetIntSetting(L"offsetY"), -4096, 4096);
    s.iconSize = Clamp(Wh_GetIntSetting(L"iconSize"), 8, 128);
    s.cellSize = Clamp(Wh_GetIntSetting(L"cellSize"), s.iconSize + 2, 256);
    s.maxColumns = Clamp(Wh_GetIntSetting(L"maxColumns"), 1, 64);
    s.background = ParseHexColor(WindhawkUtils::StringSetting::make(L"backgroundColor"),
                                 RGB(0x20, 0x20, 0x20));
    s.opacity = Clamp(Wh_GetIntSetting(L"opacity"), 16, 255);
    s.alwaysOnTop = Wh_GetIntSetting(L"alwaysOnTop") != 0;
    s.showTooltips = Wh_GetIntSetting(L"showTooltips") != 0;
    s.mirrorHiddenIcons = Wh_GetIntSetting(L"mirrorHiddenIcons") != 0;
    s.repopulateOnLoad = Wh_GetIntSetting(L"repopulateOnLoad") != 0;
    s.embedInTaskbar = Wh_GetIntSetting(L"embedInTaskbar") != 0;
    s.dumpXamlTree = Wh_GetIntSetting(L"dumpXamlTree") != 0;
    s.maxVisibleIcons = Clamp(Wh_GetIntSetting(L"maxVisibleIcons"), 0, 64);

    return s;
}

// ============================================================================
// Section 3 - Routing
// ============================================================================

// Returns the substring after the last path separator.
std::wstring_view FileNameOf(std::wstring_view path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring_view::npos ? path : path.substr(pos + 1);
}

bool EqualsInsensitive(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (towlower(a[i]) != towlower(b[i])) {
            return false;
        }
    }
    return true;
}

bool ContainsInsensitive(std::wstring_view haystack, std::wstring_view needle) {
    if (needle.empty() || needle.size() > haystack.size()) {
        return false;
    }
    for (size_t i = 0; i + needle.size() <= haystack.size(); i++) {
        if (EqualsInsensitive(haystack.substr(i, needle.size()), needle)) {
            return true;
        }
    }
    return false;
}

// A rule with a separator in it matches anywhere in the full path; otherwise it
// matches the executable's file name exactly. First match wins.
Destination ResolveDestination(const Settings& settings, std::wstring_view exePath) {
    const std::wstring_view fileName = FileNameOf(exePath);
    for (const auto& rule : settings.rules) {
        if (rule.pattern.empty()) {
            continue;
        }
        const bool isPathPattern =
            rule.pattern.find(L'\\') != std::wstring::npos ||
            rule.pattern.find(L'/') != std::wstring::npos;
        if (isPathPattern ? ContainsInsensitive(exePath, rule.pattern)
                          : EqualsInsensitive(fileName, rule.pattern)) {
            return rule.destination;
        }
    }
    return settings.defaultTray;
}

// What the mod actually does with a notification, once it is known whether the
// destination tray exists right now. Requirement: an icon whose tray is missing
// - its display unplugged, asleep, or never there - behaves as `primary`, and
// goes back when the tray does.
struct RoutingPlan {
    bool forwardToShell = true;  // let the real tray see it
    bool mirror = false;         // show it in one of Split Tray's trays
    int tray = 0;                // which one, when mirrored
};

RoutingPlan PlanFor(Destination destination, bool trayAvailable) {
    if (destination.tray <= 1 || !trayAvailable) {
        return {true, false, 0};
    }
    return {destination.alsoPrimary, true, destination.tray};
}

// ============================================================================
// Section 4 - Monitors and layout
// ============================================================================

struct MonitorInfoEntry {
    HMONITOR handle = nullptr;
    RECT workArea = {};
    bool primary = false;
    UINT dpi = 96;
};

UINT GetMonitorDpi(HMONITOR monitor) {
    using GetDpiForMonitorProc = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static GetDpiForMonitorProc proc = []() -> GetDpiForMonitorProc {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        return shcore ? reinterpret_cast<GetDpiForMonitorProc>(
                            GetProcAddress(shcore, "GetDpiForMonitor"))
                      : nullptr;
    }();
    UINT dpiX = 96, dpiY = 96;
    if (proc && proc(monitor, 0 /* MDT_EFFECTIVE_DPI */, &dpiX, &dpiY) == S_OK) {
        return dpiX;
    }
    return 96;
}

BOOL CALLBACK CollectMonitorProc(HMONITOR monitor, HDC, LPRECT, LPARAM param) {
    auto* list = reinterpret_cast<std::vector<MonitorInfoEntry>*>(param);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfoW(monitor, &mi)) {
        MonitorInfoEntry entry;
        entry.handle = monitor;
        entry.workArea = mi.rcWork;
        entry.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        entry.dpi = GetMonitorDpi(monitor);
        list->push_back(entry);
    }
    return TRUE;
}

std::vector<MonitorInfoEntry> EnumerateMonitors() {
    std::vector<MonitorInfoEntry> list;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitorProc,
                        reinterpret_cast<LPARAM>(&list));
    // EnumDisplayMonitors order is not documented as stable; sorting by position
    // gives the user an index that matches how the displays are arranged.
    std::sort(list.begin(), list.end(),
              [](const MonitorInfoEntry& a, const MonitorInfoEntry& b) {
                  if (a.workArea.left != b.workArea.left) {
                      return a.workArea.left < b.workArea.left;
                  }
                  return a.workArea.top < b.workArea.top;
              });
    return list;
}

// A display named in the settings: 0 is the primary one, otherwise a 1-based
// index over the displays sorted left to right (EnumerateMonitors). False when
// there is no such display, which is the signal to fall back to the primary
// tray.
bool ResolveDisplay(const std::vector<MonitorInfoEntry>& monitors,
                    int display,
                    MonitorInfoEntry* out) {
    if (display == 0) {
        for (const auto& m : monitors) {
            if (m.primary) {
                *out = m;
                return true;
            }
        }
        return false;
    }
    if (display < 0 || static_cast<size_t>(display) > monitors.size()) {
        return false;
    }
    *out = monitors[static_cast<size_t>(display) - 1];
    return true;
}

// One of Split Tray's trays.
struct TrayTarget {
    int number = 0;           // 2 and up; tray 1 is Explorer's own
    bool forDisplay = false;  // the tray of a non-primary display, not an extra
    bool available = false;   // its display is connected, and it is not disabled
    bool disabled = false;    // an extra tray the user switched off
    MonitorInfoEntry monitor;
    Corner corner = Corner::BottomRight;  // where it goes when it floats
};

// Which trays there are, in number order.
//
// Every display other than the primary one gets a tray, left to right, then each
// extra tray from the settings. A display tray only exists while its display
// does, so unplugging one renumbers the trays after it. An extra tray keeps its
// number while its display is missing or it is disabled, and is marked
// unavailable, so the extras after it do not shift. With one display and no
// extras there are none, and every icon stays in the primary tray.
std::vector<TrayTarget> PlanTrays(const std::vector<MonitorInfoEntry>& monitors,
                                  const Settings& settings) {
    std::vector<TrayTarget> trays;
    for (const auto& monitor : monitors) {
        if (monitor.primary) {
            continue;
        }
        TrayTarget tray;
        tray.number = static_cast<int>(trays.size()) + 2;
        tray.forDisplay = true;
        tray.available = true;
        tray.monitor = monitor;
        tray.corner = settings.corner;
        trays.push_back(tray);
    }
    for (const auto& extra : settings.extraTrays) {
        TrayTarget tray;
        tray.number = static_cast<int>(trays.size()) + 2;
        tray.corner = extra.corner;
        tray.disabled = extra.disabled;
        tray.available =
            ResolveDisplay(monitors, extra.display, &tray.monitor) && !extra.disabled;
        trays.push_back(tray);
    }
    return trays;
}

// Where a tray is, in words a menu can use: "display to the left", "floating,
// primary display". Displays have no names worth showing, and their numbers
// follow the arrangement rather than what Windows' own settings call them, so
// the direction from the primary display is what identifies one.
std::wstring DescribeTrayPlace(const TrayTarget& tray, const MonitorInfoEntry& primary) {
    if (tray.disabled) {
        return L"disabled";
    }
    std::wstring where;
    if (!tray.available) {
        where = L"display not connected";
    } else if (tray.monitor.primary) {
        where = L"primary display";
    } else {
        const RECT& a = tray.monitor.workArea;
        const RECT& p = primary.workArea;
        const LONG dx = (a.left + a.right) / 2 - (p.left + p.right) / 2;
        const LONG dy = (a.top + a.bottom) / 2 - (p.top + p.bottom) / 2;
        if (std::labs(dx) >= std::labs(dy)) {
            where = dx < 0 ? L"display to the left" : L"display to the right";
        } else {
            where = dy < 0 ? L"display above" : L"display below";
        }
    }
    return tray.forDisplay ? where : L"floating, " + where;
}

struct TrayLayout {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int cell = 28;
    int icon = 16;
    int columns = 0;
    int rows = 0;
};

int ScaleForDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

// Lays the icons out in rows of at most maxColumns, anchored to `corner` of the
// work area. Sizes are DPI-scaled for the target monitor.
TrayLayout ComputeLayout(const RECT& workArea,
                         int iconCount,
                         const Settings& settings,
                         UINT dpi,
                         Corner corner) {
    TrayLayout layout;
    layout.cell = ScaleForDpi(settings.cellSize, dpi);
    layout.icon = ScaleForDpi(settings.iconSize, dpi);

    if (iconCount <= 0) {
        return layout;
    }

    layout.columns = std::min(iconCount, settings.maxColumns);
    layout.rows = (iconCount + settings.maxColumns - 1) / settings.maxColumns;
    layout.width = layout.columns * layout.cell;
    layout.height = layout.rows * layout.cell;

    const int offsetX = ScaleForDpi(settings.offsetX, dpi);
    const int offsetY = ScaleForDpi(settings.offsetY, dpi);

    switch (corner) {
        case Corner::BottomRight:
            layout.x = workArea.right - offsetX - layout.width;
            layout.y = workArea.bottom - offsetY - layout.height;
            break;
        case Corner::BottomLeft:
            layout.x = workArea.left + offsetX;
            layout.y = workArea.bottom - offsetY - layout.height;
            break;
        case Corner::TopRight:
            layout.x = workArea.right - offsetX - layout.width;
            layout.y = workArea.top + offsetY;
            break;
        case Corner::TopLeft:
            layout.x = workArea.left + offsetX;
            layout.y = workArea.top + offsetY;
            break;
    }
    return layout;
}

// At the corner the settings choose for the display trays.
TrayLayout ComputeLayout(const RECT& workArea,
                         int iconCount,
                         const Settings& settings,
                         UINT dpi) {
    return ComputeLayout(workArea, iconCount, settings, dpi, settings.corner);
}

// Which icon index sits under a client point, or -1. Kept next to the layout so
// painting and hit testing cannot drift apart.
int HitTestCell(const TrayLayout& layout, int clientX, int clientY, int iconCount) {
    if (layout.cell <= 0 || iconCount <= 0) {
        return -1;
    }
    if (clientX < 0 || clientY < 0 || clientX >= layout.width ||
        clientY >= layout.height) {
        return -1;
    }
    const int column = clientX / layout.cell;
    const int row = clientY / layout.cell;
    const int index = row * layout.columns + column;
    return index < iconCount ? index : -1;
}

// Where the icon at `index` is painted, in screen coordinates. The inverse of
// HitTestCell, and next to it for the same reason.
bool CellScreenRect(const TrayLayout& layout, int index, int iconCount, RECT* out) {
    if (layout.cell <= 0 || layout.columns <= 0 || index < 0 || index >= iconCount) {
        return false;
    }
    const int column = index % layout.columns;
    const int row = index / layout.columns;
    if (row >= layout.rows) {
        return false;
    }
    out->left = layout.x + column * layout.cell;
    out->top = layout.y + row * layout.cell;
    out->right = out->left + layout.cell;
    out->bottom = out->top + layout.cell;
    return true;
}

// Bounds measured inside a XAML island - device-independent pixels from its
// top-left corner - in screen pixels, given where the island's window is.
// Each edge is rounded on its own, so a row of cells tiles without gaps.
RECT IslandBoundsToScreen(POINT islandOrigin,
                          double x,
                          double y,
                          double width,
                          double height,
                          double scale) {
    RECT r;
    r.left = islandOrigin.x + static_cast<LONG>(std::lround(x * scale));
    r.top = islandOrigin.y + static_cast<LONG>(std::lround(y * scale));
    r.right = islandOrigin.x + static_cast<LONG>(std::lround((x + width) * scale));
    r.bottom = islandOrigin.y + static_cast<LONG>(std::lround((y + height) * scale));
    return r;
}

// ============================================================================
// Section 5 - Icon store
// ============================================================================

// An icon handle that is destroyed with its owner.
//
// For whatever holds a picture once the store's lock is released: a snapshot
// the mod's own thread draws, a replay on its way to Explorer. The store
// destroys the picture it replaces - on the taskbar's thread, whenever an
// application changes its icon - so a bare handle copied out of it can be gone
// by the time it is used (DECISIONS 60).
class OwnedIcon {
public:
    OwnedIcon() = default;
    explicit OwnedIcon(HICON icon) : icon_(icon) {}
    OwnedIcon(OwnedIcon&& other) noexcept : icon_(std::exchange(other.icon_, nullptr)) {}
    OwnedIcon& operator=(OwnedIcon&& other) noexcept {
        if (this != &other) {
            reset();
            icon_ = std::exchange(other.icon_, nullptr);
        }
        return *this;
    }
    OwnedIcon(const OwnedIcon&) = delete;
    OwnedIcon& operator=(const OwnedIcon&) = delete;
    ~OwnedIcon() { reset(); }

    // A copy of `icon` for this owner. Taken while the store's lock is held.
    static OwnedIcon CopyOf(HICON icon) {
        return OwnedIcon(icon ? CopyIcon(icon) : nullptr);
    }

    HICON get() const { return icon_; }
    void reset() {
        if (icon_) {
            DestroyIcon(icon_);
            icon_ = nullptr;
        }
    }

private:
    HICON icon_ = nullptr;
};

struct MirroredIcon {
    // Identity. Modern applications identify an icon by GUID and may send uID 0,
    // so both keys have to be supported.
    bool hasGuid = false;
    GUID guid = {};
    HWND ownerWnd = nullptr;
    UINT uID = 0;

    // Which icon this is while it lives, for anything that has to find it again
    // later - a tray cell, a menu item. Unique, unlike the placement key, which
    // two running copies of the same application share.
    //
    // Cells used to carry their position instead, and a click looked the icon
    // up at that position in the store. The trays draw icons in the user's
    // order, not the store's, so after a reorder, or an icon moved out and back,
    // a click on one icon could reach another application.
    uint64_t serial = 0;

    // Which of Split Tray's trays shows it, or 0 when none does.
    int shownTray = 0;

    HICON icon = nullptr;  // our own copy, owned by this mod
    std::wstring tip;
    std::wstring exePath;
    UINT callbackMessage = 0;
    UINT version = 0;
    DWORD state = 0;

    // The routing decision is taken once, at NIM_ADD, and reused for every later
    // message about this icon. Re-deciding mid-life would leave the primary tray
    // holding an icon it can no longer be told about.
    Destination destination = Destination::Primary;

    // Whether Explorer has the icon, as far as its answers say: to its
    // application's own adds and modifies, and to the mod's (DECISIONS 70).
    // Only the taskbar's thread changes it.
    bool forwardedToShell = true;

    // Whether the icon is meant to be with Explorer. A move changes this and
    // nothing else; the taskbar's thread then settles the difference, handing
    // Explorer the icon as it is by then (SettleShellIcons, DECISIONS 66).
    bool shellTarget = true;

    // How many times Explorer has refused to take the icon back, or its
    // version, since it was last asked to. After kShellAttempts it is left as
    // Explorer has it - one Explorer does not have, to its application to add
    // again (OwedToShell).
    int shellRefusals = 0;

    // Counts what its application has changed, so the taskbar's thread can
    // tell whether anything arrived while Explorer was taking the icon back.
    uint64_t revision = 0;

    // Whether Explorer has less of the icon than the store: something arrived
    // while Explorer was taking it back - swallowed, since Explorer did not
    // have it yet - or Explorer did not take its version. The taskbar's thread
    // then hands Explorer the whole record again (DECISIONS 71).
    bool shellBehind = false;

    // Whether Explorer refused its application's add and has not yet been
    // asked if that was because it has the icon already. The taskbar's next
    // round asks, after the messages waiting then (DECISIONS 70).
    bool shellUnconfirmed = false;

    // Whether that decision was made with anything to decide from.
    //
    // The first message the mod sees about an icon is not always its NIM_ADD.
    // An application that updates a live icon - SystemInformer's four graphs do
    // it every second - lands a NIM_MODIFY first, and a modify carries no
    // executable path, so the rules have nothing to match and the icon is filed
    // under the default tray. Sticky routing then made that guess permanent:
    // measured on this machine, uID 2 happened to arrive as an add and moved,
    // while 3, 5 and 14 arrived as modifies and never did, however the rule was
    // written. The decision is now deferred until a message actually carries the
    // path, and applied through the same replay the settings use.
    bool destinationDecided = false;

    // Whether its application has asked where it is (Shell_NotifyIconGetRect)
    // yet, so the answer is logged once rather than on every click.
    bool askedWhere = false;

    // Every message about the icon folded into one wire record (FoldTrayRecord),
    // kept so the mod can replay an add into the real tray, or retract one from
    // it, when routing or monitor availability changes. Not the last message:
    // that is usually a partial modify, and replaying one recreates an icon
    // with most of it missing.
    std::vector<BYTE> payload;
};

// A GUID of all zeroes is not an identity.
//
// NIF_GUID only says the caller filled the flag in, not that it put anything in
// the field, and applications do set the flag over an empty GUID. Treating that
// as an identity makes every such icon the same icon: on this machine all four
// of SystemInformer's icons and several of Explorer's own arrive that way, so
// the first one decided the tray for all of them and the other three inherited
// it as a "sticky" decision. Measured, not supposed - the log prints the GUID.
bool IsEmptyGuid(const GUID& guid) {
    static const GUID empty = {};
    return memcmp(&guid, &empty, sizeof(GUID)) == 0;
}

bool UsableGuid(bool hasGuid, const GUID& guid) {
    return hasGuid && !IsEmptyGuid(guid);
}

// Set when an icon's routing was settled after the fact, so the caller knows to
// run the same replay a settings change would. Not done inside the notification
// handler: decision 5 says routing changes are applied by replaying a stored
// payload, never by rewriting the decision mid-message.
std::atomic<bool> g_routingNeedsReapply{false};

bool SameIcon(const MirroredIcon& icon, const TrayNotification& n) {
    if (UsableGuid(icon.hasGuid, icon.guid) && UsableGuid(n.hasGuid, n.guid)) {
        return memcmp(&icon.guid, &n.guid, sizeof(GUID)) == 0;
    }
    return icon.ownerWnd == n.ownerWnd && icon.uID == n.uID;
}

// The same rule for Shell_NotifyIconGetRect's question, which has no NIF_GUID
// flag: a GUID that is filled in is the identity.
bool SameIcon(const MirroredIcon& icon, const IconRectQuery& query) {
    if (UsableGuid(icon.hasGuid, icon.guid) && !IsEmptyGuid(query.guid)) {
        return memcmp(&icon.guid, &query.guid, sizeof(GUID)) == 0;
    }
    return icon.ownerWnd == query.ownerWnd && icon.uID == query.uID;
}

// ---------------------------------------------------------------------------
// Per-icon placement
//
// Two trays, not a mirror. The per-process rules in section 3 are a starting
// position; anything the user moves by hand overrides them and is remembered.
//
// The key has to survive the owning application restarting, so it is the icon's
// GUID where it has one, otherwise the executable's file name and uID. A window
// handle would not do: it is a fresh handle every launch, so a placement keyed
// on it would be forgotten whenever the application came back - which is exactly
// the case this exists to handle.
// ---------------------------------------------------------------------------

constexpr PCWSTR kPlacementValue = L"iconPlacement";

std::map<std::wstring, Destination, std::less<>> g_placements;
bool g_placementsLoaded = false;

// How a placement is stored: 'p' and 's' for trays 1 and 2, as the two-tray
// versions wrote them, and the number for any tray after that. A hand-made
// placement is always a single tray; "both" only ever comes from the rules.
std::wstring PlacementCode(Destination destination) {
    if (destination.tray <= 1) {
        return L"p";
    }
    if (destination.tray == 2) {
        return L"s";
    }
    return std::to_wstring(destination.tray);
}

Destination ParsePlacementCode(std::wstring_view code) {
    if (code == L"s") {
        return Destination::Secondary;
    }
    const int number = ParseTrayNumber(code);
    return number >= 2 ? Destination::Tray(number) : Destination::Primary;
}

std::wstring FormatGuidKey(const GUID& guid) {
    WCHAR buffer[48];
    swprintf_s(buffer, L"{%08lX-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}",
               guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
               guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
               guid.Data4[6], guid.Data4[7]);
    return buffer;
}

std::wstring MakeStableKey(bool hasGuid,
                           const GUID& guid,
                           std::wstring_view exePath,
                           UINT uID) {
    // Same reasoning as SameIcon: an empty GUID would make one key for every
    // application that sets NIF_GUID without filling the field in, so moving one
    // of those icons would move all of them.
    if (UsableGuid(hasGuid, guid)) {
        return FormatGuidKey(guid);
    }
    std::wstring key{FileNameOf(exePath)};
    if (key.empty()) {
        key = exePath;
    }
    key += L'#';
    key += std::to_wstring(uID);
    return key;
}

std::wstring StableKeyOf(const MirroredIcon& icon) {
    return MakeStableKey(icon.hasGuid, icon.guid, icon.exePath, icon.uID);
}

std::wstring StableKeyOf(const TrayNotification& n) {
    return MakeStableKey(n.hasGuid, n.guid, n.exePath, n.uID);
}

void LoadPlacements() {
    if (g_placementsLoaded) {
        return;
    }
    g_placementsLoaded = true;

    WCHAR buffer[8192] = {};
    if (!Wh_GetStringValue(kPlacementValue, buffer, ARRAYSIZE(buffer))) {
        return;
    }
    std::wstring_view remaining(buffer);
    while (!remaining.empty()) {
        const size_t lineEnd = remaining.find(L'\n');
        std::wstring_view line = remaining.substr(0, lineEnd);
        const size_t split = line.rfind(L'=');
        if (split != std::wstring_view::npos && split + 1 < line.size()) {
            g_placements.emplace(std::wstring(line.substr(0, split)),
                                 ParsePlacementCode(line.substr(split + 1)));
        }
        if (lineEnd == std::wstring_view::npos) {
            break;
        }
        remaining.remove_prefix(lineEnd + 1);
    }
    Wh_Log(L"loaded %zu remembered icon placement(s)", g_placements.size());
}

void SavePlacements() {
    std::wstring joined;
    for (const auto& [key, destination] : g_placements) {
        if (joined.size() > 7900) {
            break;
        }
        if (!joined.empty()) {
            joined += L'\n';
        }
        joined += key;
        joined += L'=';
        joined += PlacementCode(destination);
    }
    Wh_SetStringValue(kPlacementValue, joined.c_str());
}

// The user's choice wins; the rules decide only what has never been moved.
Destination ResolvePlacement(const Settings& settings,
                             std::wstring_view key,
                             std::wstring_view exePath) {
    LoadPlacements();
    auto it = g_placements.find(key);
    if (it != g_placements.end()) {
        return it->second;
    }
    return ResolveDestination(settings, exePath);
}

void RememberPlacement(std::wstring_view key, Destination destination) {
    LoadPlacements();
    g_placements[std::wstring(key)] = destination;
    SavePlacements();
}

// ---------------------------------------------------------------------------
// Icons the user has put in the overflow
//
// Which icons are hidden was decided purely by count - the first maxVisibleIcons
// were shown and the rest went to the chevron - so there was no way to say "this
// one belongs in the popup and that one on the bar". That is most of what a
// notification area is for.
//
// Kept separate from placement: an icon is in a tray, and within the secondary
// tray it is on the bar or in the overflow. Two questions, two answers.
// ---------------------------------------------------------------------------

constexpr PCWSTR kHiddenValue = L"iconHidden";

std::set<std::wstring, std::less<>> g_hidden;
bool g_hiddenLoaded = false;
// Read on the taskbar thread when the tray is redrawn, written from there by the
// tray menu and from the mod's own thread by the arrange window. Its own lock,
// always taken innermost, so it cannot order against g_mutex the wrong way.
std::mutex g_hiddenMutex;

void LoadHidden() {
    if (g_hiddenLoaded) {
        return;
    }
    g_hiddenLoaded = true;

    WCHAR buffer[8192] = {};
    if (!Wh_GetStringValue(kHiddenValue, buffer, ARRAYSIZE(buffer))) {
        return;
    }
    std::wstring_view remaining(buffer);
    while (!remaining.empty()) {
        const size_t lineEnd = remaining.find(L'\n');
        std::wstring_view line = remaining.substr(0, lineEnd);
        if (!line.empty()) {
            g_hidden.emplace(line);
        }
        if (lineEnd == std::wstring_view::npos) {
            break;
        }
        remaining.remove_prefix(lineEnd + 1);
    }
    Wh_Log(L"loaded %zu icon(s) kept in the overflow", g_hidden.size());
}

void SaveHidden() {
    std::wstring joined;
    for (const auto& key : g_hidden) {
        if (joined.size() > 7900) {
            break;
        }
        if (!joined.empty()) {
            joined += L'\n';
        }
        joined += key;
    }
    Wh_SetStringValue(kHiddenValue, joined.c_str());
}

bool IsIconHidden(std::wstring_view key) {
    std::lock_guard<std::mutex> lock(g_hiddenMutex);
    LoadHidden();
    return g_hidden.find(key) != g_hidden.end();
}

void SetIconHidden(std::wstring_view key, bool hidden) {
    std::lock_guard<std::mutex> lock(g_hiddenMutex);
    LoadHidden();
    if (hidden) {
        g_hidden.emplace(key);
    } else {
        auto it = g_hidden.find(key);
        if (it != g_hidden.end()) {
            g_hidden.erase(it);
        }
    }
    SaveHidden();
}

// Which icons go on the bar and which into the overflow, by index into `keys`.
//
// The user's choice is applied first and the count second: an icon they hid is
// in the overflow however much room there is, and the row limit then only
// decides among the icons they want shown. Before this, the count was the only
// thing that decided, so the user could not choose at all. Pure, so that order
// of precedence is tested rather than read off the drawing code.
struct BarSplit {
    std::vector<size_t> shown;
    std::vector<size_t> overflow;
};

template <typename IsHidden>
BarSplit SplitBarAndOverflow(const std::vector<std::wstring>& keys,
                             IsHidden isHidden,
                             int maxVisible) {
    BarSplit split;
    for (size_t i = 0; i < keys.size(); i++) {
        (isHidden(keys[i]) ? split.overflow : split.shown).push_back(i);
    }
    if (maxVisible > 0 && split.shown.size() > static_cast<size_t>(maxVisible)) {
        split.overflow.insert(
            split.overflow.end(),
            split.shown.begin() + static_cast<ptrdiff_t>(maxVisible),
            split.shown.end());
        split.shown.resize(static_cast<size_t>(maxVisible));
    }
    return split;
}

void ShowAllHiddenIcons() {
    std::lock_guard<std::mutex> lock(g_hiddenMutex);
    LoadHidden();
    const size_t count = g_hidden.size();
    g_hidden.clear();
    SaveHidden();
    Wh_Log(L"brought %zu icon(s) back out of the overflow", count);
}

// Forget every hand-made placement, so the per-process rules decide again.
//
// There was no way back: once an icon had been moved, that choice outranked the
// rules for good, and the only way to undo it was to move every icon back one
// at a time - or to know where the mod keeps its storage.
void ForgetAllPlacements() {
    LoadPlacements();
    const size_t count = g_placements.size();
    g_placements.clear();
    SavePlacements();
    Wh_Log(L"forgot %zu remembered icon placement(s)", count);
}

// ============================================================================
// Shared state
//
// Two threads touch it: Explorer's taskbar thread (the Shell_TrayWnd subclass)
// and the mod's own tray-window thread. Neither calls out to the shell while
// holding the lock.
// ============================================================================

std::mutex g_mutex;
Settings g_settings;
std::vector<MirroredIcon> g_icons;         // shown in one of Split Tray's trays
std::vector<MirroredIcon> g_primaryOnly;   // tracked for routing stickiness only

// Split Tray's trays as they stand (PlanTrays), in number order: g_trays[i] is
// tray i + 2.
std::vector<TrayTarget> g_trays;
MonitorInfoEntry g_primaryMonitor;

// The layout of every tray drawn as a floating panel right now, by number. A
// display tray that is embedded in its taskbar is not in here.
std::map<int, TrayLayout> g_floatingLayouts;

// The displays whose tray is embedded in their taskbar. Written on the
// taskbar's thread as panels attach and detach, read by the tray thread to know
// which trays still need a floating panel.
std::set<HMONITOR> g_embeddedMonitors;

// Each floating tray's window, by number. Created and destroyed on the tray
// thread; kept here so other threads (and the tests) can find them.
std::map<int, HWND> g_floatingWnds;

std::atomic<uint64_t> g_nextSerial{1};

// --- Trays, from the shared state. Each of these needs g_mutex held. --------

TrayTarget* FindTrayLocked(int number) {
    if (number < 2) {
        return nullptr;
    }
    const size_t index = static_cast<size_t>(number - 2);
    return index < g_trays.size() ? &g_trays[index] : nullptr;
}

// The primary tray always exists; one of Split Tray's exists while its display
// does.
bool TrayAvailableLocked(int number) {
    if (number <= 1) {
        return true;
    }
    const TrayTarget* tray = FindTrayLocked(number);
    return tray && tray->available;
}

bool TrayEmbeddedLocked(const TrayTarget& tray) {
    return tray.forDisplay && g_embeddedMonitors.count(tray.monitor.handle) != 0;
}

// Indices into g_icons of the icons tray `number` shows, in store order.
std::vector<size_t> IconsInTrayLocked(int number) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < g_icons.size(); i++) {
        if (g_icons[i].shownTray == number) {
            indices.push_back(i);
        }
    }
    return indices;
}

// ---------------------------------------------------------------------------
// The program behind an icon, when its message does not say
//
// The rules match the program's path, and only an add carries it (DECISIONS
// 4). An application that only ever updates its icon - SystemInformer redraws
// four graphs a second and adds them again only when an update fails - never
// sends one while Explorer already has its icons. That is every load of the
// mod into a running Explorer: installing it, updating it, switching it off and
// on. Its icons then stayed in the main tray until Explorer restarted, whatever
// the rules said. So when a message carries no path and the icon's tray still
// depends on one, Windows is asked which program owns its window (DECISIONS
// 63, refining 4), with the least a process can be opened for: the right Task
// Manager uses to show the paths of elevated programs.
// ---------------------------------------------------------------------------

std::wstring ProcessImagePathOfWindow(HWND window) {
    DWORD pid = 0;
    if (!window || !GetWindowThreadProcessId(window, &pid) || !pid) {
        return std::wstring();
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return std::wstring();
    }
    WCHAR path[1024] = {};
    DWORD size = ARRAYSIZE(path);
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        result.assign(path, size);
    }
    CloseHandle(process);
    return result;
}

// Replaceable, so the tests decide what a window's program is.
std::wstring (*g_lookUpProcessPath)(HWND) = ProcessImagePathOfWindow;

// Fills in the path a message leaves out, when the icon's tray depends on it:
// an icon the mod has not seen, or one it has not been able to place yet.
// Caller holds g_mutex.
void FillMissingPathLocked(TrayNotification* n) {
    if (!n->exePath.empty() || (n->message != NIM_ADD && n->message != NIM_MODIFY)) {
        return;
    }
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (const auto& icon : *list) {
            if (SameIcon(icon, *n) && icon.destinationDecided) {
                return;
            }
        }
    }
    n->exePath = g_lookUpProcessPath(n->ownerWnd);
}

// The icon with this serial in g_icons, or -1.
int IndexOfSerialLocked(uint64_t serial) {
    for (size_t i = 0; i < g_icons.size(); i++) {
        if (g_icons[i].serial == serial) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// "the primary tray", "tray 2 (display to the left)".
std::wstring TrayLabelLocked(int number) {
    if (number <= 1) {
        return L"the primary tray";
    }
    std::wstring label = L"tray " + std::to_wstring(number);
    if (const TrayTarget* tray = FindTrayLocked(number)) {
        label += L" (" + DescribeTrayPlace(*tray, g_primaryMonitor) + L")";
    }
    return label;
}

std::atomic<bool> g_unloading{false};
std::atomic<HWND> g_trayWnd{nullptr};
std::atomic<HWND> g_shellTrayWnd{nullptr};
HANDLE g_trayThread = nullptr;
DWORD g_trayThreadId = 0;
// Whether it was asked to end and did not (StopTrayThread).
bool g_trayThreadStuck = false;
// How far it got: set once its window exists, or once it has given up.
enum class TrayThreadState { Starting, Running, GaveUp };
std::atomic<TrayThreadState> g_trayThreadState{TrayThreadState::Starting};
// How long Wh_ModInit waits for it to be running. A thread slower than this is
// left to carry on, and an unload can then come before its window exists
// (StopTrayThread); the integration test sets it to 0 to make that happen.
DWORD g_trayThreadStartWaitMs = 5000;
// The integration test holds the tray thread here, before it registers
// anything, to load the mod while the thread is still starting.
HANDLE g_trayThreadHold = nullptr;

// Set on the taskbar's thread once every icon is back with Explorer as the mod
// unloads. Until then the subclass keeps track of icons, unloading or not;
// from then it passes everything on untouched until it is removed
// (DECISIONS 68).
std::atomic<bool> g_handedBack{false};
// Whether unloading ended before that happened (HandBackToShell).
bool g_handBackStuck = false;
// How long unloading waits for the taskbar's thread at each step that needs
// it (DECISIONS 73); the tests shorten it.
DWORD g_taskbarWaitMs = 5000;
// How many calls of the mod's subclass are under way on the taskbar's thread.
// Unloading does not finish while there are any (DECISIONS 73).
std::atomic<int> g_subclassDepth{0};
// Whether the mod's panels could not be taken out of the taskbars in time.
bool g_panelsStuck = false;

// Messages posted to the mod's own tray window.
constexpr UINT WM_ST_REFRESH = WM_APP + 0x101;   // icons changed, re-layout
constexpr UINT WM_ST_SETTINGS = WM_APP + 0x102;  // settings changed
constexpr UINT WM_ST_SHUTDOWN = WM_APP + 0x103;
constexpr UINT WM_ST_ARRANGE = WM_APP + 0x104;   // open the arrange window

// Posted, or sent while unloading, to Shell_TrayWnd to settle the icons whose
// place in Explorer's tray is not where it should be, on the taskbar's thread,
// straight to Explorer's own window procedure without re-entering the mod's
// handler. It carries nothing: what to do is read from the icon store when it
// arrives (DECISIONS 58, 66). Being registered by name, it can be posted by any
// process on the desktop.
UINT GetReplayMessage() {
    static UINT msg = RegisterWindowMessageW(L"SplitTray_ReplayToShell_" WH_MOD_ID);
    return msg;
}

// Posted to Shell_TrayWnd to run an attach attempt on the taskbar's UI thread.
//
// Both taskbars are on the same thread - verified on this machine: Shell_TrayWnd
// and Shell_SecondaryTrayWnd both report thread 93292 - so the window the mod
// already subclasses is a usable way onto the thread that owns the secondary
// taskbar's XAML. XAML objects are thread-affine, so the timer cannot touch them
// itself.
UINT GetAttachMessage() {
    static UINT msg = RegisterWindowMessageW(L"SplitTray_AttachXaml_" WH_MOD_ID);
    return msg;
}

// Same idea, for redrawing the embedded trays from a thread that must not touch
// XAML - the arrange window's, for one.
UINT GetXamlRefreshMessage() {
    static UINT msg = RegisterWindowMessageW(L"SplitTray_RefreshXaml_" WH_MOD_ID);
    return msg;
}

// Sent while unloading, so the panels are taken out of the taskbars on the
// thread that owns them. Removing them from Windhawk's unload thread threw the
// wrong-thread error inside a catch-all and left them in place until Explorer
// restarted.
UINT GetXamlRemoveMessage() {
    static UINT msg = RegisterWindowMessageW(L"SplitTray_RemoveXaml_" WH_MOD_ID);
    return msg;
}

// ============================================================================
// Section 6 - The floating trays, and the mod's own window
//
// The tray thread owns a hidden controller window - the target of every
// WM_ST_* message, of the retry timer, and of Explorer's TaskbarCreated
// broadcast - and a floating panel for each tray that is not embedded in a
// taskbar: every extra tray, and a display's tray while embedding is off or
// has not attached.
// ============================================================================

constexpr PCWSTR kControllerClassName = L"SplitTrayController";
constexpr PCWSTR kFloatingClassName = L"SplitTrayFloatingTray";
constexpr PCWSTR kArrangeClassName = L"SplitTrayArrangeWindow";

// ---------------------------------------------------------------------------
// The mod's window classes
//
// A window class outlives the module that registered it: Windows does not
// unregister a DLL's classes when the DLL unloads, and a class left behind
// points at a window procedure that is no longer there. They were registered
// against Explorer's own module, which the mod never unloads, and the arrange
// window's was never unregistered at all - so after the mod was reloaded, its
// next arrange window would have been created with the old, unloaded window
// procedure (DECISIONS 59). They now belong to the mod's own module and are
// unregistered, all of them, when the tray thread ends.
// ---------------------------------------------------------------------------

HINSTANCE ModuleInstance() {
    static const HINSTANCE instance = [] {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ModuleInstance), &module);
        return module;
    }();
    return instance;
}

bool RegisterModClass(WNDCLASSEXW* wc) {
    wc->hInstance = ModuleInstance();
    // One an earlier build registered against Explorer's module, which nothing
    // ever took down. Harmless when there is none.
    if (ModuleInstance() != GetModuleHandleW(nullptr)) {
        UnregisterClassW(wc->lpszClassName, GetModuleHandleW(nullptr));
    }
    if (RegisterClassExW(wc)) {
        return true;
    }
    // Left by a load of this module that could not clean up. Reused, it would
    // run that load's window procedure; replaced, it runs this one's.
    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS &&
        UnregisterClassW(wc->lpszClassName, wc->hInstance) && RegisterClassExW(wc)) {
        return true;
    }
    Wh_Log(L"could not register window class %s: %lu", wc->lpszClassName,
           GetLastError());
    return false;
}

void UnregisterModClass(PCWSTR name) {
    if (!UnregisterClassW(name, ModuleInstance()) &&
        GetLastError() != ERROR_CLASS_DOES_NOT_EXIST) {
        Wh_Log(L"could not unregister window class %s: %lu", name, GetLastError());
    }
}

// Tray-thread state for one floating panel.
struct FloatingTray {
    HWND wnd = nullptr;
    HWND tooltip = nullptr;
    int hotIndex = -1;  // the cell under the pointer, for the highlight
};

std::map<int, FloatingTray> g_floatingTrays;  // tray thread only, by number

// Where the displays come from: Windows, or in the regression tests a fixed set,
// so that what they check does not depend on the machine they run on.
std::vector<MonitorInfoEntry> (*g_enumerateMonitors)() = EnumerateMonitors;

// Recomputes which trays exist and how the floating ones are laid out. Caller
// must hold g_mutex.
void RecomputeGeometryLocked() {
    const auto monitors = g_enumerateMonitors();
    g_trays = PlanTrays(monitors, g_settings);
    g_primaryMonitor = MonitorInfoEntry{};
    for (const auto& monitor : monitors) {
        if (monitor.primary) {
            g_primaryMonitor = monitor;
        }
    }
    g_floatingLayouts.clear();
    for (const auto& tray : g_trays) {
        if (!tray.available || TrayEmbeddedLocked(tray)) {
            continue;
        }
        // An empty tray still gets one cell: a handle to reach the mod's menu
        // by, as the embedded tray has. Otherwise an extra tray nothing has been
        // routed to yet would be invisible and unreachable.
        const int count = static_cast<int>(IconsInTrayLocked(tray.number).size());
        g_floatingLayouts[tray.number] =
            ComputeLayout(tray.monitor.workArea, std::max(count, 1), g_settings,
                          tray.monitor.dpi, tray.corner);
    }
}

// The name an icon goes by in a list or a menu: the first line of its tooltip
// - SystemInformer's run to several lines of live data - or its executable.
std::wstring IconLabel(const MirroredIcon& icon) {
    std::wstring label = icon.tip;
    const size_t newline = label.find_first_of(L"\r\n");
    if (newline != std::wstring::npos) {
        label.erase(newline);
    }
    while (!label.empty() && label.back() == L' ') {
        label.pop_back();
    }
    if (label.empty()) {
        label = FileNameOf(icon.exePath);
    }
    if (label.empty()) {
        label = L"(unnamed icon)";
    }
    return label;
}

// Every icon the mod knows about, with the tray it is in now. An icon whose own
// tray is missing is in the primary tray, which is where it actually shows.
struct KnownIcon {
    std::wstring key;
    std::wstring label;
    int tray = 1;
};

std::vector<KnownIcon> KnownIcons() {
    std::vector<KnownIcon> known;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& icon : g_icons) {
            known.push_back({StableKeyOf(icon), IconLabel(icon), icon.shownTray});
        }
        for (const auto& icon : g_primaryOnly) {
            known.push_back({StableKeyOf(icon), IconLabel(icon), 1});
        }
    }
    std::sort(known.begin(), known.end(), [](KnownIcon const& a, KnownIcon const& b) {
        return _wcsicmp(a.label.c_str(), b.label.c_str()) < 0;
    });
    return known;
}

// Every tray an icon can be sent to right now, primary first.
std::vector<int> AvailableTrayNumbers() {
    std::vector<int> numbers = {1};
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& tray : g_trays) {
        if (tray.available) {
            numbers.push_back(tray.number);
        }
    }
    return numbers;
}

std::wstring TrayLabel(int number) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return TrayLabelLocked(number);
}

bool ShiftHeld() {
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}

int TrayNumberOfWindow(HWND hWnd) {
    return static_cast<int>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
}

// What one floating tray draws: its layout, and its icons in store order.
struct FloatingView {
    TrayLayout layout;
    std::vector<uint64_t> serials;
    std::vector<OwnedIcon> icons;  // empty unless asked for
    std::vector<std::wstring> tips;
    int Cells() const { return std::max(1, static_cast<int>(serials.size())); }
};

// `withIcons` copies each icon's picture for drawing; hit tests and tooltips
// do without.
FloatingView FloatingViewOf(int number, bool withIcons = true) {
    FloatingView view;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto layout = g_floatingLayouts.find(number);
    if (layout != g_floatingLayouts.end()) {
        view.layout = layout->second;
    }
    for (size_t i : IconsInTrayLocked(number)) {
        view.serials.push_back(g_icons[i].serial);
        if (withIcons) {
            view.icons.push_back(OwnedIcon::CopyOf(g_icons[i].icon));
        }
        view.tips.push_back(g_icons[i].tip);
    }
    return view;
}

// One icon of a tray embedded in a taskbar, as the taskbar's thread draws it:
// from its own copy of the picture, like everything else drawn after the
// store's lock is released. The tray thread's watchdog destroys the store's
// copy when the icon's application goes (DECISIONS 60).
struct CellSnapshot {
    OwnedIcon icon;
    // The cell's tooltip: empty when tooltips are switched off (DECISIONS 77).
    std::wstring tip;
    std::wstring key;
    uint64_t serial = 0;
    // Whether the store has a picture for the icon, which `icon` is a copy of
    // unless copying it failed.
    bool hasPicture = false;
};

// What a cell updated in place does with its picture (DECISIONS 76). It was
// given a new one only when there was one, so a picture its application took
// away stayed on show in the taskbar until something else rebuilt the tray.
// One that could not be copied for this refresh is still in the store
// (DECISIONS 72): the cell keeps what it shows, and the next refresh copies it
// again.
enum class CellPicture { Keep, Replace, Clear };

CellPicture CellPictureOf(const CellSnapshot& cell) {
    if (!cell.hasPicture) {
        return CellPicture::Clear;
    }
    return cell.icon.get() ? CellPicture::Replace : CellPicture::Keep;
}

// Tray `number`'s icons, in the store's order.
std::vector<CellSnapshot> CellSnapshotsOf(int number) {
    std::vector<CellSnapshot> cells;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (size_t i : IconsInTrayLocked(number)) {
        const MirroredIcon& icon = g_icons[i];
        cells.push_back({OwnedIcon::CopyOf(icon.icon),
                         g_settings.showTooltips ? icon.tip : std::wstring(),
                         StableKeyOf(icon), icon.serial, icon.icon != nullptr});
    }
    return cells;
}

FloatingTray* FloatingTrayOfWindow(HWND hWnd) {
    auto found = g_floatingTrays.find(TrayNumberOfWindow(hWnd));
    return (found != g_floatingTrays.end() && found->second.wnd == hWnd)
               ? &found->second
               : nullptr;
}

void UpdateTooltipText(HWND hWnd) {
    FloatingTray* tray = FloatingTrayOfWindow(hWnd);
    if (!tray || !tray->tooltip) {
        return;
    }
    const int number = TrayNumberOfWindow(hWnd);
    const FloatingView view = FloatingViewOf(number, /*withIcons=*/false);
    std::wstring text;
    if (view.serials.empty()) {
        text = L"Split Tray - " + TrayLabel(number) + L" is empty";
    } else if (tray->hotIndex >= 0 &&
               tray->hotIndex < static_cast<int>(view.tips.size())) {
        text = view.tips[static_cast<size_t>(tray->hotIndex)];
    }
    TTTOOLINFOW ti = {sizeof(ti)};
    ti.hwnd = hWnd;
    ti.uId = 0;
    ti.lpszText = text.empty() ? const_cast<PWSTR>(L" ") : text.data();
    SendMessageW(tray->tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
}

void PaintTray(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    if (!hdc) {
        return;
    }

    RECT client;
    GetClientRect(hWnd, &client);

    // Double buffer: the tray repaints on every icon change and flicker on top of
    // someone's wallpaper is very visible.
    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP bmp =
        CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);

    const FloatingView view = FloatingViewOf(TrayNumberOfWindow(hWnd));
    const FloatingTray* tray = FloatingTrayOfWindow(hWnd);
    const int hotIndex = tray ? tray->hotIndex : -1;
    COLORREF background;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        background = g_settings.background;
    }
    const TrayLayout& layout = view.layout;

    HBRUSH bgBrush = CreateSolidBrush(background);
    FillRect(memDc, &client, bgBrush);
    DeleteObject(bgBrush);

    if (layout.cell > 0 && layout.columns > 0) {
        const int inset = (layout.cell - layout.icon) / 2;
        for (int i = 0; i < view.Cells(); i++) {
            const int column = i % layout.columns;
            const int row = i / layout.columns;
            RECT cell = {column * layout.cell, row * layout.cell,
                         (column + 1) * layout.cell, (row + 1) * layout.cell};
            if (i == hotIndex) {
                // A subtle highlight so it is obvious the icons are live.
                HBRUSH hot = CreateSolidBrush(
                    RGB(std::min(255, GetRValue(background) + 28),
                        std::min(255, GetGValue(background) + 28),
                        std::min(255, GetBValue(background) + 28)));
                FillRect(memDc, &cell, hot);
                DeleteObject(hot);
            }
            if (i < static_cast<int>(view.icons.size())) {
                if (view.icons[static_cast<size_t>(i)].get()) {
                    DrawIconEx(memDc, cell.left + inset, cell.top + inset,
                               view.icons[static_cast<size_t>(i)].get(), layout.icon,
                               layout.icon, 0, nullptr, DI_NORMAL);
                }
                continue;
            }
            // The empty tray's handle: three dots, as a "more" affordance.
            HBRUSH dots = CreateSolidBrush(RGB(0x9A, 0x9A, 0x9A));
            HGDIOBJ oldBrush = SelectObject(memDc, dots);
            HGDIOBJ oldPen = SelectObject(memDc, GetStockObject(NULL_PEN));
            const int dot = std::max(2, layout.icon / 6);
            const int midY = (cell.top + cell.bottom) / 2;
            const int midX = (cell.left + cell.right) / 2;
            for (int d = -1; d <= 1; d++) {
                const int x = midX + d * dot * 2;
                Ellipse(memDc, x - dot / 2, midY - dot / 2, x + dot / 2 + 1,
                        midY + dot / 2 + 1);
            }
            SelectObject(memDc, oldPen);
            SelectObject(memDc, oldBrush);
            DeleteObject(dots);
        }
    }

    BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, memDc,
           0, 0, SRCCOPY);

    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hWnd, &ps);
}

void ForwardClick(uint64_t serial, UINT mouseMessage, POINT screenPoint);
void ApplySettingsToTrackedIcons();
void ReplayRoutingChanges();
void WakeReplayDelivery();
void MoveIconToTray(std::wstring_view key, Destination destination);
void EnsureShellTrayWindowSubclassed();
UINT TaskbarCreatedMessage();
void NoteShellAnnouncedTaskbar();
void ShowArrangeWindow();
void CloseArrangeWindow();
void RefreshArrangeWindow(bool repopulate);
void NotifyTrayWindow(UINT message);
void RequestRedrawOfAllTrays();

// The mod's menu on a floating tray: Shift+right-click on an icon, or any click
// on an empty tray's handle. The same choices as the embedded tray's menu.
void ShowFloatingTrayMenu(HWND hWnd, int number, uint64_t serial, POINT at) {
    constexpr UINT kArrange = 1;
    constexpr UINT kResetMoved = 2;
    constexpr UINT kMoveThis = 100;   // + target tray number
    constexpr UINT kMoveHere = 1000;  // + index into `others`

    std::wstring key;
    if (serial) {
        std::lock_guard<std::mutex> lock(g_mutex);
        const int index = IndexOfSerialLocked(serial);
        if (index >= 0) {
            key = StableKeyOf(g_icons[static_cast<size_t>(index)]);
        }
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    if (!key.empty()) {
        for (int target : AvailableTrayNumbers()) {
            if (target == number) {
                continue;
            }
            const std::wstring text = L"Move to " + TrayLabel(target);
            AppendMenuW(menu, MF_STRING, kMoveThis + static_cast<UINT>(target),
                        text.c_str());
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }

    std::vector<KnownIcon> others;
    for (auto& icon : KnownIcons()) {
        if (icon.tray != number) {
            others.push_back(std::move(icon));
        }
    }
    HMENU here = CreatePopupMenu();
    for (size_t i = 0; i < others.size() && i < 500; i++) {
        AppendMenuW(here, MF_STRING, kMoveHere + static_cast<UINT>(i),
                    others[i].label.c_str());
    }
    if (others.empty()) {
        AppendMenuW(here, MF_STRING | MF_GRAYED, 0, L"Every icon is already here");
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(here),
                L"Move an icon to this tray");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kArrange, L"Arrange icons\x2026");
    AppendMenuW(menu, MF_STRING, kResetMoved, L"Reset moved icons");

    // Without the foreground, a tray menu does not close when the user clicks
    // elsewhere; the WM_NULL afterwards is the documented companion.
    SetForegroundWindow(hWnd);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON |
                                                   TPM_BOTTOMALIGN | TPM_NONOTIFY,
                                        at.x, at.y, 0, hWnd, nullptr);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(menu);  // and the submenu with it

    if (command == kArrange) {
        ShowArrangeWindow();
    } else if (command == kResetMoved) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            ForgetAllPlacements();
        }
        ApplySettingsToTrackedIcons();
        RequestRedrawOfAllTrays();
    } else if (command >= kMoveHere) {
        const size_t index = command - kMoveHere;
        if (index < others.size()) {
            MoveIconToTray(others[index].key, Destination::Tray(number));
        }
    } else if (command >= kMoveThis && !key.empty()) {
        MoveIconToTray(key, Destination::Tray(static_cast<int>(command - kMoveThis)));
    }
}

LRESULT CALLBACK FloatingTrayProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            PaintTray(hWnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;  // painted in WM_PAINT

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_MOUSEMOVE: {
            FloatingTray* tray = FloatingTrayOfWindow(hWnd);
            if (!tray) {
                return 0;
            }
            const FloatingView view =
                FloatingViewOf(TrayNumberOfWindow(hWnd), /*withIcons=*/false);
            const int index = HitTestCell(view.layout, GET_X_LPARAM(lParam),
                                          GET_Y_LPARAM(lParam), view.Cells());
            if (index != tray->hotIndex) {
                tray->hotIndex = index;
                UpdateTooltipText(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
            TrackMouseEvent(&tme);
            break;  // on to the tooltip relay below
        }

        case WM_MOUSELEAVE:
            if (FloatingTray* tray = FloatingTrayOfWindow(hWnd)) {
                tray->hotIndex = -1;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            const int number = TrayNumberOfWindow(hWnd);
            const FloatingView view = FloatingViewOf(number, /*withIcons=*/false);
            const POINT client = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int index = HitTestCell(view.layout, client.x, client.y, view.Cells());
            if (index < 0) {
                return 0;
            }
            POINT screen = client;
            ClientToScreen(hWnd, &screen);

            const bool onIcon = index < static_cast<int>(view.serials.size());
            const bool release = (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP ||
                                  msg == WM_MBUTTONUP);
            // The handle belongs to the mod, so any click on it is the mod's.
            // On an icon, plain right-click is the application's (DECISIONS 30)
            // and Shift+right-click is the mod's.
            if (!onIcon) {
                if (release) {
                    ShowFloatingTrayMenu(hWnd, number, 0, screen);
                }
                return 0;
            }
            if (ShiftHeld() && (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP)) {
                if (msg == WM_RBUTTONUP) {
                    ShowFloatingTrayMenu(hWnd, number,
                                         view.serials[static_cast<size_t>(index)],
                                         screen);
                }
                return 0;
            }
            ForwardClick(view.serials[static_cast<size_t>(index)], msg, screen);
            return 0;
        }

        case WM_NCHITTEST:
            return HTCLIENT;  // never show a resize or caption cursor

        case WM_DESTROY:
            if (FloatingTray* tray = FloatingTrayOfWindow(hWnd)) {
                if (tray->tooltip) {
                    DestroyWindow(tray->tooltip);
                    tray->tooltip = nullptr;
                }
            }
            return 0;
    }

    FloatingTray* tray = FloatingTrayOfWindow(hWnd);
    if (tray && tray->tooltip && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
        MSG relay = {hWnd, msg, wParam, lParam};
        SendMessageW(tray->tooltip, TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&relay));
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HWND CreateTooltip(HWND owner) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    HWND tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                   WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0,
                                   owner, nullptr, nullptr, nullptr);
    if (!tooltip) {
        return nullptr;
    }
    // The whole window is one tool; its text follows the cell under the pointer.
    TTTOOLINFOW ti = {sizeof(ti)};
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = owner;
    ti.uId = 0;
    ti.rect = RECT{0, 0, 32767, 32767};
    ti.lpszText = const_cast<PWSTR>(L" ");
    SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 400);
    return tooltip;
}

// Brings the floating panels in line with the trays: one for every tray that
// floats, where its layout says, and none for any other. Tray thread only.
void SyncFloatingTrays() {
    std::map<int, TrayLayout> layouts;
    int opacity;
    bool onTop;
    bool tooltips;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecomputeGeometryLocked();
        if (!g_unloading.load()) {
            layouts = g_floatingLayouts;
        }
        opacity = g_settings.opacity;
        onTop = g_settings.alwaysOnTop;
        tooltips = g_settings.showTooltips;
    }

    for (auto it = g_floatingTrays.begin(); it != g_floatingTrays.end();) {
        if (layouts.count(it->first)) {
            ++it;
            continue;
        }
        const int number = it->first;
        HWND wnd = it->second.wnd;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_floatingWnds.erase(number);
        }
        if (wnd) {
            DestroyWindow(wnd);  // WM_DESTROY takes its tooltip with it
        }
        it = g_floatingTrays.erase(it);
    }

    for (const auto& [number, layout] : layouts) {
        FloatingTray& tray = g_floatingTrays[number];
        if (!tray.wnd) {
            tray.wnd = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
                    (onTop ? WS_EX_TOPMOST : 0),
                kFloatingClassName, L"Split Tray", WS_POPUP, layout.x, layout.y,
                std::max(1, layout.width), std::max(1, layout.height), nullptr, nullptr,
                ModuleInstance(), nullptr);
            if (!tray.wnd) {
                Wh_Log(L"could not create the floating panel for tray %d: %lu", number,
                       GetLastError());
                g_floatingTrays.erase(number);
                continue;
            }
            SetWindowLongPtrW(tray.wnd, GWLP_USERDATA, number);
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_floatingWnds[number] = tray.wnd;
            }
            Wh_Log(L"tray %d floats at (%d,%d) %dx%d", number, layout.x, layout.y,
                   layout.width, layout.height);
        }
        // Every time, not only for a new window: the setting was read when the
        // window was made, so switching it had no effect on a tray already
        // there (DECISIONS 77).
        if (tooltips && !tray.tooltip) {
            tray.tooltip = CreateTooltip(tray.wnd);
        } else if (!tooltips && tray.tooltip) {
            TTTOOLINFOW ti = {sizeof(ti)};
            ti.hwnd = tray.wnd;
            ti.uId = 0;
            SendMessageW(tray.tooltip, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
            DestroyWindow(tray.tooltip);
            tray.tooltip = nullptr;
        }
        SetLayeredWindowAttributes(tray.wnd, 0, static_cast<BYTE>(opacity), LWA_ALPHA);
        SetWindowPos(tray.wnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, layout.x, layout.y,
                     layout.width, layout.height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(tray.wnd, nullptr, FALSE);
    }
}

void DestroyFloatingTrays() {
    for (auto& [number, tray] : g_floatingTrays) {
        if (tray.wnd) {
            DestroyWindow(tray.wnd);
        }
    }
    g_floatingTrays.clear();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_floatingWnds.clear();
}

LRESULT CALLBACK ControllerProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Explorer's own announcement that its taskbar is ready. This window is a
    // top-level one, so it hears it like any application's.
    if (msg == TaskbarCreatedMessage()) {
        NoteShellAnnouncedTaskbar();
        return 0;
    }

    switch (msg) {
        case WM_ST_SETTINGS:
            // New rules can change where an icon that is already on screen
            // belongs, so re-resolve every tracked icon and replay the
            // difference into the shell before re-laying out.
            ApplySettingsToTrackedIcons();
            SyncFloatingTrays();
            RefreshArrangeWindow(true);
#ifndef SPLITTRAY_NO_XAML
            SplitTrayXaml::RequestEmbeddedRefresh();
#endif
            return 0;

        case WM_ST_REFRESH:
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            for (auto& [number, tray] : g_floatingTrays) {
                tray.hotIndex = -1;
            }
            SyncFloatingTrays();
            RefreshArrangeWindow(false);
            return 0;

        case WM_ST_ARRANGE:
            ShowArrangeWindow();
            return 0;

        case WM_ST_SHUTDOWN:
            CloseArrangeWindow();
            DestroyFloatingTrays();
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK ArrangeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ReleaseArrangeResources();

// Everything the tray thread registers, taken down again once its windows are
// gone; a class with a window still open cannot be unregistered.
void UnregisterTrayThreadClasses() {
    UnregisterModClass(kArrangeClassName);
    UnregisterModClass(kFloatingClassName);
    UnregisterModClass(kControllerClassName);
}

DWORD WINAPI TrayThreadProc(LPVOID) {
    if (g_trayThreadHold) {
        WaitForSingleObject(g_trayThreadHold, INFINITE);
    }

    WNDCLASSEXW controllerClass = {sizeof(controllerClass)};
    controllerClass.lpfnWndProc = ControllerProc;
    controllerClass.lpszClassName = kControllerClassName;

    WNDCLASSEXW floatingClass = {sizeof(floatingClass)};
    floatingClass.lpfnWndProc = FloatingTrayProc;
    floatingClass.lpszClassName = kFloatingClassName;
    floatingClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    floatingClass.style = CS_DBLCLKS;

    WNDCLASSEXW arrangeClass = {sizeof(arrangeClass)};
    arrangeClass.lpfnWndProc = ArrangeWndProc;
    arrangeClass.lpszClassName = kArrangeClassName;
    arrangeClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    arrangeClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    // All or nothing: without its thread the mod never attaches, and every
    // icon stays where Explorer puts it (DECISIONS 69). RegisterModClass says
    // which failed. Wh_ModInit may have stopped waiting already, so this is
    // where the outcome is logged.
    auto giveUp = []() -> DWORD {
        UnregisterTrayThreadClasses();
        Wh_Log(L"the tray thread gave up; Split Tray leaves Explorer's tray alone");
        g_trayThreadState.store(TrayThreadState::GaveUp);
        return 1;
    };
    if (!RegisterModClass(&controllerClass) || !RegisterModClass(&floatingClass) ||
        !RegisterModClass(&arrangeClass)) {
        return giveUp();
    }

    // Never shown. A hidden top-level window still receives broadcasts, which
    // a message-only window would not - and TaskbarCreated is one.
    HWND hWnd = CreateWindowExW(WS_EX_TOOLWINDOW, kControllerClassName, L"Split Tray",
                                WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                ModuleInstance(), nullptr);
    if (!hWnd) {
        Wh_Log(L"CreateWindowExW for the controller window failed: %u", GetLastError());
        return giveUp();
    }

    g_trayWnd.store(hWnd);
    g_trayThreadState.store(TrayThreadState::Running);
    SyncFloatingTrays();

    // Cheap watchdog: prunes icons whose owner died, notices display changes that
    // arrive without a WM_DISPLAYCHANGE, and re-subclasses a recreated taskbar.
    SetTimer(hWnd, 1, 2000, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.hwnd == hWnd) {
            EnsureShellTrayWindowSubclassed();
#ifndef SPLITTRAY_NO_XAML
            SplitTrayXaml::EnsureTaskbarXamlHooked();
            // The IconView constructor hook only catches elements built after it
            // is installed, and resolving the symbols takes seconds - long
            // enough that on a normal boot the other taskbars' trays are already
            // built and the mod never sees an element on them at all. Asking
            // again on the timer is the same remedy as DECISIONS 18, for the
            // same shape of defect.
            if (SplitTrayXaml::AnyDisplayTrayWaitingToEmbed()) {
                if (HWND tray = g_shellTrayWnd.load()) {
                    PostMessageW(tray, GetAttachMessage(), 0, 0);
                }
            }
#endif
            // Also wakes the taskbar's thread while any icon is waiting for
            // it: a wake-up can be lost - the taskbar was being recreated when
            // it was posted - and one Explorer refused is asked again.
            ReplayRoutingChanges();
            SyncFloatingTrays();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_trayWnd.store(nullptr);
    CloseArrangeWindow();
    DestroyFloatingTrays();
    ReleaseArrangeResources();
    UnregisterTrayThreadClasses();
    return 0;
}

// ============================================================================
// Section 6b - The arrange window
//
// One list per tray: the primary tray, then each of Split Tray's that exists.
// An icon in a tray's overflow is listed in that tray and marked. Drag an icon
// to another list, or select it and press that tray's number; H puts it in its
// tray's overflow or brings it back out. Every choice is remembered.
//
// This exists because dragging inside the taskbar could not be made to work.
// The mod's cells live in Explorer's XAML island, where the pointer does not
// behave the way it does in an ordinary window: pointer capture on our Border
// is not enough to keep a drag alive, and two attempts at it changed nothing
// the user could see. This window is the mod's own, on the mod's own thread,
// with no other claim on its input - so a drag here is just a drag.
//
// Plain Win32 rather than XAML: the mod's thread already pumps messages, and
// this way the feature does not depend on any of the taskbar internals that
// have been the fragile part of this project throughout.
// ============================================================================

constexpr int kArrangeListIdBase = 1001;

// Tray-thread state only; no locking needed beyond the snapshot itself.
HWND g_arrangeWnd = nullptr;
std::vector<HWND> g_arrangeLists;
std::vector<int> g_arrangeTrays;          // the tray number each list shows
// One image list for every list in the window. The window owns it, and each
// list is created with LVS_SHAREIMAGELISTS: without that, every list destroys
// the image list it was given when it is destroyed itself, so closing the
// window freed the one list once per list, after the window had freed it
// already.
HIMAGELIST g_arrangeImages = nullptr;
HFONT g_arrangeFont = nullptr;  // the shell's message font, made once
std::vector<std::wstring> g_arrangeKeys;  // indexed by ListView item lParam
bool g_arrangeDragging = false;
int g_arrangeDragItem = -1;
int g_arrangeDragList = -1;

int ArrangeListOfId(UINT_PTR id) {
    const int list = static_cast<int>(id) - kArrangeListIdBase;
    return (list >= 0 && list < static_cast<int>(g_arrangeLists.size())) ? list : -1;
}

// The list that shows tray `number`, or -1.
int ArrangeListOfTray(int number) {
    for (size_t i = 0; i < g_arrangeTrays.size(); i++) {
        if (g_arrangeTrays[i] == number) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// What the arrange window's rows are made of, without their pictures and
// labels: which icons there are, the tray each is in, and whether it is in that
// tray's overflow (DECISIONS 75). The window was filled when it opened and
// again only after a move made in it, so an application started or closed
// meanwhile, or an icon moved from a tray's menu, left rows missing or stale.
// Pictures and tooltips are left out: they change several times a second, and
// filling the lists again resets what the user has selected.
std::wstring ArrangeLayoutNow() {
    std::wstring layout;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& icon : g_primaryOnly) {
        layout += std::to_wstring(icon.serial) + L":1;";
    }
    for (const auto& icon : g_icons) {
        layout += std::to_wstring(icon.serial) + L':' + std::to_wstring(icon.shownTray) +
                  (IsIconHidden(StableKeyOf(icon)) ? L"h;" : L";");
    }
    return layout;
}

// What the lists were last filled with (ArrangeLayoutNow).
std::wstring g_arrangeLayout;

std::wstring ArrangeKeyOfItem(int list, int item);

void PopulateArrangeLists() {
    if (g_arrangeLists.empty()) {
        return;
    }
    g_arrangeLayout = ArrangeLayoutNow();
    // What is selected in each list stays selected, where it is still there.
    std::vector<std::wstring> selected;
    for (size_t i = 0; i < g_arrangeLists.size(); i++) {
        selected.push_back(ArrangeKeyOfItem(
            static_cast<int>(i),
            ListView_GetNextItem(g_arrangeLists[i], -1, LVNI_SELECTED)));
    }

    struct Row {
        std::wstring key;
        std::wstring label;
        OwnedIcon icon;  // drawn after the lock is released (DECISIONS 60)
        int list;
        bool hidden;
    };
    std::vector<Row> rows;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        rows.reserve(g_icons.size() + g_primaryOnly.size());
        for (const auto& icon : g_primaryOnly) {
            rows.push_back({StableKeyOf(icon), IconLabel(icon),
                            OwnedIcon::CopyOf(icon.icon), 0, false});
        }
        for (const auto& icon : g_icons) {
            const std::wstring key = StableKeyOf(icon);
            const int list = std::max(0, ArrangeListOfTray(icon.shownTray));
            rows.push_back({key, IconLabel(icon), OwnedIcon::CopyOf(icon.icon), list,
                            list > 0 && IsIconHidden(key)});
        }
    }
    // Within a tray, the icons on its bar first, then those in its overflow.
    std::sort(rows.begin(), rows.end(), [](Row const& a, Row const& b) {
        if (a.hidden != b.hidden) {
            return !a.hidden;
        }
        return _wcsicmp(a.label.c_str(), b.label.c_str()) < 0;
    });

    for (HWND list : g_arrangeLists) {
        ListView_DeleteAllItems(list);
    }
    if (g_arrangeImages) {
        ImageList_RemoveAll(g_arrangeImages);
    }
    g_arrangeKeys.clear();

    std::vector<int> counts(g_arrangeLists.size(), 0);
    for (const auto& row : rows) {
        const int imageIndex =
            (g_arrangeImages && row.icon.get())
                ? ImageList_ReplaceIcon(g_arrangeImages, -1, row.icon.get())
                : -1;
        g_arrangeKeys.push_back(row.key);
        const std::wstring text =
            row.hidden ? row.label + L"  (in the overflow)" : row.label;

        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        item.iItem = counts[static_cast<size_t>(row.list)];
        item.pszText = const_cast<PWSTR>(text.c_str());
        item.iImage = imageIndex;
        item.lParam = static_cast<LPARAM>(g_arrangeKeys.size() - 1);
        HWND list = g_arrangeLists[static_cast<size_t>(row.list)];
        const int inserted = ListView_InsertItem(list, &item);
        std::wstring& wasSelected = selected[static_cast<size_t>(row.list)];
        if (inserted >= 0 && !wasSelected.empty() && wasSelected == row.key) {
            ListView_SetItemState(list, inserted, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            wasSelected.clear();
        }
        counts[static_cast<size_t>(row.list)]++;
    }

    for (HWND list : g_arrangeLists) {
        ListView_SetColumnWidth(list, 0, LVSCW_AUTOSIZE_USEHEADER);
    }
}

// Which of the lists, if any, the cursor is over.
int ArrangeListUnderCursor() {
    POINT cursor = {};
    GetCursorPos(&cursor);
    for (size_t i = 0; i < g_arrangeLists.size(); i++) {
        RECT rect = {};
        if (g_arrangeLists[i] && GetWindowRect(g_arrangeLists[i], &rect) &&
            PtInRect(&rect, cursor)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::wstring ArrangeKeyOfItem(int list, int item) {
    if (list < 0 || list >= static_cast<int>(g_arrangeLists.size()) || item < 0) {
        return std::wstring();
    }
    LVITEMW query = {};
    query.mask = LVIF_PARAM;
    query.iItem = item;
    if (!ListView_GetItem(g_arrangeLists[static_cast<size_t>(list)], &query)) {
        return std::wstring();
    }
    const size_t index = static_cast<size_t>(query.lParam);
    return index < g_arrangeKeys.size() ? g_arrangeKeys[index] : std::wstring();
}

// Moves an icon to the tray another list shows. Only a change of tray goes
// through MoveIconToTray, which records a placement.
void ArrangeMove(std::wstring key, int fromList, int toList) {
    if (key.empty() || toList < 0 || toList == fromList ||
        toList >= static_cast<int>(g_arrangeTrays.size())) {
        return;
    }
    const int toTray = g_arrangeTrays[static_cast<size_t>(toList)];
    MoveIconToTray(key, Destination::Tray(toTray));
    // An icon sent to the primary tray is not left marked hidden, or it would
    // come back into an overflow unasked the next time it was moved over.
    if (toTray <= 1 && IsIconHidden(key)) {
        SetIconHidden(key, false);
    }
    PopulateArrangeLists();
    RequestRedrawOfAllTrays();
}

// H: into the tray's overflow, or back out onto its bar. The primary tray's
// overflow is Windows' own business.
void ArrangeToggleHidden(int list) {
    if (list <= 0 || list >= static_cast<int>(g_arrangeLists.size())) {
        return;
    }
    const int item =
        ListView_GetNextItem(g_arrangeLists[static_cast<size_t>(list)], -1, LVNI_SELECTED);
    const std::wstring key = ArrangeKeyOfItem(list, item);
    if (key.empty()) {
        return;
    }
    SetIconHidden(key, !IsIconHidden(key));
    PopulateArrangeLists();
    RequestRedrawOfAllTrays();
}

// Double-click and Enter: between the primary tray and tray 2, which is the
// move people make most, and back to the primary tray from any other.
int ArrangeDefaultTarget(int fromList) {
    if (fromList < 0 || fromList >= static_cast<int>(g_arrangeTrays.size())) {
        return -1;
    }
    if (g_arrangeTrays[static_cast<size_t>(fromList)] <= 1) {
        return g_arrangeLists.size() > 1 ? 1 : -1;
    }
    return 0;
}

// Moves the selected row of `list` and keeps a row selected at the same place,
// so a run of icons can be sent across by pressing the same key repeatedly.
void ArrangeMoveSelected(int list, int toList) {
    if (list < 0 || list >= static_cast<int>(g_arrangeLists.size())) {
        return;
    }
    HWND view = g_arrangeLists[static_cast<size_t>(list)];
    const int item = ListView_GetNextItem(view, -1, LVNI_SELECTED);
    if (item < 0) {
        return;
    }
    ArrangeMove(ArrangeKeyOfItem(list, item), list, toList);

    const int remaining = ListView_GetItemCount(view);
    if (remaining > 0) {
        const int next = std::min(item, remaining - 1);
        ListView_SetItemState(view, next, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(view, next, FALSE);
    }
    SetFocus(view);
}

void EndArrangeDrag(bool drop) {
    if (!g_arrangeDragging) {
        return;
    }
    g_arrangeDragging = false;
    if (GetCapture() == g_arrangeWnd) {
        ReleaseCapture();
    }

    const int target = drop ? ArrangeListUnderCursor() : -1;
    if (target >= 0 && target != g_arrangeDragList) {
        ArrangeMove(ArrangeKeyOfItem(g_arrangeDragList, g_arrangeDragItem),
                    g_arrangeDragList, target);
    }
    g_arrangeDragItem = -1;
    g_arrangeDragList = -1;
    // What changed during the drag was left for its end (RefreshArrangeWindow).
    // Posted: this can run while the window is being destroyed.
    NotifyTrayWindow(WM_ST_REFRESH);
}

LRESULT CALLBACK ArrangeWndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    switch (msg) {
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lParam);
            const int list = ArrangeListOfId(header->idFrom);
            if (list < 0) {
                break;
            }

            if (header->code == LVN_KEYDOWN) {
                // The fast way: select an icon, press the number of the tray it
                // should go to. Repeating the key walks down the list.
                const WORD key = reinterpret_cast<NMLVKEYDOWN*>(lParam)->wVKey;
                int tray = -1;
                if (key >= L'1' && key <= L'9') {
                    tray = key - L'0';
                } else if (key >= VK_NUMPAD1 && key <= VK_NUMPAD9) {
                    tray = key - VK_NUMPAD0;
                }
                if (tray > 0) {
                    const int target = ArrangeListOfTray(tray);
                    if (target >= 0) {
                        ArrangeMoveSelected(list, target);
                    }
                } else if (key == L'H') {
                    ArrangeToggleHidden(list);
                } else if (key == VK_RETURN || key == VK_SPACE) {
                    ArrangeMoveSelected(list, ArrangeDefaultTarget(list));
                }
                return 0;
            }

            if (header->code == LVN_BEGINDRAG) {
                auto* view = reinterpret_cast<NMLISTVIEW*>(lParam);
                g_arrangeDragList = list;
                g_arrangeDragItem = view->iItem;
                g_arrangeDragging = true;
                SetCapture(hWnd);
                // Deliberately no ImageList drag image. ImageList_DragEnter with
                // a null window locks the screen DC, so a drag that cannot end -
                // and one did, under synthetic input during testing - takes the
                // whole desktop with it (DECISIONS 41). A cursor change says the
                // same thing and cannot wedge anything.
                SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                return 0;
            }

            if (header->code == NM_DBLCLK) {
                // The same move without the drag: quicker once you know, and it
                // works if the drag is ever awkward.
                auto* activate = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                ArrangeMove(ArrangeKeyOfItem(list, activate->iItem), list,
                            ArrangeDefaultTarget(list));
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE:
            if (g_arrangeDragging) {
                // The button going up without a WM_LBUTTONUP reaching here is
                // how a drag gets stuck holding capture. Checked every move
                // rather than trusted.
                if (!(GetKeyState(VK_LBUTTON) & 0x8000)) {
                    EndArrangeDrag(true);
                    return 0;
                }
                const int over = ArrangeListUnderCursor();
                SetCursor(LoadCursorW(
                    nullptr, (over >= 0 && over != g_arrangeDragList)
                                 ? IDC_SIZEALL
                                 : IDC_NO));
            }
            return 0;

        case WM_LBUTTONUP:
            EndArrangeDrag(true);
            return 0;

        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            EndArrangeDrag(false);
            return 0;

        case WM_SETCURSOR:
            if (g_arrangeDragging) {
                return TRUE;  // the drag owns the cursor
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            EndArrangeDrag(false);
            g_arrangeWnd = nullptr;
            g_arrangeLists.clear();
            g_arrangeTrays.clear();
            g_arrangeKeys.clear();
            return 0;

        case WM_NCDESTROY:
            // The last message, after the lists have gone: nothing that could
            // still draw with the image list is left.
            if (g_arrangeImages) {
                ImageList_Destroy(g_arrangeImages);
                g_arrangeImages = nullptr;
            }
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// What the arrange window keeps beyond its own lifetime, released when the
// tray thread ends.
void ReleaseArrangeResources() {
    if (g_arrangeFont) {
        DeleteObject(g_arrangeFont);
        g_arrangeFont = nullptr;
    }
}

HWND CreateArrangeList(HWND parent, int id, int x, int y, int width,
                       int height) {
    HWND list = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
            LVS_SHAREIMAGELISTS,
        x, y, width, height, parent, reinterpret_cast<HMENU>(
                                         static_cast<UINT_PTR>(id)),
        nullptr, nullptr);
    if (!list) {
        return nullptr;
    }
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT |
                                                LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<PWSTR>(L"Icon");
    column.cx = width - 24;
    ListView_InsertColumn(list, 0, &column);
    return list;
}

void ShowArrangeWindow() {
    if (g_arrangeWnd && IsWindow(g_arrangeWnd)) {
        ShowWindow(g_arrangeWnd, SW_RESTORE);
        SetForegroundWindow(g_arrangeWnd);
        PopulateArrangeLists();
        return;
    }

    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);

    // The class is registered with the tray thread's others (TrayThreadProc).
    const std::vector<int> trays = AvailableTrayNumbers();
    const int lists = static_cast<int>(trays.size());

    constexpr int kMargin = 12;
    constexpr int kListWidth = 250;
    constexpr int kHeight = 460;
    RECT frame = {0, 0, kMargin + lists * (kListWidth + kMargin), kHeight};
    AdjustWindowRectEx(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_TOOLWINDOW);
    g_arrangeWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, kArrangeClassName, L"Split Tray - arrange icons",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr,
        ModuleInstance(), nullptr);
    if (!g_arrangeWnd) {
        Wh_Log(L"could not create the arrange window: %lu", GetLastError());
        return;
    }

    RECT client = {};
    GetClientRect(g_arrangeWnd, &client);
    const int labelHeight = 20;
    const int hintHeight = 52;
    const int listTop = kMargin + labelHeight;
    const int listHeight = client.bottom - listTop - kMargin - hintHeight - kMargin / 2;

    g_arrangeTrays = trays;
    g_arrangeLists.assign(trays.size(), nullptr);
    for (int i = 0; i < lists; i++) {
        const int number = trays[static_cast<size_t>(i)];
        std::wstring title = std::to_wstring(number) + L"  ";
        if (number <= 1) {
            title += L"Primary tray";
        } else {
            std::wstring label = TrayLabel(number);
            label[0] = static_cast<wchar_t>(towupper(label[0]));
            title += label;
        }
        const int x = kMargin + i * (kListWidth + kMargin);
        CreateWindowExW(0, L"STATIC", title.c_str(),
                        WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, x, kMargin,
                        kListWidth, labelHeight, g_arrangeWnd, nullptr, nullptr,
                        nullptr);
        g_arrangeLists[static_cast<size_t>(i)] = CreateArrangeList(
            g_arrangeWnd, kArrangeListIdBase + i, x, listTop, kListWidth, listHeight);
    }

    CreateWindowExW(0, L"STATIC",
                    L"Drag an icon to another tray, or select it and press that "
                    L"tray's number - hold the key to work down the list. H puts "
                    L"an icon in its tray's overflow or brings it back. "
                    L"Double-click or Enter moves it between the primary tray "
                    L"and tray 2. Every choice is remembered.",
                    WS_CHILD | WS_VISIBLE, kMargin, listTop + listHeight + 6,
                    client.right - kMargin * 2, hintHeight, g_arrangeWnd,
                    nullptr, nullptr, nullptr);

    // Same font the rest of the shell uses; the default is the 1990s one.
    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    if (!g_arrangeFont && SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                                sizeof(metrics), &metrics, 0)) {
        g_arrangeFont = CreateFontIndirectW(&metrics.lfMessageFont);
    }
    if (g_arrangeFont) {
        EnumChildWindows(
            g_arrangeWnd,
            [](HWND child, LPARAM param) -> BOOL {
                SendMessageW(child, WM_SETFONT, param, TRUE);
                return TRUE;
            },
            reinterpret_cast<LPARAM>(g_arrangeFont));
    }

    g_arrangeImages = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 8, 8);
    for (HWND list : g_arrangeLists) {
        ListView_SetImageList(list, g_arrangeImages, LVSIL_SMALL);
    }

    PopulateArrangeLists();
    ShowWindow(g_arrangeWnd, SW_SHOW);
    SetForegroundWindow(g_arrangeWnd);
    Wh_Log(L"arrange window opened with %d tray(s)", lists);
}

void CloseArrangeWindow() {
    if (g_arrangeWnd && IsWindow(g_arrangeWnd)) {
        DestroyWindow(g_arrangeWnd);
    }
}

// After a change to the icons or the trays. The lists follow the trays, so a
// window laid out for the old set is replaced. Otherwise they are filled again
// when asked, or when which icons there are or where has changed
// (ArrangeLayoutNow, DECISIONS 75) - not for every change of picture or
// tooltip, which would reset the user's selection several times a second, and
// not in the middle of a drag, whose end catches up (EndArrangeDrag).
void RefreshArrangeWindow(bool repopulate) {
    if (!g_arrangeWnd || !IsWindow(g_arrangeWnd)) {
        return;
    }
    if (AvailableTrayNumbers() != g_arrangeTrays) {
        CloseArrangeWindow();
        ShowArrangeWindow();
    } else if (!g_arrangeDragging &&
               (repopulate || ArrangeLayoutNow() != g_arrangeLayout)) {
        PopulateArrangeLists();
    }
}

void NotifyTrayWindow(UINT message) {
    HWND hWnd = g_trayWnd.load();
    if (hWnd) {
        PostMessageW(hWnd, message, 0, 0);
    }
}

// Every tray redraws: the floating panels on the tray thread, the embedded
// ones on the taskbar's.
void RequestRedrawOfAllTrays() {
    NotifyTrayWindow(WM_ST_REFRESH);
#ifndef SPLITTRAY_NO_XAML
    SplitTrayXaml::RequestEmbeddedRefresh();
#endif
}

// ============================================================================
// Section 7 - Click forwarding
//
// The notification-area callback protocol, as the real tray implements it.
// Version 0-3 icons get (uID, mouseMessage); version 4 icons get the anchor
// point in wParam and (mouseMessage, uID) in lParam.
// ============================================================================

struct TrayCallback {
    WPARAM wParam = 0;
    LPARAM lParam = 0;
};

// What the real tray posts to an icon's owner for one mouse message, in order
// (DECISIONS 62). Version 4 packs the anchor point into wParam and the message
// and uID into lParam; earlier versions send (uID, message). From version 3 a
// left button-up is followed by NIN_SELECT and a right one by WM_CONTEXTMENU -
// documented for version 4, but Explorer does it for 3 as well, and Cairo's
// ManagedShell, which reimplements the tray, does the same. Applications
// written to the newer protocol act on those, not on the raw buttons.
std::vector<TrayCallback> TrayCallbacksFor(UINT version,
                                           UINT uID,
                                           UINT mouseMessage,
                                           POINT screenPoint) {
    auto pack = [&](UINT message) {
        if (version >= NOTIFYICON_VERSION_4) {
            return TrayCallback{MAKEWPARAM(screenPoint.x, screenPoint.y),
                                MAKELPARAM(message, uID)};
        }
        return TrayCallback{static_cast<WPARAM>(uID), static_cast<LPARAM>(message)};
    };
    std::vector<TrayCallback> callbacks = {pack(mouseMessage)};
    if (version >= NOTIFYICON_VERSION) {
        if (mouseMessage == WM_LBUTTONUP) {
            callbacks.push_back(pack(NIN_SELECT));
        } else if (mouseMessage == WM_RBUTTONUP) {
            callbacks.push_back(pack(WM_CONTEXTMENU));
        }
    }
    return callbacks;
}

void ForwardClick(uint64_t serial, UINT mouseMessage, POINT screenPoint) {
    HWND ownerWnd = nullptr;
    UINT uID = 0;
    UINT callbackMessage = 0;
    UINT version = 0;
    int iconIndex;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        iconIndex = IndexOfSerialLocked(serial);
        if (iconIndex < 0) {
            return;  // gone since it was drawn
        }
        const auto& icon = g_icons[static_cast<size_t>(iconIndex)];
        ownerWnd = icon.ownerWnd;
        uID = icon.uID;
        callbackMessage = icon.callbackMessage;
        version = icon.version;
    }

    if (!callbackMessage || !IsWindow(ownerWnd)) {
        return;
    }

    // Applications put their context menu up with TrackPopupMenu, which needs the
    // owner to be allowed to take the foreground - the real tray does the same.
    DWORD pid = 0;
    GetWindowThreadProcessId(ownerWnd, &pid);
    if (pid) {
        AllowSetForegroundWindow(pid);
    }

    for (const TrayCallback& callback :
         TrayCallbacksFor(version, uID, mouseMessage, screenPoint)) {
        PostMessageW(ownerWnd, callbackMessage, callback.wParam, callback.lParam);
    }
    Wh_Log(L"forwarded 0x%X to icon %d (hWnd=%p uID=%u v%u)", mouseMessage,
           iconIndex, ownerWnd, uID, version);
}

// ============================================================================
// Section 8 - Interception
// ============================================================================

// Hands a stored payload to Explorer's own window procedure, bypassing our
// handler, and returns what Explorer answered. Runs on the taskbar thread, from
// inside our subclass.
LRESULT DeliverToShell(HWND shellTrayWnd,
                       HWND senderWnd,
                       const std::vector<BYTE>& payload) {
    COPYDATASTRUCT cds = {};
    cds.dwData = kTrayCopyDataId;
    cds.cbData = static_cast<DWORD>(payload.size());
    cds.lpData = const_cast<BYTE*>(payload.data());
    return DefSubclassProc(shellTrayWnd, WM_COPYDATA,
                           reinterpret_cast<WPARAM>(senderWnd),
                           reinterpret_cast<LPARAM>(&cds));
}

// ---------------------------------------------------------------------------
// Settling Explorer's tray (DECISIONS 58, 66, 68, 71)
//
// A move changes where an icon should be (shellTarget) and wakes the taskbar's
// thread, and nothing else. That thread then compares, icon by icon, where it
// should be with where Explorer has it (forwardedToShell), and hands Explorer
// what closes the gap: an add built from the icon as it is at that moment,
// followed by the version its application negotiated, or a delete. What
// Explorer answers is what is recorded.
//
// Moves used to be queued as finished records. A record made when the move was
// asked for missed what arrived while it waited - a new callback, a new
// version, swallowed because Explorer did not have the icon yet - and two moves
// of one icon could be queued in the wrong order, each prepared under the
// store's lock and queued after it was released: an old removal delivered
// after a newer add took the icon out of Explorer for good. Reading the store
// when the thread gets to it leaves no order to get wrong and nothing to go
// stale. The wake-up carries nothing, because any process on the desktop can
// post it.
//
// In between, an application's own messages are passed on by where the icon
// really is. One Explorer has not been handed yet swallows them, and they are
// in the add when it goes. So are ones that arrive while Explorer is taking it
// - Explorer may send messages of its own while it handles a record, and they
// come back through the subclass - by a second record, the whole icon again as
// a modify (shellBehind).
// ---------------------------------------------------------------------------

// How many times Explorer is asked to take an icon back, or its version,
// before it is left as Explorer has it (OwedToShell).
constexpr int kShellAttempts = 3;

void RecordShellHoldingLocked(MirroredIcon* icon, bool holds);

// The icon with this serial in either list, or null. Caller holds g_mutex.
MirroredIcon* FindIconBySerialLocked(uint64_t serial) {
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (auto& icon : *list) {
            if (icon.serial == serial) {
                return &icon;
            }
        }
    }
    return nullptr;
}

// Whether the taskbar's thread has something to hand Explorer for this icon.
// Caller holds g_mutex.
bool NeedsSettlingLocked(const MirroredIcon& icon) {
    if (icon.payload.empty()) {
        return false;
    }
    if (!icon.shellTarget) {
        // Always taken out: a ghost left in Explorer's tray costs more than a
        // refused call.
        return icon.forwardedToShell;
    }
    if (icon.shellUnconfirmed) {
        return true;
    }
    if (icon.forwardedToShell && !icon.shellBehind) {
        return false;
    }
    return icon.shellRefusals < kShellAttempts;
}

// An icon meant to be in Explorer's tray that Explorer would not take back.
// Its application's own adds and modifies go to Explorer, as they would with
// no mod at all: a modify for an icon Explorer does not have fails, and an
// application that recovers from that adds its icon again, which Explorer
// takes (RecordShellAnswerLocked).
bool OwedToShell(const MirroredIcon& icon) {
    return icon.shellTarget && !icon.forwardedToShell &&
           icon.shellRefusals >= kShellAttempts;
}

// What the taskbar's thread hands Explorer for one icon.
enum class ShellChange {
    Add,      // Explorer does not have it
    Update,   // Explorer has less of it than the store (shellBehind)
    Delete,   // Explorer has it and should not
    Confirm,  // whether Explorer has it, after refusing its add (shellUnconfirmed)
};

struct ShellDelivery {
    uint64_t serial = 0;
    HWND senderWnd = nullptr;
    ShellChange change = ShellChange::Delete;
    std::vector<BYTE> record;         // the add, the icon as a modify, or the delete
    std::vector<BYTE> versionRecord;  // after an add or update, when there is a version
    uint64_t revision = 0;            // the icon's, when the record was built
    // A copy of the icon's picture for Explorer to copy in turn, released once
    // the record has been delivered.
    OwnedIcon picture;
    bool pictureLost = false;  // the store had one, and copying it failed
    std::wstring exe;          // whose it is, for the log
};

// The next icon not in `skip` whose place in Explorer's tray is not where it
// should be, and what to hand Explorer for it. Caller holds g_mutex.
//
// An add is built from the folded record and drawn with a fresh copy of the
// mod's own picture, or with none when that cannot be copied (DECISIONS 72).
// It is followed by the version the application negotiated, since a re-added
// icon starts again at version 0. An update is the same, as a modify.
bool NextShellDeliveryLocked(const std::vector<uint64_t>& skip, ShellDelivery* out) {
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (const auto& icon : *list) {
            if (!NeedsSettlingLocked(icon) ||
                std::find(skip.begin(), skip.end(), icon.serial) != skip.end()) {
                continue;
            }
            out->serial = icon.serial;
            out->senderWnd = icon.ownerWnd;
            out->revision = icon.revision;
            out->exe = std::wstring(FileNameOf(icon.exePath));
            if (!icon.shellTarget) {
                out->change = ShellChange::Delete;
                out->record = PayloadWithMessage(icon.payload, NIM_DELETE);
                return true;
            }
            if (icon.shellUnconfirmed) {
                out->change = ShellChange::Confirm;
                out->record = ProbeRecordFor(icon.payload);
                return true;
            }
            out->change = icon.forwardedToShell ? ShellChange::Update : ShellChange::Add;
            out->picture = OwnedIcon::CopyOf(icon.icon);
            out->pictureLost = icon.icon && !out->picture.get();
            out->record = AddRecordFor(icon.payload, out->picture.get());
            if (out->change == ShellChange::Update) {
                out->record = PayloadWithMessage(out->record, NIM_MODIFY);
            }
            if (icon.version) {
                out->versionRecord = SetVersionRecordFor(icon.payload, icon.version);
            }
            return true;
        }
    }
    return false;
}

// What became of one delivery.
enum class ShellOutcome {
    Settled,   // Explorer has the icon, or has let it go, as it should
    Refused,   // Explorer would not take it back, or its version; asked again later
    GaveUp,    // ...kShellAttempts times, so it is left as Explorer has it
    Orphaned,  // taken back, for an icon removed while it was being handed over
};

// Records what Explorer did with `delivery`: whether it took the record, and
// the version after it. Caller holds g_mutex, on the taskbar's thread.
ShellOutcome RecordShellDeliveryLocked(const ShellDelivery& delivery,
                                       bool taken,
                                       bool versionTaken) {
    MirroredIcon* icon = FindIconBySerialLocked(delivery.serial);
    if (delivery.change == ShellChange::Delete) {
        // Explorer does not have it now, whatever it answered.
        if (icon) {
            icon->forwardedToShell = false;
            icon->shellBehind = false;
            icon->shellUnconfirmed = false;
        }
        return ShellOutcome::Settled;
    }
    if (delivery.change == ShellChange::Confirm) {
        // Taken, Explorer has it; refused, it has not, and the icon is left
        // to its application, which was told its add failed (DECISIONS 70).
        if (icon) {
            icon->shellUnconfirmed = false;
            RecordShellHoldingLocked(icon, taken);
        }
        return ShellOutcome::Settled;
    }
    if (!icon) {
        return taken && delivery.change == ShellChange::Add ? ShellOutcome::Orphaned
                                                            : ShellOutcome::Settled;
    }
    if (taken) {
        icon->forwardedToShell = true;
        // What arrived meanwhile was swallowed - Explorer did not have the
        // icon yet - and a version Explorer did not take is not its version.
        icon->shellBehind = icon->revision != delivery.revision || !versionTaken;
        if (versionTaken) {
            if (!icon->shellBehind) {
                icon->shellRefusals = 0;
            }
            return ShellOutcome::Settled;
        }
    } else {
        // An add refused, or an update for an icon Explorer turns out not to
        // have: either way, it does not have it.
        icon->forwardedToShell = false;
        icon->shellBehind = false;
    }
    icon->shellRefusals++;
    return icon->shellRefusals >= kShellAttempts ? ShellOutcome::GaveUp
                                                 : ShellOutcome::Refused;
}

// Set while SettleShellIcons runs. Taskbar's thread only.
bool g_settlingShell = false;

// Settles every icon whose place in Explorer's tray is not where it should be,
// through `deliver`, which hands Explorer a record and returns its answer. On
// the taskbar's thread; the tests pass a stand-in for Explorer. Returns whether
// it handed Explorer anything.
//
// Each icon is tried once per call. One Explorer refused is tried again on a
// later call - the tray thread's timer makes one every tick while anything is
// waiting - and after kShellAttempts is left as Explorer has it.
//
// Never inside itself. Explorer may run a message loop while it handles a
// record, and a wake-up posted meanwhile is dispatched from inside it: a round
// started there handed Explorer again what the round under way was handing it.
// What that wake-up was for is left to the round under way, or the next one.
template <typename Deliver>
bool SettleShellIcons(Deliver deliver) {
    if (g_settlingShell) {
        return false;
    }
    g_settlingShell = true;
    bool handedOver = false;
    std::vector<uint64_t> tried;
    for (;;) {
        ShellDelivery delivery;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!NextShellDeliveryLocked(tried, &delivery)) {
                break;
            }
        }
        tried.push_back(delivery.serial);
        handedOver = true;

        // Outside the lock: Explorer may send messages of its own while it
        // handles this one, and they come back through the subclass.
        bool taken = deliver(delivery.senderWnd, delivery.record) != FALSE;
        if (delivery.change == ShellChange::Add && !taken) {
            // Explorer refuses an add for an icon it has already. Asked to
            // update it instead, it says which it was.
            taken = deliver(delivery.senderWnd,
                            PayloadWithMessage(delivery.record, NIM_MODIFY)) != FALSE;
        }
        bool versionTaken = true;
        if ((delivery.change == ShellChange::Add || delivery.change == ShellChange::Update) &&
            taken && !delivery.versionRecord.empty()) {
            versionTaken = deliver(delivery.senderWnd, delivery.versionRecord) != FALSE;
        }

        ShellOutcome outcome;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            outcome = RecordShellDeliveryLocked(delivery, taken, versionTaken);
        }
        const DWORD uID = ReadDword(delivery.record.data(), wire::kUID);
        const wchar_t* exe = delivery.exe.empty() ? L"?" : delivery.exe.c_str();
        switch (outcome) {
            case ShellOutcome::Settled:
                break;
            case ShellOutcome::Refused:
                if (taken) {
                    Wh_Log(L"Explorer took back icon uID %u (%s) but not its version; "
                           L"asking again later",
                           uID, exe);
                } else {
                    Wh_Log(L"Explorer did not take back icon uID %u (%s); asking again "
                           L"later",
                           uID, exe);
                }
                break;
            case ShellOutcome::GaveUp:
                if (taken) {
                    Wh_Log(L"Explorer would not take the version of icon uID %u (%s); "
                           L"it is left at the one Explorer gives it",
                           uID, exe);
                } else {
                    Wh_Log(L"Explorer would not take back icon uID %u (%s); it is left "
                           L"to its application to add again",
                           uID, exe);
                }
                break;
            case ShellOutcome::Orphaned:
                // Its application removed it while Explorer was taking it.
                deliver(delivery.senderWnd,
                        PayloadWithMessage(delivery.record, NIM_DELETE));
                break;
        }
        if (delivery.pictureLost) {
            Wh_Log(L"the picture of icon uID %u (%s) could not be copied; Explorer "
                   L"was handed it without one",
                   uID, exe);
        }
    }
    g_settlingShell = false;
    return handedOver;
}

// As the mod unloads, on the taskbar's thread: every icon whose application is
// still there goes back to Explorer as it is by then, and the subclass stops
// keeping track, in one go (DECISIONS 68). Keeping track used to stop as soon
// as unloading began, while the mod's thread was still being stopped, and an
// application's messages then went to an Explorer that did not have the icon:
// one removed meanwhile was put back by the hand-back, and one changed
// meanwhile was put back as it had been. A message handled between a
// hand-back and the subclass letting go would be swallowed into a tray that is
// gone.
//
// What arrives while it runs is handed back by another round: a new icon,
// swallowed into a tray that is going, or a change to one Explorer was taking.
// A hand-back that arrives inside a round of settling waits for that round to
// end (OnShellTrayWake).
//
// Only icons the mod took away are handed back. One meant for Explorer all
// along that Explorer has already refused is left to its application, as
// before: found live, Explorer's own icons - its volume icon among them -
// register again when the mod is loaded into a running Explorer, and Explorer
// refuses them; handing them back meant three more refusals each, every time
// the mod unloaded.
template <typename Deliver>
void HandIconsBackToShell(Deliver deliver) {
    if (g_handedBack.load() || g_settlingShell) {
        return;
    }
    for (int round = 0; round < kShellAttempts; round++) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (auto* list : {&g_icons, &g_primaryOnly}) {
                for (auto& icon : *list) {
                    // An icon whose application has gone is left as it is.
                    const bool target =
                        IsWindow(icon.ownerWnd) ? true : icon.forwardedToShell;
                    if (target != icon.shellTarget) {
                        icon.shellTarget = target;
                        icon.shellRefusals = 0;
                    }
                }
            }
        }
        if (!SettleShellIcons(deliver)) {
            break;
        }
    }
    g_handedBack.store(true);
}

// What a wake-up does, on the taskbar's thread: settles what is waiting, or,
// once the mod is unloading, hands every icon back. Both after a round that
// unloading began in the middle of, since the hand-back may have been
// dispatched from inside it and waited (DECISIONS 68).
template <typename Deliver>
void OnShellTrayWake(Deliver deliver) {
    if (!g_unloading.load()) {
        SettleShellIcons(deliver);
    }
    if (g_unloading.load()) {
        HandIconsBackToShell(deliver);
    }
}

// Wakes the taskbar's thread to settle what is waiting. Posting fails only when
// the window has gone or its queue is full; the tray thread's timer wakes it
// again while anything is waiting (ReplayRoutingChanges).
void WakeReplayDelivery() {
    HWND shellTrayWnd = g_shellTrayWnd.load();
    if (shellTrayWnd && IsWindow(shellTrayWnd)) {
        PostMessageW(shellTrayWnd, GetReplayMessage(), 0, 0);
    }
}

// Whether Explorer has an icon, as its answer to its application - or to the
// mod asking on its behalf - says (DECISIONS 70). One Explorer does not have is
// left to its application, which has been told so, rather than handed to
// Explorer by the mod: its record may be made of modifies alone, which would
// recreate the icon without most of it (DECISIONS 49). Caller holds g_mutex.
void RecordShellHoldingLocked(MirroredIcon* icon, bool holds) {
    if (!holds) {
        icon->forwardedToShell = false;
        icon->shellBehind = false;
        icon->shellRefusals = kShellAttempts;
    } else if (!icon->forwardedToShell) {
        icon->forwardedToShell = true;
        icon->shellBehind = false;
        icon->shellRefusals = 0;
    }
}

// What Explorer answered an application's own add or modify, passed on to it.
// Returns whether the taskbar's thread has to ask Explorer about the icon.
//
// A refused add says nothing yet: Explorer refuses to add an icon it has
// already, which is every application's add when the mod is loaded into a
// running Explorer. It is asked with a modify on the taskbar's next round, not
// here: every application registers again at once then, shell32 gives up on a
// tray that keeps it waiting, and an application that does not retry - Tauri's
// tray library does not - loses its icon (DECISIONS 51). Asked here, every
// refused add made two calls into Explorer in that burst instead of one; live,
// with the question asked here, a log listener attached and the processor
// busy, Telemachus's icon was lost in two loads of six. Caller holds g_mutex,
// on the taskbar's thread.
bool RecordShellAnswerLocked(const TrayNotification& n, bool taken) {
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (auto& icon : *list) {
            if (!SameIcon(icon, n)) {
                continue;
            }
            if (!taken && n.message == NIM_ADD) {
                icon.shellUnconfirmed = true;
                return true;
            }
            // An add Explorer takes is a new icon there, at version 0; one it
            // refused because it has the icon already, above, leaves the
            // version as it was (DECISIONS 74).
            if (n.message == NIM_ADD) {
                icon.version = 0;
            }
            icon.shellUnconfirmed = false;
            RecordShellHoldingLocked(&icon, taken);
            return false;
        }
    }
    return false;
}

// Puts one icon where its destination says, given which trays exist now: into
// Explorer's tray or out of it, and into one of Split Tray's trays or none.
// Returns whether the taskbar's thread has anything to hand Explorer for it.
// Caller holds g_mutex, and re-splits the lists afterwards (ResplitStoreLocked).
bool ReconcileIconLocked(MirroredIcon& icon) {
    const RoutingPlan plan =
        PlanFor(icon.destination, TrayAvailableLocked(icon.destination.tray));
    if (plan.forwardToShell != icon.shellTarget && !icon.payload.empty()) {
        icon.shellTarget = plan.forwardToShell;
        icon.shellRefusals = 0;
    }
    // An icon its application hid (NIS_HIDDEN) stays out of Split Tray's trays
    // when the settings say to respect that, as it does on arrival.
    const bool appHidden = (icon.state & NIS_HIDDEN) != 0;
    const bool mirror = plan.mirror && !(appHidden && !g_settings.mirrorHiddenIcons);
    icon.shownTray = mirror ? plan.tray : 0;
    return NeedsSettlingLocked(icon);
}

// Moves icons between g_icons and g_primaryOnly to match their shownTray,
// keeping each list's order. Caller holds g_mutex.
void ResplitStoreLocked() {
    std::vector<MirroredIcon> shown;
    std::vector<MirroredIcon> primaryOnly;
    shown.reserve(g_icons.size() + g_primaryOnly.size());
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (auto& icon : *list) {
            (icon.shownTray > 0 ? shown : primaryOnly).push_back(std::move(icon));
        }
    }
    g_icons = std::move(shown);
    g_primaryOnly = std::move(primaryOnly);
}

// Moves icons between the trays after the monitor set changed, and forgets
// icons whose application has gone. Runs on the tray thread, every tick.
void ReplayRoutingChanges() {
    if (g_unloading.load()) {
        return;
    }

    bool settle = false;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        std::vector<int> wasAvailable;
        for (const auto& tray : g_trays) {
            wasAvailable.push_back(tray.available ? tray.number : -tray.number);
        }
        RecomputeGeometryLocked();
        std::vector<int> nowAvailable;
        for (const auto& tray : g_trays) {
            nowAvailable.push_back(tray.available ? tray.number : -tray.number);
        }

        // Drop icons whose owner has gone away.
        for (auto* list : {&g_icons, &g_primaryOnly}) {
            for (size_t i = list->size(); i-- > 0;) {
                if (!IsWindow((*list)[i].ownerWnd)) {
                    if ((*list)[i].icon) {
                        DestroyIcon((*list)[i].icon);
                    }
                    list->erase(list->begin() + static_cast<ptrdiff_t>(i));
                    changed = true;
                }
            }
        }

        for (auto* list : {&g_icons, &g_primaryOnly}) {
            for (auto& icon : *list) {
                const int before = icon.shownTray;
                settle = ReconcileIconLocked(icon) || settle;
                changed = changed || icon.shownTray != before;
            }
        }
        ResplitStoreLocked();

        if (wasAvailable != nowAvailable) {
            changed = true;
            Wh_Log(L"trays changed: %zu of Split Tray's tray(s) now, %zu shown",
                   g_trays.size(),
                   static_cast<size_t>(std::count_if(
                       g_trays.begin(), g_trays.end(),
                       [](const TrayTarget& tray) { return tray.available; })));
        }
        if (changed) {
            RecomputeGeometryLocked();
        }
    }

    if (settle) {
        WakeReplayDelivery();
    }
    if (changed) {
        // Among them icons whose application went without removing them,
        // which no message says (DECISIONS 75).
        NotifyTrayWindow(WM_ST_REFRESH);
#ifndef SPLITTRAY_NO_XAML
        SplitTrayXaml::RequestEmbeddedRefresh();
#endif
    }
}

// Moves a single icon to a tray and remembers it. Called from the trays' own
// menus, the arrange window and dragging.
//
// This is the same work ApplySettingsToTrackedIcons does for every icon at once:
// the routing decision is sticky, so moving an icon means replaying a stored
// payload into the shell as an add or retracting it as a delete, not waiting for
// the application to touch its icon again. Between two of Split Tray's trays
// nothing goes to the shell at all.
void MoveIconToTray(std::wstring_view key, Destination destination) {
    bool settle = false;
    bool found = false;
    std::wstring label;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RememberPlacement(key, destination);
        label = TrayLabelLocked(destination.tray);

        for (auto* list : {&g_icons, &g_primaryOnly}) {
            for (auto& icon : *list) {
                if (StableKeyOf(icon) != key) {
                    continue;
                }
                found = true;
                icon.destination = destination;
                icon.destinationDecided = true;
                settle = ReconcileIconLocked(icon) || settle;
            }
        }
        // The icon copy is kept either way: the arrange window lists icons in
        // every tray and needs something to draw.
        ResplitStoreLocked();
        RecomputeGeometryLocked();
    }

    if (settle) {
        WakeReplayDelivery();
    }

    Wh_Log(L"moved icon '%s' to %s%s", std::wstring(key).c_str(), label.c_str(),
           found ? L"" : L" (not currently present)");

    RequestRedrawOfAllTrays();
}

// Re-evaluates every tracked icon against freshly loaded settings, moving it
// between the lists and between the trays as needed. Runs on the tray thread.
void ApplySettingsToTrackedIcons() {
    bool settle = false;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecomputeGeometryLocked();

        for (auto* list : {&g_icons, &g_primaryOnly}) {
            for (auto& icon : *list) {
                // Placement first, not just the rules: a settings change used to
                // re-resolve straight from ResolveDestination, which threw away
                // every icon the user had moved by hand (DECISIONS 29). An icon
                // whose path is still unknown keeps its provisional tray.
                if (!icon.exePath.empty()) {
                    icon.destination =
                        ResolvePlacement(g_settings, StableKeyOf(icon), icon.exePath);
                    icon.destinationDecided = true;
                }
                settle = ReconcileIconLocked(icon) || settle;
            }
        }
        ResplitStoreLocked();
        RecomputeGeometryLocked();
    }

    if (settle) {
        WakeReplayDelivery();
    }
}

// Takes the mod's own copy of a picture an application sent: the application
// is free to destroy its own once the shell has answered. One that cannot be
// copied leaves the picture the icon had - it used to be let go first, which
// left the icon with none at all (DECISIONS 72). Caller holds g_mutex.
void TakePictureLocked(MirroredIcon* icon, HICON sent) {
    HICON copy = nullptr;
    if (sent) {
        copy = CopyIcon(sent);
        if (!copy) {
            return;
        }
    }
    if (icon->icon) {
        DestroyIcon(icon->icon);
    }
    icon->icon = copy;
}

// Updates the store from a parsed notification. Caller must hold g_mutex.
// Returns true if the secondary tray needs repainting. `outRetractFromShell`
// is set for an icon new to the mod that goes to one of its trays: Explorer
// may have it already, and is to be told to let it go (DECISIONS 64).
// `outRecordShellAnswer` is set for an add or modify passed on to Explorer,
// whose answer says whether it has the icon (RecordShellAnswerLocked).
bool ApplyNotificationLocked(const TrayNotification& n,
                             const std::vector<BYTE>& payload,
                             bool* outForwardToShell,
                             bool* outRetractFromShell = nullptr,
                             bool* outRecordShellAnswer = nullptr) {
    if (outRetractFromShell) {
        *outRetractFromShell = false;
    }
    if (outRecordShellAnswer) {
        *outRecordShellAnswer = false;
    }
    // Find the icon in whichever list it currently lives in.
    MirroredIcon* existing = nullptr;
    bool existingIsMirrored = false;
    for (auto& icon : g_icons) {
        if (SameIcon(icon, n)) {
            existing = &icon;
            existingIsMirrored = true;
            break;
        }
    }
    if (!existing) {
        for (auto& icon : g_primaryOnly) {
            if (SameIcon(icon, n)) {
                existing = &icon;
                break;
            }
        }
    }

    if (n.message == NIM_DELETE) {
        if (!existing) {
            *outForwardToShell = true;
            return false;
        }
        *outForwardToShell = existing->forwardedToShell;
        if (existing->icon) {
            DestroyIcon(existing->icon);
            existing->icon = nullptr;
        }
        if (existingIsMirrored) {
            g_icons.erase(g_icons.begin() +
                          (existing - g_icons.data()));
            return true;
        }
        g_primaryOnly.erase(g_primaryOnly.begin() +
                            (existing - g_primaryOnly.data()));
        return false;
    }

    if (n.message == NIM_SETVERSION) {
        // Recorded either way, because it is replayed after the icon is next
        // added to the shell. Passed on only to a shell that has the icon: one
        // that was never given it fails the call, and the application is then
        // told its tray does not support the version it asked for.
        if (existing) {
            existing->version = n.version;
            existing->revision++;
            *outForwardToShell = existing->forwardedToShell;
        } else {
            *outForwardToShell = true;
        }
        return false;
    }

    if (n.message != NIM_ADD && n.message != NIM_MODIFY) {
        *outForwardToShell = true;  // NIM_SETFOCUS and anything unknown
        return false;
    }

    // An icon known by its GUID can come back from a new window, when its
    // application restarts, and with a new uID. The folded record takes both
    // from every message (FoldTrayRecord), and the icon has to agree with it:
    // its owner is where clicks go, and the window whose end the watchdog takes
    // for the icon's. For any other icon these are what identified it anyway.
    if (existing) {
        existing->ownerWnd = n.ownerWnd;
        existing->uID = n.uID;
    }

    // Decide the destination once, when the icon first appears.
    Destination destination;
    bool forwardToShell;
    bool mirror;
    int mirrorTray;
    const bool wasSticky = (existing != nullptr);
    if (existing) {
        // An earlier guess made without an executable path is not a decision.
        // The first message that brings one settles it, and the move itself goes
        // through the settings replay rather than being done mid-message.
        if (!existing->destinationDecided && !n.exePath.empty()) {
            existing->exePath = n.exePath;
            existing->destination =
                ResolvePlacement(g_settings, StableKeyOf(*existing), n.exePath);
            existing->destinationDecided = true;
            g_routingNeedsReapply.store(true);
            Wh_Log(L"decide: uID=%u settled as %s once the path arrived (%s)",
                   n.uID, DestinationName(existing->destination).c_str(),
                   FileNameOf(n.exePath).empty()
                       ? L"?"
                       : std::wstring(FileNameOf(n.exePath)).c_str());
        }
        destination = existing->destination;
        const RoutingPlan plan =
            PlanFor(destination, TrayAvailableLocked(destination.tray));
        // By where the icon really is (DECISIONS 58) - or to Explorer when
        // Explorer would not take it back, and only its application can put it
        // there now (OwedToShell, DECISIONS 66).
        const bool owed = OwedToShell(*existing);
        forwardToShell = existing->forwardedToShell || owed;
        mirror = plan.mirror;
        mirrorTray = plan.tray;
    } else {
        // Remembered placement first: the rules only decide what the user has
        // never moved by hand.
        destination = ResolvePlacement(g_settings, StableKeyOf(n), n.exePath);
        const RoutingPlan plan =
            PlanFor(destination, TrayAvailableLocked(destination.tray));
        forwardToShell = plan.forwardToShell;
        mirror = plan.mirror;
        mirrorTray = plan.tray;
    }
    *outForwardToShell = forwardToShell;
    // Whatever Explorer answers is recorded, not only for an icon it had
    // refused: an application's own add it refused was recorded as there
    // (DECISIONS 70).
    if (outRecordShellAnswer) {
        *outRecordShellAnswer = forwardToShell;
    }
    // An add the mod answers itself is taken, and is a registration afresh:
    // version 0 until its application asks for another. An icon re-registered
    // by GUID from a new window kept the version the old one had asked for.
    // One passed on to Explorer is decided by Explorer's answer
    // (RecordShellAnswerLocked) (DECISIONS 74).
    if (existing && n.message == NIM_ADD && !forwardToShell) {
        existing->version = 0;
    }

    // Why this icon went where it did. A sticky decision and a fresh one that
    // happened to agree are indistinguishable from the outcome alone, and that
    // is the difference this is being used to find.
    if (n.message == NIM_ADD) {
        Wh_Log(L"decide: uID=%u hwnd=%p guid=%s path='%s' -> %s (%s)", n.uID,
               n.ownerWnd,
               n.hasGuid ? FormatGuidKey(n.guid).c_str() : L"(none)",
               n.exePath.c_str(), DestinationName(destination).c_str(),
               wasSticky ? L"sticky, from the existing entry" : L"fresh");
    }

    // Whether the icon is hidden once this message is applied - not whether
    // this message says so. A modify that leaves the state out, a new tooltip,
    // said nothing about it and was read as "not hidden", which put an icon its
    // application had hidden back in the tray.
    DWORD state = existing ? existing->state : 0;
    if (n.flags & NIF_STATE) {
        state = (state & ~n.stateMask) | (n.state & n.stateMask);
    }
    const bool hidden = (state & NIS_HIDDEN) != 0;
    if (mirror && hidden && !g_settings.mirrorHiddenIcons) {
        mirror = false;
    }

    if (!mirror) {
        // Track it anyway, so routing stays sticky and settings changes can move
        // it later without waiting for the application to touch its icon.
        if (existingIsMirrored && existing) {
            MirroredIcon moved = std::move(*existing);
            g_icons.erase(g_icons.begin() + (existing - g_icons.data()));
            g_primaryOnly.push_back(std::move(moved));
            existing = &g_primaryOnly.back();
        } else if (!existing) {
            MirroredIcon icon;
            icon.serial = g_nextSerial.fetch_add(1);
            icon.hasGuid = n.hasGuid;
            icon.guid = n.guid;
            icon.ownerWnd = n.ownerWnd;
            icon.uID = n.uID;
            icon.exePath = n.exePath;
            icon.destination = destination;
            icon.forwardedToShell = forwardToShell;
            icon.shellTarget = forwardToShell;
            icon.destinationDecided = !n.exePath.empty();
            g_primaryOnly.push_back(std::move(icon));
            if (outRetractFromShell) {
                *outRetractFromShell = !forwardToShell;
            }
            existing = &g_primaryOnly.back();
        }
        existing->shownTray = 0;
        FoldTrayRecord(&existing->payload, payload);
        existing->revision++;
        if (n.flags & NIF_MESSAGE) {
            existing->callbackMessage = n.callbackMessage;
        }
        // Kept here too, so a later reconcile knows the application hid it.
        if (n.flags & NIF_STATE) {
            existing->state = (existing->state & ~n.stateMask) | (n.state & n.stateMask);
        }
        // The tooltip and the icon are kept for icons in the primary tray too.
        // The secondary tray does not draw these, but the arrange window lists
        // them, and an icon the user cannot see is one they cannot pick out of a
        // list of twenty-odd. One HICON copy each is cheap.
        if (n.flags & NIF_TIP) {
            existing->tip = n.tip;
        }
        if (n.flags & NIF_ICON) {
            TakePictureLocked(existing, n.icon);
        }
        if (!n.exePath.empty()) {
            existing->exePath = n.exePath;
        }
        return existingIsMirrored;  // repaint only if it just left the tray
    }

    if (!existing) {
        MirroredIcon icon;
        icon.serial = g_nextSerial.fetch_add(1);
        icon.hasGuid = n.hasGuid;
        icon.guid = n.guid;
        icon.ownerWnd = n.ownerWnd;
        icon.uID = n.uID;
        icon.exePath = n.exePath;
        icon.destination = destination;
        icon.forwardedToShell = forwardToShell;
        icon.shellTarget = forwardToShell;
        icon.destinationDecided = !n.exePath.empty();
        g_icons.push_back(std::move(icon));
        if (outRetractFromShell) {
            *outRetractFromShell = !forwardToShell;
        }
        existing = &g_icons.back();
    } else if (!existingIsMirrored) {
        MirroredIcon moved = std::move(*existing);
        g_primaryOnly.erase(g_primaryOnly.begin() + (existing - g_primaryOnly.data()));
        g_icons.push_back(std::move(moved));
        existing = &g_icons.back();
    }
    existing->shownTray = mirrorTray;

    // NIF_* flags say which fields are meaningful; anything else keeps its old
    // value, exactly as the real tray behaves on a partial NIM_MODIFY.
    if (n.flags & NIF_ICON) {
        TakePictureLocked(existing, n.icon);
    }
    if (n.flags & NIF_TIP) {
        existing->tip = n.tip;
    }
    if (n.flags & NIF_MESSAGE) {
        existing->callbackMessage = n.callbackMessage;
    }
    if (n.flags & NIF_STATE) {
        existing->state = (existing->state & ~n.stateMask) | (n.state & n.stateMask);
    }
    if (!n.exePath.empty()) {
        existing->exePath = n.exePath;
    }
    FoldTrayRecord(&existing->payload, payload);
    existing->revision++;
    return true;
}

// ---------------------------------------------------------------------------
// Saying where an icon is
//
// Explorer answers Shell_NotifyIconGetRect for the icons it has, and an icon
// that lives only in one of Split Tray's trays is not one of them. The question
// failed, and Tauri's tray library, which asks before it handles any click,
// dropped every click on such an icon: Telemachus gave no menu and no window
// from the secondary tray, and worked again as soon as it was moved back. So
// the mod answers for those icons, with where it drew them (DECISIONS 52).
// ---------------------------------------------------------------------------

// The icon a question is about, as an index into g_icons, when the answer is
// the mod's to give; -1 when Explorer has the icon or nobody does. Caller holds
// g_mutex.
int SecondaryOnlyIconIndexLocked(const IconRectQuery& query) {
    for (size_t i = 0; i < g_icons.size(); i++) {
        if (SameIcon(g_icons[i], query)) {
            return g_icons[i].forwardedToShell ? -1 : static_cast<int>(i);
        }
    }
    return -1;
}

// Runs on the taskbar's thread, which is also the XAML thread (DECISIONS 37).
bool SecondaryIconScreenRect(const IconRectQuery& query, RECT* out) {
    uint64_t serial;
    std::wstring exe;
    bool firstAsk;
    bool embedded = false;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const int index = SecondaryOnlyIconIndexLocked(query);
        if (index < 0) {
            return false;
        }
        MirroredIcon& icon = g_icons[static_cast<size_t>(index)];
        serial = icon.serial;
        exe = FileNameOf(icon.exePath);
        firstAsk = !icon.askedWhere;
        icon.askedWhere = true;

        // Drawn either in a taskbar or in a floating panel, never both.
        const TrayTarget* tray = FindTrayLocked(icon.shownTray);
        embedded = tray && TrayEmbeddedLocked(*tray);
        auto layout = g_floatingLayouts.find(icon.shownTray);
        if (!embedded && layout != g_floatingLayouts.end()) {
            const auto inTray = IconsInTrayLocked(icon.shownTray);
            const auto at = std::find(inTray.begin(), inTray.end(),
                                      static_cast<size_t>(index));
            found = at != inTray.end() &&
                    CellScreenRect(layout->second,
                                   static_cast<int>(at - inTray.begin()),
                                   static_cast<int>(inTray.size()), out);
        }
    }
#ifndef SPLITTRAY_NO_XAML
    if (embedded) {
        found = SplitTrayXaml::IconScreenRect(serial, out);
    }
#else
    (void)serial;
#endif

    // Once per icon: applications ask on every click, and this is the
    // taskbar's thread (DECISIONS 51).
    if (firstAsk) {
        if (found) {
            Wh_Log(L"told %s where its icon is: (%ld,%ld)-(%ld,%ld)",
                   exe.empty() ? L"?" : exe.c_str(), out->left, out->top, out->right,
                   out->bottom);
        } else {
            Wh_Log(L"could not say where %s's icon is; Explorer will say it has none",
                   exe.empty() ? L"?" : exe.c_str());
        }
    }
    return found;
}

// Counts a call of the subclass for as long as it runs: unloading does not
// finish while one is under way on the taskbar's thread (DECISIONS 73).
struct SubclassCall {
    SubclassCall() { g_subclassDepth.fetch_add(1); }
    ~SubclassCall() { g_subclassDepth.fetch_sub(1); }
    SubclassCall(const SubclassCall&) = delete;
    SubclassCall& operator=(const SubclassCall&) = delete;
};

LRESULT CALLBACK ShellTrayWndSubclassProc(HWND hWnd,
                                          UINT msg,
                                          WPARAM wParam,
                                          LPARAM lParam,
                                          DWORD_PTR) {
    const SubclassCall call;

    if (msg == GetReplayMessage()) {
        // Only a wake-up: what to hand Explorer is read from the icon store,
        // and wParam and lParam mean nothing (DECISIONS 58, 66). While the
        // mod unloads, it is the hand-back (DECISIONS 68).
        OnShellTrayWake([hWnd](HWND senderWnd, const std::vector<BYTE>& record) {
            // The shell copies an icon's picture while it handles the message -
            // which is why an application may destroy its own straight after
            // Shell_NotifyIcon returns - so the delivery's copy can go after.
            return DeliverToShell(hWnd, senderWnd, record);
        });
        // Once every icon is back, the subclass takes itself off, here: from
        // the unloading thread that is a message this thread has to answer,
        // and one that did not answer held unloading for as long as it did
        // not (DECISIONS 73). On this thread it is a direct call.
        if (g_handedBack.load()) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, ShellTrayWndSubclassProc);
        }
        return 0;
    }

#ifndef SPLITTRAY_NO_XAML
    if (msg == GetXamlRefreshMessage()) {
        if (!g_unloading.load()) {
            SplitTrayXaml::OnIconStoreChanged();
        }
        return 0;
    }

    if (msg == GetAttachMessage()) {
        // On the taskbar's UI thread, which is the only place XAML may be
        // touched. Posted by the tray thread's timer until it takes.
        if (!g_unloading.load()) {
            SplitTrayXaml::TryAttachEmbeddedTray();
        }
        return 0;
    }

    if (msg == GetXamlRemoveMessage()) {
        SplitTrayXaml::RemoveEverything();
        return 0;
    }
#endif

    // Kept track of until every icon is back with Explorer, unloading or not
    // (DECISIONS 68).
    if (msg != WM_COPYDATA || g_handedBack.load()) {
        return DefSubclassProc(hWnd, msg, wParam, lParam);
    }

    auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);

    IconRectQuery rectQuery;
    if (cds && ParseIconRectQuery(cds->dwData, cds->lpData, cds->cbData, &rectQuery)) {
        RECT rect;
        if (SecondaryIconScreenRect(rectQuery, &rect)) {
            return IconRectReply(rect, rectQuery.part);
        }
        return DefSubclassProc(hWnd, msg, wParam, lParam);
    }

    TrayNotification n;
    if (!cds || !ParseTrayNotification(cds->dwData, cds->lpData, cds->cbData, &n)) {
        // Appbar traffic, in-proc load requests, anything unrecognised: untouched.
        return DefSubclassProc(hWnd, msg, wParam, lParam);
    }

    const BYTE* raw = static_cast<const BYTE*>(cds->lpData);
    std::vector<BYTE> payload(raw, raw + cds->cbData);

    bool forwardToShell = true;
    bool retractFromShell = false;
    bool recordShellAnswer = false;
    bool needsRepaint = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        FillMissingPathLocked(&n);
        needsRepaint = ApplyNotificationLocked(n, payload, &forwardToShell,
                                               &retractFromShell, &recordShellAnswer);
    }

    // Every arrival is logged, not only the ones routed away.
    //
    // Only swallowed icons used to be logged, so an icon that a rule should have
    // matched and did not was indistinguishable from an icon that never reached
    // the mod - which is precisely the question that could not be answered when
    // the SystemInformer rule appeared to do nothing. NIM_ADD and NIM_DELETE are
    // rare; NIM_MODIFY is not, so it is left out.
    if (n.message == NIM_ADD || n.message == NIM_DELETE) {
        std::wstring exe{SplitTray::FileNameOf(n.exePath)};
        if (exe.empty()) {
            exe = n.exePath.empty() ? L"(none)" : n.exePath;
        }
        Wh_Log(L"tray %s: uID=%u exe=%s tip='%s' -> %s",
               n.message == NIM_ADD ? L"add" : L"delete", n.uID, exe.c_str(),
               n.tip.c_str(),
               forwardToShell ? L"primary tray" : L"secondary tray only");
    }

    // An icon whose routing was settled late needs the same replay a settings
    // change performs, to retract it from the tray it was provisionally put in.
    if (g_routingNeedsReapply.exchange(false)) {
        NotifyTrayWindow(WM_ST_SETTINGS);
    }

    if (needsRepaint) {
        NotifyTrayWindow(WM_ST_REFRESH);
#ifndef SPLITTRAY_NO_XAML
        // This handler already runs on the taskbar's UI thread, which is the
        // XAML thread, so the embedded tray is updated here rather than
        // marshalled across.
        SplitTrayXaml::OnIconStoreChanged();
#endif
    } else if (n.message == NIM_ADD || n.message == NIM_DELETE) {
        // An icon of the main tray's coming or going repaints none of the
        // mod's trays, but the arrange window lists it (DECISIONS 75).
        NotifyTrayWindow(WM_ST_REFRESH);
    }

    if (forwardToShell) {
        const LRESULT answer = DefSubclassProc(hWnd, msg, wParam, lParam);
        if (recordShellAnswer) {
            bool ask;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                ask = RecordShellAnswerLocked(n, answer != FALSE);
            }
            // Posted, so it is handled once the messages waiting now are.
            if (ask) {
                WakeReplayDelivery();
            }
        }
        return answer;
    }

    // An icon new to the mod, going to one of its trays, may be in Explorer's
    // already. Loaded into a running Explorer - installed, updated, switched
    // off and on - the mod cannot know what Explorer holds: the icons an
    // earlier load put back as it unloaded, or ones registered before it was
    // there. Swallowing the message left those in the main tray as well, for
    // good. So Explorer is told to let it go; one it never had costs a refused
    // call (DECISIONS 64).
    if (retractFromShell) {
        DeliverToShell(hWnd, reinterpret_cast<HWND>(wParam),
                       PayloadWithMessage(payload, NIM_DELETE));
        // A modify for an icon the mod has never seen added is answered as
        // Explorer answers one for an icon it does not have. An application
        // that recovers from that - SystemInformer does - adds its icon again,
        // with the callback and everything else a modify leaves out: what a
        // click in Split Tray's tray is sent with, and what puts the icon back
        // whole when the mod unloads. Saying yes left it with neither - its
        // icons did nothing when clicked, and Explorer refused them back.
        if (n.message == NIM_MODIFY) {
            return FALSE;
        }
    }

    // Swallowed: the icon lives only in the secondary tray. Shell_NotifyIcon
    // reports success to the caller, which is what the real tray would do.
    //
    // Not logged for NIM_MODIFY. SystemInformer sends four a second, and this
    // runs on the taskbar's own thread: with a log viewer attached each line
    // costs tens of milliseconds there, time in which every other
    // application's tray messages wait - and shell32 gives up on a tray that
    // does not answer, so an icon arriving then is lost. Adds and deletes are
    // logged above already (DECISIONS 44).
    if (n.message != NIM_MODIFY) {
        Wh_Log(L"routed away from primary tray: msg=%u uID=%u exe=%s", n.message,
               n.uID, n.exePath.c_str());
    }
    return TRUE;
}

void RequestIconRepopulation();

// Finds the tray window that shell32 targets. Secondary taskbars use the class
// Shell_SecondaryTrayWnd and never receive notification-area messages.
HWND FindShellTrayWindow() {
    return FindWindowW(L"Shell_TrayWnd", nullptr);
}

// Returns true only when this call attached to a tray window the mod was not
// already watching.
//
// Only once the tray thread is running (DECISIONS 69). Without it nothing
// draws the mod's trays, and an icon swallowed into one is lost. Wh_ModInit
// took the end of its wait for the thread as leave to attach, and a thread
// that went on to fail left a subclass swallowing icons into trays nothing
// drew; one still starting attaches from its own timer once it runs.
bool SubclassShellTrayWindow() {
    if (g_trayThreadState.load() != TrayThreadState::Running) {
        return false;
    }
    HWND hWnd = FindShellTrayWindow();
    if (!hWnd || g_shellTrayWnd.load() == hWnd) {
        return false;
    }
    if (!WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd,
                                                      ShellTrayWndSubclassProc, 0)) {
        Wh_Log(L"failed to subclass Shell_TrayWnd %p", hWnd);
        return false;
    }
    g_shellTrayWnd.store(hWnd);
    Wh_Log(L"subclassed Shell_TrayWnd %p", hWnd);
    return true;
}

// Whether applications have to be asked to re-register their icons, now that the
// mod is watching a tray window.
//
// Explorer announces every taskbar it creates (TaskbarCreated) once its tray is
// ready, and every application re-registers in answer. The mod asked as well,
// as soon as it attached - which on an Explorer start is about 3 seconds in,
// against Explorer's own announcement at about 17. Everything registered twice,
// the first time into a tray that was not ready and dropped it; and an icon
// whose application answered only once was simply gone. Measured on this
// machine: Desk Tray's "WhatsApp (default)" answered the mod and not Explorer,
// was forwarded to the primary tray, and was not in it.
//
// So the mod asks only when Explorer will not: when it was loaded into an
// Explorer whose taskbar already existed, or when Explorer's announcement went
// out while the mod was not watching. Pure, so the three cases are tested.
bool ShouldAskAppsToReRegister(bool shellExistedAtLoad,
                               bool firstAttach,
                               bool missedShellAnnouncement) {
    return (firstAttach && shellExistedAtLoad) || missedShellAnnouncement;
}

std::atomic<bool> g_shellExistedAtLoad{false};
std::atomic<bool> g_attachedBefore{false};
std::atomic<bool> g_missedShellAnnouncement{false};
// The tray window the re-register question was last settled for, so it is
// asked once per window and not on every tick of the timer.
std::atomic<HWND> g_reRegisterSettledFor{nullptr};

UINT TaskbarCreatedMessage() {
    static const UINT msg = RegisterWindowMessageW(L"TaskbarCreated");
    return msg;
}

// Explorer said its taskbar is ready. If the mod is not watching that taskbar
// yet, every application has just re-registered where the mod cannot see it,
// and has to be asked again once it can.
void NoteShellAnnouncedTaskbar() {
    HWND watched = g_shellTrayWnd.load();
    const bool watching =
        watched && IsWindow(watched) && FindShellTrayWindow() == watched;
    if (!watching) {
        g_missedShellAnnouncement.store(true);
    }
    Wh_Log(L"Explorer announced its taskbar (TaskbarCreated)%s",
           watching ? L"" : L" before the mod was watching it");
}

// Keeps the mod attached to whatever tray window currently exists.
//
// This cannot be a one-shot at startup. Windhawk injects into explorer.exe before
// the shell has created its taskbar, so at Wh_ModInit there is usually no
// Shell_TrayWnd to find at all; and the taskbar is destroyed and recreated again
// later on some display, DPI and theme changes. Called from Wh_ModAfterInit and
// then from the tray thread's timer until it succeeds.
//
// Icons are collected only once the mod is actually watching: broadcasting
// TaskbarCreated before the subclass is in place asks every application to
// re-register into a tray the mod cannot see, which wastes the one chance to pick
// up icons that pre-date the mod.
void EnsureShellTrayWindowSubclassed() {
    if (g_unloading.load()) {
        return;
    }

    HWND watched = g_shellTrayWnd.load();
    if (watched && !(IsWindow(watched) && FindShellTrayWindow() == watched)) {
        Wh_Log(L"Shell_TrayWnd %p is gone; looking for the new one", watched);
        g_shellTrayWnd.store(nullptr);
        watched = nullptr;
    }

    if (!watched) {
        if (!SubclassShellTrayWindow()) {
            return;
        }
        watched = g_shellTrayWnd.load();
    }

    // Once per tray window. Wh_ModInit may have attached already - the case of
    // a mod loaded into a running Explorer - and that attachment is settled
    // here too; it used to return early and never ask, which was the one case
    // where asking is needed.
    if (g_reRegisterSettledFor.exchange(watched) == watched) {
        return;
    }
    const bool firstAttach = !g_attachedBefore.exchange(true);
    const bool ask = ShouldAskAppsToReRegister(g_shellExistedAtLoad.load(), firstAttach,
                                               g_missedShellAnnouncement.exchange(false));
    bool repopulate;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        repopulate = g_settings.repopulateOnLoad;
    }
    if (!ask) {
        Wh_Log(L"not asking applications to re-register: Explorer announces this "
               L"taskbar itself once it is ready");
    } else if (repopulate) {
        RequestIconRepopulation();
    } else {
        Wh_Log(L"applications would be asked to re-register, but repopulateOnLoad "
               L"is off");
    }
}

// Asks every application to re-register its tray icon. This is the message the
// shell broadcasts after an Explorer restart, so applications already handle it;
// it is the only supported way to learn about icons that existed before the mod
// loaded.
void RequestIconRepopulation() {
    const UINT taskbarCreated = TaskbarCreatedMessage();
    if (!taskbarCreated) {
        return;
    }
    DWORD recipients = BSM_APPLICATIONS;
    BroadcastSystemMessageW(BSF_IGNORECURRENTTASK | BSF_POSTMESSAGE, &recipients,
                           taskbarCreated, 0, 0);
    Wh_Log(L"broadcast TaskbarCreated to collect existing icons");
}

// ---------------------------------------------------------------------------
// Moving one cell along a row
//
// Pure, and deliberately on this side of the XAML guard so it can be tested.
// Reordering the tray by removing the dragged cell and re-inserting it is what
// stopped dragging working at all: an element that leaves the visual tree loses
// pointer capture, so the drag ended on its first step and the release that
// followed was taken for a click. The invariant the plan has to hold is that
// the dragged cell is never the one removed; the cells around it move instead.
// ---------------------------------------------------------------------------

struct CellShiftStep {
    size_t removeAt;
    size_t insertAt;
};

std::vector<CellShiftStep> PlanCellShift(size_t from, size_t to) {
    std::vector<CellShiftStep> steps;
    while (from != to) {
        // Walk the neighbour on the side we are heading for across to the far
        // side of the dragged cell. That advances the dragged cell one slot
        // without touching it.
        const size_t neighbour = (to > from) ? from + 1 : from - 1;
        steps.push_back({neighbour, from});
        from = neighbour;
    }
    return steps;
}

// Where a TaskbarHost keeps its root XAML element, read out of the first
// instructions of TaskbarHost::FrameHeight, which loads that very member:
//
//   48 83 EC xx    sub rsp, xx
//   48 83 C1 nn    add rcx, nn    <- nn is the offset
//
// Any other code is a layout this mod does not know, and the answer is "no"
// rather than a guess: the offset is dereferenced and called through, so a
// guess that is wrong after a Windows update is a crash inside Explorer
// (DECISIONS 61). The pattern comes from taskbar-start-button-position.
// How good an anchor an element of the tray is for the tree walk, best first.
// The row the tray goes into is found by walking up from the anchor
// (FindTrayRow), so the anchor has to be below the row: a tray icon, not the
// frame around them. SystemTrayFrame comes first in the tree and sits above
// the row, so walking up from it found no row at all and settled for the Grid
// beside the clock - where the tray went on every reload, when the icons exist
// already and only the tree walk can find them. The icon the constructor hook
// hands over on a fresh start is an IconView named SystemTrayIcon (DECISIONS
// 65).
int AnchorPreference(std::wstring_view className, std::wstring_view name) {
    if (className == L"SystemTray.IconView") {
        return name == L"SystemTrayIcon" ? 0 : 1;
    }
    if (className == L"SystemTray.SystemTrayFrame") {
        return 3;
    }
    return 2;
}

bool ElementOffsetFromFrameHeight(const BYTE* code, size_t* offset) {
    if (!code || !offset) {
        return false;
    }
    if (code[0] == 0x48 && code[1] == 0x83 && code[2] == 0xEC &&
        code[4] == 0x48 && code[5] == 0x83 && code[6] == 0xC1 && code[7] <= 0x7F) {
        *offset = code[7];
        return true;
    }
    return false;
}

}  // namespace SplitTray

// The test binaries define SPLITTRAY_NO_XAML: they cannot exercise any of this
// (no XAML island in a test process, no taskbar to attach to) and compiling nine
// WinRT projections roughly doubles every build in the test loop. The compile
// check and the DLL build - the two that decide whether the shipped mod is
// correct - always compile it.
#ifndef SPLITTRAY_NO_XAML

// ============================================================================
// Section 10 - The taskbar's XAML
//
// The Windows 11 taskbar is system XAML (Windows.UI.Xaml, not WinUI 3) hosted in
// a DesktopWindowXamlSource island inside explorer.exe, and there is no public
// API for reaching it.
//
// The XAML debugging route - registering a TAP with InitializeXamlDiagnosticsEx
// - was tried first and abandoned: it is a single-consumer-per-process resource,
// and windows-11-taskbar-styler holds it and challenges anyone else who asks.
// Using it would mean breaking that mod (DECISIONS.md 24).
//
// So this section does what every mod that manipulates SystemTray elements does
// instead: it hooks private symbols in Explorer's own DLLs. That needs no
// exclusive connection and coexists with the styler. It is the fragile part of
// the mod, which is why tools/check-symbols.py verifies every symbol against the
// live binaries at build time - a hook that fails to resolve is otherwise silent.
//
// Everything in this section runs on Explorer's XAML UI thread except the hook
// installation itself. XAML objects are not agile and must never be touched from
// the mod's tray thread.
// ============================================================================


// winbase.h defines GetCurrentTime as a macro, which collides with
// Windows.UI.Xaml.Media.Animation's Timeline::GetCurrentTime.
#undef GetCurrentTime

#include <winrt/base.h>

#include <winrt/Windows.Foundation.h>
// WriteableBitmap::PixelBuffer returns an IBuffer. Its accessors have deduced
// return types, so the consumer definitions have to be in scope before they can
// be called, not just the forward declaration the imaging projection pulls in.
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Interop.h>  // xaml_typename, for the popup style
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Input.h>

#include <robuffer.h>

#include <list>
#include <memory>

namespace SplitTrayXaml {

namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxm = winrt::Windows::UI::Xaml::Media;
namespace wuxmi = winrt::Windows::UI::Xaml::Media::Imaging;

using SplitTray::g_mutex;
using SplitTray::g_settings;
using SplitTray::g_unloading;

HMODULE GetCurrentModuleHandle() {
    HMODULE module;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           L"", &module)) {
        return nullptr;
    }
    return module;
}

// ---------------------------------------------------------------------------
// Tree dump (discovery aid)
//
// Explorer's taskbar XAML is undocumented and changes between Windows builds.
// Rather than hardcode a path through it, the mod can print the subtree it is
// looking at, so the element types and names it targets are chosen from what is
// actually there. Off by default; turned on with the dumpXamlTree setting.
// ---------------------------------------------------------------------------

std::wstring DescribeElement(wux::DependencyObject const& obj) {
    std::wstring description;
    try {
        description = winrt::get_class_name(obj).c_str();
    } catch (...) {
        description = L"<unknown type>";
    }
    if (auto element = obj.try_as<wux::FrameworkElement>()) {
        auto name = element.Name();
        if (!name.empty()) {
            description += L" name='";
            description += name.c_str();
            description += L"'";
        }
        WCHAR size[96];
        swprintf_s(size, L" %gx%g vis=%d", element.ActualWidth(),
                   element.ActualHeight(),
                   element.Visibility() == wux::Visibility::Visible ? 1 : 0);
        description += size;
    }
    return description;
}

void DumpSubtree(wux::DependencyObject const& obj, int depth, int maxDepth) {
    if (!obj || depth > maxDepth) {
        return;
    }
    std::wstring indent(static_cast<size_t>(depth) * 2, L' ');
    Wh_Log(L"[xaml] %s%s", indent.c_str(), DescribeElement(obj).c_str());

    const int count = wuxm::VisualTreeHelper::GetChildrenCount(obj);
    for (int i = 0; i < count; i++) {
        DumpSubtree(wuxm::VisualTreeHelper::GetChild(obj, i), depth + 1, maxDepth);
    }
}

// ---------------------------------------------------------------------------
// HICON to a XAML image source
//
// The floating renderer could hand an HICON straight to DrawIconEx. XAML cannot:
// it needs pixels. Converting properly matters more than it looks, because tray
// icons come in two historical shapes and getting the alpha wrong is not subtle
// - it shows up as a black box around every icon.
//
//   * A 32-bit icon carries its own alpha channel in the colour bitmap.
//   * An older icon has no alpha at all, and its transparency lives in a
//     separate 1bpp mask where a set bit means "transparent".
//
// GetDIBits reads the colour bitmap; if every alpha byte comes back zero the
// icon is one of the older ones and the alpha is rebuilt from the mask. The
// result is premultiplied, which is what WriteableBitmap expects.
// ---------------------------------------------------------------------------

struct IconPixels {
    int width = 0;
    int height = 0;
    std::vector<BYTE> bgra;  // top-down, premultiplied
};

bool ReadIconPixels(HICON icon, IconPixels* out) {
    if (!icon || !out) {
        return false;
    }

    ICONINFO info = {};
    if (!GetIconInfo(icon, &info)) {
        return false;
    }
    // GetIconInfo hands back two bitmaps that belong to the caller.
    struct BitmapGuard {
        HBITMAP colour;
        HBITMAP mask;
        ~BitmapGuard() {
            if (colour) DeleteObject(colour);
            if (mask) DeleteObject(mask);
        }
    } guard{info.hbmColor, info.hbmMask};

    BITMAP bitmap = {};
    // A monochrome icon has no colour bitmap; its mask holds the image stacked
    // above the mask, which is double height.
    const HBITMAP source = info.hbmColor ? info.hbmColor : info.hbmMask;
    if (!source || !GetObjectW(source, sizeof(bitmap), &bitmap)) {
        return false;
    }

    const int width = bitmap.bmWidth;
    const int height = info.hbmColor ? bitmap.bmHeight : bitmap.bmHeight / 2;
    if (width <= 0 || height <= 0 || width > 512 || height > 512) {
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return false;
    }
    struct DcGuard {
        HDC dc;
        ~DcGuard() {
            if (dc) ReleaseDC(nullptr, dc);
        }
    } dcGuard{screenDc};

    BITMAPINFO header = {};
    header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    header.bmiHeader.biWidth = width;
    header.bmiHeader.biHeight = -height;  // negative: top-down
    header.bmiHeader.biPlanes = 1;
    header.bmiHeader.biBitCount = 32;
    header.bmiHeader.biCompression = BI_RGB;

    std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
    if (!GetDIBits(screenDc, source, 0, static_cast<UINT>(height), pixels.data(),
                   &header, DIB_RGB_COLORS)) {
        return false;
    }

    // Older icons come back with a zero alpha channel throughout; their
    // transparency has to be taken from the mask instead.
    bool hasAlpha = false;
    for (size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] != 0) {
            hasAlpha = true;
            break;
        }
    }

    if (!hasAlpha && info.hbmMask) {
        BITMAPINFO maskHeader = header;
        std::vector<BYTE> maskPixels(static_cast<size_t>(width) * height * 4);
        if (GetDIBits(screenDc, info.hbmMask, 0, static_cast<UINT>(height),
                      maskPixels.data(), &maskHeader, DIB_RGB_COLORS)) {
            for (size_t i = 0; i < pixels.size(); i += 4) {
                // In the mask, white (non-zero) means transparent.
                pixels[i + 3] = maskPixels[i] ? 0 : 255;
            }
        } else {
            for (size_t i = 3; i < pixels.size(); i += 4) {
                pixels[i] = 255;
            }
        }
    }

    // WriteableBitmap treats its buffer as premultiplied BGRA; handing it
    // straight alpha leaves a dark fringe on every anti-aliased edge.
    for (size_t i = 0; i < pixels.size(); i += 4) {
        const unsigned alpha = pixels[i + 3];
        if (alpha == 255) {
            continue;
        }
        pixels[i + 0] = static_cast<BYTE>(pixels[i + 0] * alpha / 255);
        pixels[i + 1] = static_cast<BYTE>(pixels[i + 1] * alpha / 255);
        pixels[i + 2] = static_cast<BYTE>(pixels[i + 2] * alpha / 255);
    }

    out->width = width;
    out->height = height;
    out->bgra = std::move(pixels);
    return true;
}

// Builds a XAML image source from an icon. Must be called on the XAML thread.
wuxmi::WriteableBitmap IconToBitmap(HICON icon) {
    IconPixels pixels;
    if (!ReadIconPixels(icon, &pixels)) {
        return nullptr;
    }

    wuxmi::WriteableBitmap bitmap(pixels.width, pixels.height);
    auto buffer = bitmap.PixelBuffer();
    if (buffer.Capacity() < pixels.bgra.size()) {
        return nullptr;
    }

    // IBufferByteAccess is how the pixels are reached without a copy through a
    // WinRT collection.
    winrt::com_ptr<::Windows::Storage::Streams::IBufferByteAccess> access;
    if (FAILED(reinterpret_cast<::IUnknown*>(winrt::get_abi(buffer))
                   ->QueryInterface(winrt::guid_of<
                                        ::Windows::Storage::Streams::IBufferByteAccess>(),
                                    access.put_void()))) {
        return nullptr;
    }
    BYTE* target = nullptr;
    if (FAILED(access->Buffer(&target)) || !target) {
        return nullptr;
    }
    memcpy(target, pixels.bgra.data(), pixels.bgra.size());
    buffer.Length(static_cast<uint32_t>(pixels.bgra.size()));
    bitmap.Invalidate();
    return bitmap;
}

// ---------------------------------------------------------------------------
// Walking the tree
// ---------------------------------------------------------------------------

// Depth-first search for a descendant whose runtime class name matches, which is
// how the mod finds the containers it inserts into without hardcoding a path
// through Explorer's private tree.
wux::DependencyObject FindDescendantByType(wux::DependencyObject const& root,
                                           std::wstring_view typeName,
                                           int maxDepth = 24) {
    if (!root || maxDepth < 0) {
        return nullptr;
    }
    const int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; i++) {
        auto child = wuxm::VisualTreeHelper::GetChild(root, i);
        if (!child) {
            continue;
        }
        try {
            if (winrt::get_class_name(child) == typeName) {
                return child;
            }
        } catch (...) {
        }
        if (auto found = FindDescendantByType(child, typeName, maxDepth - 1)) {
            return found;
        }
    }
    return nullptr;
}

// The nearest ancestor that can actually hold arbitrary children. Custom tray
// controls are not all panels, so an insertion point has to be looked for rather
// than assumed.
wuxc::Panel FindInsertablePanel(wux::DependencyObject const& root, int maxDepth = 24) {
    if (!root || maxDepth < 0) {
        return nullptr;
    }
    const int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; i++) {
        auto child = wuxm::VisualTreeHelper::GetChild(root, i);
        if (!child) {
            continue;
        }
        if (auto panel = child.try_as<wuxc::Panel>()) {
            return panel;
        }
        if (auto found = FindInsertablePanel(child, maxDepth - 1)) {
            return found;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// What the mod is looking for
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Reaching the taskbar's XAML
//
// Not through XAML diagnostics: that is a single-consumer-per-process resource
// and windows-11-taskbar-styler holds it (DECISIONS.md 24). Instead the mod
// hooks private symbols in Explorer's own DLLs, which is what every mod that
// manipulates SystemTray elements does, and which coexists with the styler.
//
// Two separate jobs:
//
//   SystemTray.dll   IconView's constructor is the anchor. Every tray icon view
//                    Explorer creates runs through it, and the XAML element is
//                    the implementation object's projected interface. This is
//                    how the mod gets a live element to work from at all.
//
//   taskbar.dll      Turns a taskbar *window* into its XamlRoot, so an element
//                    can be matched to the taskbar that owns it by identity.
//                    Element -> window is not possible without diagnostics;
//                    window -> XamlRoot is (DECISIONS.md 27).
//
// Every symbol here is checked against the live binaries at build time by
// tools/check-symbols.py, because a hook that fails to resolve is silent.
// ---------------------------------------------------------------------------

// --- taskbar.dll ------------------------------------------------------------

void* g_CTaskBand_ITaskListWndSite_vftable = nullptr;
void* g_CSecondaryTaskBand_ITaskListWndSite_vftable = nullptr;

// GetTaskbarHost returns a std::shared_ptr by value, so on x64 it takes a
// hidden pointer to the caller's two-pointer result slot.
using CTaskBand_GetTaskbarHost_t = void*(WINAPI*)(void* pThis, void** result);
CTaskBand_GetTaskbarHost_t g_CTaskBand_GetTaskbarHost = nullptr;
CTaskBand_GetTaskbarHost_t g_CSecondaryTaskBand_GetTaskbarHost = nullptr;

using TaskbarHost_FrameHeight_t = int(WINAPI*)(void* pThis);
TaskbarHost_FrameHeight_t g_TaskbarHost_FrameHeight = nullptr;

using Ref_count_base_Decref_t = void(WINAPI*)(void* pThis);
Ref_count_base_Decref_t g_Ref_count_base_Decref = nullptr;

// --- SystemTray.dll ---------------------------------------------------------

using IconView_IconView_t = void*(WINAPI*)(void* pThis);
IconView_IconView_t g_IconView_IconView_Original = nullptr;

// --- state ------------------------------------------------------------------

std::atomic<bool> g_taskbarSymbolsHooked{false};
std::atomic<bool> g_systemTraySymbolsHooked{false};
std::atomic<bool> g_symbolFailureLogged{false};

bool g_loggedTargetStack = false;

// One display's tray, inside that display's taskbar. Taskbar-thread only, like
// the XAML it holds.
struct EmbeddedTray {
    HMONITOR monitor = nullptr;
    int number = 0;  // its tray number, refreshed from g_trays on every sync

    // Resolved lazily on the taskbar's own UI thread, because touching XAML
    // from the mod's tray thread is not allowed. Cleared when the taskbar is
    // recreated.
    HWND taskbarWnd = nullptr;
    wux::XamlRoot root = nullptr;
    bool loggedTarget = false;

    winrt::weak_ref<wuxc::StackPanel> panel;
    bool active = false;
    bool loggedEmbedded = false;

    // Measured off an element of this taskbar (EnsureEmbeddedPanel), so a
    // taskbar at another scale, or resized by another mod, gets cells its own
    // size.
    double cellWidthDip = 32;
    double cellHeightDip = 38;
    double iconSizeDip = 16;

    // What was last drawn, so a refresh can tell an update from a change of
    // layout: every icon in the tray, then those on the bar and in the overflow.
    std::vector<uint64_t> drawnAll;
    std::vector<uint64_t> drawnShown;
    std::vector<uint64_t> drawnOverflow;
    // The icons behind the chevron, as its overflow popup draws them.
    std::vector<SplitTray::CellSnapshot> hiddenEntries;
};

std::vector<std::unique_ptr<EmbeddedTray>> g_embeddedTrays;

EmbeddedTray* TrayOfMonitor(HMONITOR monitor) {
    for (auto& tray : g_embeddedTrays) {
        if (tray->monitor == monitor) {
            return tray.get();
        }
    }
    return nullptr;
}

EmbeddedTray* TrayOfPanel(wuxc::StackPanel const& panel) {
    if (!panel) {
        return nullptr;
    }
    for (auto& tray : g_embeddedTrays) {
        if (tray->panel.get() == panel) {
            return tray.get();
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Window -> XamlRoot
// ---------------------------------------------------------------------------

// The TaskbarHost keeps its root XAML element at an offset which is not a
// stable part of any contract, so it is read out of TaskbarHost::FrameHeight
// (ElementOffsetFromFrameHeight). When that code is not what it was, the
// taskbar is left alone and the tray floats instead.
wux::XamlRoot XamlRootFromTaskbarHost(void* taskbarHostSharedPtr[2]) {
    if (!taskbarHostSharedPtr[0] && !taskbarHostSharedPtr[1]) {
        return nullptr;
    }

    size_t elementOffset = 0;
    const bool known =
        taskbarHostSharedPtr[0] &&
        SplitTray::ElementOffsetFromFrameHeight(
            reinterpret_cast<const BYTE*>(g_TaskbarHost_FrameHeight), &elementOffset);
    if (!known) {
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) {
            Wh_Log(L"[xaml] TaskbarHost::FrameHeight is not the code this mod "
                   L"knows; not embedding, the trays float instead");
        }
    }

    wux::FrameworkElement element = nullptr;
    if (known) {
        auto* elementUnknown = *reinterpret_cast<IUnknown**>(
            reinterpret_cast<BYTE*>(taskbarHostSharedPtr[0]) + elementOffset);
        if (elementUnknown) {
            elementUnknown->QueryInterface(winrt::guid_of<wux::FrameworkElement>(),
                                           winrt::put_abi(element));
        }
    }

    wux::XamlRoot result = nullptr;
    if (element) {
        try {
            result = element.XamlRoot();
        } catch (...) {
        }
    }

    // GetTaskbarHost handed back a shared_ptr; release the reference it took.
    if (taskbarHostSharedPtr[1] && g_Ref_count_base_Decref) {
        g_Ref_count_base_Decref(taskbarHostSharedPtr[1]);
    }
    return result;
}

// Walks a task band object to the sub-object whose vftable is the one for
// ITaskListWndSite, which is what GetTaskbarHost has to be called on.
void* TaskListWndSiteOf(void* taskBand, void* wantedVftable) {
    if (!taskBand || !wantedVftable) {
        return nullptr;
    }
    void* candidate = taskBand;
    for (int i = 0; *reinterpret_cast<void**>(candidate) != wantedVftable; i++) {
        if (i == 20) {
            return nullptr;
        }
        candidate = reinterpret_cast<void**>(candidate) + 1;
    }
    return candidate;
}

wux::XamlRoot XamlRootOfTaskbar(HWND taskbarWnd) {
    if (!taskbarWnd || !g_taskbarSymbolsHooked.load()) {
        return nullptr;
    }

    WCHAR className[64] = {};
    GetClassNameW(taskbarWnd, className, ARRAYSIZE(className));
    const bool secondary =
        _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0;

    HWND bandWnd;
    void* wantedVftable;
    CTaskBand_GetTaskbarHost_t getTaskbarHost;

    if (secondary) {
        bandWnd = FindWindowExW(taskbarWnd, nullptr, L"WorkerW", nullptr);
        wantedVftable = g_CSecondaryTaskBand_ITaskListWndSite_vftable;
        getTaskbarHost = g_CSecondaryTaskBand_GetTaskbarHost;
    } else {
        bandWnd = reinterpret_cast<HWND>(GetPropW(taskbarWnd, L"TaskbandHWND"));
        wantedVftable = g_CTaskBand_ITaskListWndSite_vftable;
        getTaskbarHost = g_CTaskBand_GetTaskbarHost;
    }

    if (!bandWnd || !wantedVftable || !getTaskbarHost) {
        return nullptr;
    }

    void* taskBand = reinterpret_cast<void*>(GetWindowLongPtrW(bandWnd, 0));
    void* site = TaskListWndSiteOf(taskBand, wantedVftable);
    if (!site) {
        Wh_Log(L"[xaml] could not find the ITaskListWndSite sub-object on %s",
               className);
        return nullptr;
    }

    void* taskbarHostSharedPtr[2] = {};
    getTaskbarHost(site, taskbarHostSharedPtr);
    return XamlRootFromTaskbarHost(taskbarHostSharedPtr);
}

// A display's taskbar window, if it has one: Windows can be set to show the
// taskbar on the main display only, and then there is nothing to embed into
// and the tray floats instead.
HWND FindTaskbarWindowOn(HMONITOR monitor) {
    if (!monitor) {
        return nullptr;
    }

    struct Search {
        HMONITOR wanted;
        HWND found;
    } search{monitor, nullptr};

    EnumWindows(
        [](HWND wnd, LPARAM param) -> BOOL {
            auto* search = reinterpret_cast<Search*>(param);
            WCHAR className[64] = {};
            GetClassNameW(wnd, className, ARRAYSIZE(className));
            if (_wcsicmp(className, L"Shell_SecondaryTrayWnd") != 0) {
                return TRUE;
            }
            if (MonitorFromWindow(wnd, MONITOR_DEFAULTTONULL) == search->wanted) {
                search->found = wnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));

    return search.found;
}

// Must run on the taskbar's UI thread.
wux::XamlRoot EnsureTargetXamlRoot(EmbeddedTray& tray) {
    if (tray.root && tray.taskbarWnd && IsWindow(tray.taskbarWnd)) {
        return tray.root;
    }

    tray.root = nullptr;
    tray.taskbarWnd = FindTaskbarWindowOn(tray.monitor);
    if (!tray.taskbarWnd) {
        return nullptr;
    }

    tray.root = XamlRootOfTaskbar(tray.taskbarWnd);
    if (tray.root && !tray.loggedTarget) {
        tray.loggedTarget = true;
        RECT rect = {};
        GetWindowRect(tray.taskbarWnd, &rect);
        Wh_Log(L"[xaml] tray %d: taskbar hwnd=%p (%ld,%ld)-(%ld,%ld), XamlRoot "
               L"resolved", tray.number, tray.taskbarWnd, rect.left, rect.top,
               rect.right, rect.bottom);
    }
    return tray.root;
}

void RemovePanel(EmbeddedTray& tray);

// Brings the embedded trays in line with the displays: one for every connected
// display's tray while embedding is on, and none otherwise. Numbers are taken
// afresh each time, since plugging a display in renumbers the trays after it.
void SyncEmbeddedTrays() {
    std::vector<std::pair<HMONITOR, int>> wanted;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_settings.embedInTaskbar && !g_unloading.load()) {
            for (const auto& tray : SplitTray::g_trays) {
                if (tray.forDisplay && tray.available) {
                    wanted.push_back({tray.monitor.handle, tray.number});
                }
            }
        }
    }

    for (auto it = g_embeddedTrays.begin(); it != g_embeddedTrays.end();) {
        const bool keep =
            std::any_of(wanted.begin(), wanted.end(),
                        [&](const auto& want) { return want.first == (*it)->monitor; });
        if (keep) {
            ++it;
            continue;
        }
        RemovePanel(**it);
        it = g_embeddedTrays.erase(it);
    }

    for (const auto& [monitor, number] : wanted) {
        if (EmbeddedTray* existing = TrayOfMonitor(monitor)) {
            existing->number = number;
            continue;
        }
        auto tray = std::make_unique<EmbeddedTray>();
        tray->monitor = monitor;
        tray->number = number;
        g_embeddedTrays.push_back(std::move(tray));
    }
}

// ---------------------------------------------------------------------------
// The anchor hook
// ---------------------------------------------------------------------------

// Keeps the Loaded revokers alive until they fire. A raw token would outlive
// the element and fire on a dead object.
std::list<wux::FrameworkElement::Loaded_revoker> g_loadedRevokers;

// Walks up to the named container an element sits in, the way Explorer's own
// tray elements are addressed - by name, not by position (DECISIONS.md 25).
wux::FrameworkElement AncestorNamed(wux::FrameworkElement const& element,
                                    std::wstring_view name) {
    wux::DependencyObject current = element;
    for (int depth = 0; depth < 32 && current; depth++) {
        if (auto asElement = current.try_as<wux::FrameworkElement>()) {
            if (asElement.Name() == name) {
                return asElement;
            }
        }
        current = wuxm::VisualTreeHelper::GetParent(current);
    }
    return nullptr;
}

bool EnsureEmbeddedPanel(EmbeddedTray& tray, wux::FrameworkElement const& anchor);
void RefreshEmbeddedTray();
void RefreshTray(EmbeddedTray& tray);

void OnTrayIconViewLoaded(wux::FrameworkElement const& iconView) {
    wux::XamlRoot elementRoot = nullptr;
    try {
        elementRoot = iconView.XamlRoot();
    } catch (...) {
        return;
    }
    if (!elementRoot) {
        return;
    }

    // Which display's taskbar it is on. Identity, not geometry: two taskbars on
    // identically sized monitors would be indistinguishable by size or scale.
    SyncEmbeddedTrays();
    EmbeddedTray* owner = nullptr;
    for (auto& tray : g_embeddedTrays) {
        auto root = EnsureTargetXamlRoot(*tray);
        if (root && root == elementRoot) {
            owner = tray.get();
            break;
        }
    }
    if (!owner) {
        return;  // the primary taskbar's, or one Split Tray has no tray on
    }

    std::wstring className;
    try {
        className = winrt::get_class_name(iconView).c_str();
    } catch (...) {
        return;
    }

    // Which named stack it landed in says what kind of tray slot it is.
    PCWSTR stackName = L"(none)";
    for (PCWSTR candidate : {L"MainStack", L"NonActivatableStack",
                             L"ControlCenterButton", L"NotificationCenterButton"}) {
        if (AncestorNamed(iconView, candidate)) {
            stackName = candidate;
            break;
        }
    }

    Wh_Log(L"[xaml] tray %d element: %s name='%s' in %s", owner->number,
           className.c_str(), iconView.Name().c_str(), stackName);

    // This element is on the tray's taskbar, so it is a usable anchor: its
    // parent panel is where the mod's tray goes, and its size is the native
    // cell size.
    if (EnsureEmbeddedPanel(*owner, iconView)) {
        RefreshTray(*owner);
    }

    bool dump;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        dump = g_settings.dumpXamlTree;
    }
    if (dump && !g_loggedTargetStack) {
        // Print from the tray frame down once, so the insertion point is chosen
        // from what is there rather than assumed.
        if (auto frame = AncestorNamed(iconView, L"SystemTrayFrameGrid")) {
            g_loggedTargetStack = true;
            Wh_Log(L"[xaml] ---- target taskbar tray subtree ----");
            DumpSubtree(frame, 0, 14);
            Wh_Log(L"[xaml] ---- end ----");
        } else {
            // No element of that name: walk to the top of the island instead.
            wux::DependencyObject root = iconView;
            for (int i = 0; i < 32; i++) {
                auto parent = wuxm::VisualTreeHelper::GetParent(root);
                if (!parent) {
                    break;
                }
                root = parent;
            }
            g_loggedTargetStack = true;
            Wh_Log(L"[xaml] ---- target island subtree (no SystemTrayFrameGrid) ----");
            DumpSubtree(root, 0, 14);
            Wh_Log(L"[xaml] ---- end ----");
        }
    }
}

void* WINAPI IconView_IconView_Hook(void* pThis) {
    void* result = g_IconView_IconView_Original(pThis);

    if (g_unloading.load()) {
        return result;
    }

    // The C++/WinRT implementation object carries its projected interface in the
    // second slot; this is how the reference mods reach the element.
    wux::FrameworkElement iconView = nullptr;
    try {
        auto** slots = reinterpret_cast<IUnknown**>(pThis);
        if (slots && slots[1]) {
            slots[1]->QueryInterface(winrt::guid_of<wux::FrameworkElement>(),
                                     winrt::put_abi(iconView));
        }
    } catch (...) {
        return result;
    }
    if (!iconView) {
        return result;
    }

    // XamlRoot is not available until the element is in a tree.
    try {
        g_loadedRevokers.emplace_back();
        auto revoker = std::prev(g_loadedRevokers.end());
        *revoker = iconView.Loaded(
            winrt::auto_revoke,
            [revoker](wf::IInspectable const& sender, wux::RoutedEventArgs const&) {
                g_loadedRevokers.erase(revoker);
                if (g_unloading.load()) {
                    return;
                }
                if (auto element = sender.try_as<wux::FrameworkElement>()) {
                    try {
                        OnTrayIconViewLoaded(element);
                    } catch (...) {
                        Wh_Log(L"[xaml] tray element handling failed: %08X",
                               winrt::to_hresult());
                    }
                }
            });
    } catch (...) {
        Wh_Log(L"[xaml] could not observe a tray element: %08X",
               winrt::to_hresult());
    }

    return result;
}

// ---------------------------------------------------------------------------
// The tray itself, inside the taskbar
//
// The insertion point is derived at runtime rather than written down: from a
// tray element known to be on the target taskbar, walk up to the first XAML
// Panel that can hold children, and put the mod's own panel at the front of it.
// That lands the icons to the left of the clock, which is where the native tray
// sits, without depending on the shape of Explorer's private tree.
//
// Sizes come from the anchor element too - its ActualWidth/ActualHeight are the
// real cell size in DIPs, already correct for the monitor's scaling and for
// whatever other taskbar mods have done to it. Measuring beats assuming 32x38.
//
// All of this runs on the taskbar's UI thread. That is also the thread the
// Shell_TrayWnd subclass runs on, so icon changes can update the XAML directly
// with no marshalling. Every taskbar is on that one thread (DECISIONS 37), so
// the same holds for every display's tray.
// ---------------------------------------------------------------------------

// Where the mod's panel belongs, and which sibling it goes in front of.
//
// The first attempt took the anchor's nearest Panel ancestor. On a secondary
// taskbar the anchor is the clock, and its nearest Panel ancestor is *inside the
// clock's own button* - so the icons became children of the clock and inherited
// its context menu.
//
// What is wanted is the tray row: the container the clock button itself sits in,
// so the mod's panel is the clock's sibling. Walking up keeps hold of the child
// it came through, which is the clock's top-level wrapper and therefore exactly
// the element to insert in front of.
//
// The row is recognised by the names Explorer gives it (DECISIONS.md 25), with
// the SystemTray.Stack class as a fallback, rather than by counting levels.
struct TrayRow {
    wuxc::Panel panel = nullptr;
    wux::UIElement insertBefore = nullptr;
};

std::wstring ClassNameOf(wux::DependencyObject const& object) {
    try {
        return winrt::get_class_name(object).c_str();
    } catch (...) {
        return L"?";
    }
}

bool g_loggedAncestorChain = false;

TrayRow FindTrayRow(wux::FrameworkElement const& anchor) {
    // The whole ancestor chain, so the decision can be made with all of it in
    // view rather than stopping at the first thing that looked plausible.
    std::vector<wux::DependencyObject> chain;
    chain.push_back(anchor);
    wux::DependencyObject current = wuxm::VisualTreeHelper::GetParent(anchor);
    for (int i = 0; i < 24 && current; i++) {
        chain.push_back(current);
        current = wuxm::VisualTreeHelper::GetParent(current);
    }

    if (!g_loggedAncestorChain) {
        g_loggedAncestorChain = true;
        // Printed once: if the tray lands in the wrong place again, this says
        // exactly what the walk had to choose from.
        for (size_t i = 0; i < chain.size(); i++) {
            auto element = chain[i].try_as<wux::FrameworkElement>();
            Wh_Log(L"[xaml]   ancestor %zu: %s name='%s'%s", i,
                   ClassNameOf(chain[i]).c_str(),
                   element ? element.Name().c_str() : L"",
                   chain[i].try_as<wuxc::StackPanel>()  ? L" [StackPanel]"
                   : chain[i].try_as<wuxc::Panel>()     ? L" [Panel]"
                                                        : L"");
        }
    }

    // The clock lives inside a SystemTray.OmniButton. Everything at or below the
    // highest such ancestor is that button's own template - inserting there is
    // what put the icons under its context menu. Start looking above it.
    size_t base = 0;
    for (size_t i = 0; i < chain.size(); i++) {
        const std::wstring name = ClassNameOf(chain[i]);
        if (name == L"SystemTray.OmniButton" || name == L"SystemTray.IconView") {
            base = i;
        }
    }

    TrayRow result;

    // Above the button, the first panel that lays children out in a line. A Grid
    // puts every child in the same cell, which is why the clock ended up behind
    // the icons rather than beside them.
    for (size_t i = base + 1; i < chain.size(); i++) {
        if (auto stack = chain[i].try_as<wuxc::StackPanel>()) {
            result.panel = stack;
            result.insertBefore = chain[i - 1].try_as<wux::UIElement>();
            return result;
        }
    }

    // No sequential panel anywhere above it. Any panel above the button still
    // beats being inside the button, but say so - it will overlap.
    for (size_t i = base + 1; i < chain.size(); i++) {
        if (auto panel = chain[i].try_as<wuxc::Panel>()) {
            Wh_Log(L"[xaml] no StackPanel above the tray button; falling back to "
                   L"%s, which may overlap the clock",
                   ClassNameOf(chain[i]).c_str());
            result.panel = panel;
            result.insertBefore = chain[i - 1].try_as<wux::UIElement>();
            return result;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Order, overflow and dragging
//
// Three things that belong together because they all act on the same list:
//
//   * A user-chosen order, which has to outlive the applications that own the
//     icons - so it is keyed on something stable, not on a window handle.
//   * An overflow flyout behind a chevron, the way the native tray hides icons
//     past a certain count.
//   * Dragging a cell to a new position, which rewrites that order.
//
// The store (section 5) is deliberately not involved. Order is a presentation
// concern; the routing model underneath has no opinion about it.
// ---------------------------------------------------------------------------

// The saved order, most-left first. Kept in the mod's own storage rather than in
// its settings: it is state the mod maintains, not something to hand-edit.
std::vector<std::wstring> g_iconOrder;
bool g_iconOrderLoaded = false;

constexpr PCWSTR kIconOrderValue = L"embeddedIconOrder";

void LoadIconOrder() {
    if (g_iconOrderLoaded) {
        return;
    }
    g_iconOrderLoaded = true;
    g_iconOrder.clear();

    WCHAR buffer[4096] = {};
    if (!Wh_GetStringValue(kIconOrderValue, buffer, ARRAYSIZE(buffer))) {
        return;
    }
    std::wstring_view remaining(buffer);
    while (!remaining.empty()) {
        const size_t split = remaining.find(L'\n');
        std::wstring_view entry = remaining.substr(0, split);
        if (!entry.empty()) {
            g_iconOrder.emplace_back(entry);
        }
        if (split == std::wstring_view::npos) {
            break;
        }
        remaining.remove_prefix(split + 1);
    }
}

void SaveIconOrder() {
    std::wstring joined;
    for (const auto& key : g_iconOrder) {
        if (!joined.empty()) {
            joined += L'\n';
        }
        joined += key;
        if (joined.size() > 3900) {  // storage is not unbounded
            break;
        }
    }
    Wh_SetStringValue(kIconOrderValue, joined.c_str());
}

size_t OrderPositionOf(std::wstring_view key) {
    for (size_t i = 0; i < g_iconOrder.size(); i++) {
        if (g_iconOrder[i] == key) {
            return i;
        }
    }
    return g_iconOrder.size();  // unknown icons go to the end, in store order
}

// ---------------------------------------------------------------------------
// Dragging
// ---------------------------------------------------------------------------

uint64_t SerialOfCell(wux::FrameworkElement const& cell);
void CommitVisualOrder(wuxc::StackPanel const& panel);

struct DragState {
    bool pointerDown = false;
    bool dragging = false;
    // Set when a drag ended some way other than a clean release - capture taken
    // away, say. Without it the release that follows looks like a click and
    // activates whatever icon the pointer was over, which is the worst possible
    // outcome of a failed drag.
    bool suppressClick = false;
    // The pointer has been dragged clear of the tray row, which means "move this
    // icon to the other tray" rather than "reorder it".
    bool outside = false;
    double startX = 0;
    // The row the press was in, and its cell size: a drag belongs to one tray.
    winrt::weak_ref<wuxc::StackPanel> panel;
    double cellWidthDip = 32;
    double cellHeightDip = 38;
    // Diagnostic only: keeps the move log to one line per press.
    bool loggedMove = false;
};

DragState g_drag;

// A refresh that arrived while the pointer was down, to run once it is released.
// See RefreshEmbeddedTray for why it waits.
bool g_refreshPending = false;

// Far enough that a click with a shaky hand is still a click.
constexpr double kDragThresholdDip = 5;

// Dragged this far above or below the row and the icon is leaving this tray.
constexpr double kDragOutThresholdDip = 28;

// Move the child at `from` to `to` without ever taking it out of the panel.
//
// The obvious implementation - remove the dragged cell and insert it at the new
// index - is why dragging did not work. An element that leaves the visual tree
// loses pointer capture, XAML raises PointerCaptureLost, the drag state is torn
// down, and the release that follows is taken for a click on whatever the icon
// was dropped on. The drag therefore died on its first step, every time.
//
// Walking the neighbour across the dragged cell has the same effect on the
// order while leaving the dragged cell itself in place, so its capture holds
// for the whole gesture.
void ShiftCellTo(wuxc::StackPanel const& panel, uint32_t from, uint32_t to) {
    auto children = panel.Children();
    for (const auto& step : SplitTray::PlanCellShift(from, to)) {
        auto element = children.GetAt(static_cast<uint32_t>(step.removeAt));
        children.RemoveAt(static_cast<uint32_t>(step.removeAt));
        children.InsertAt(static_cast<uint32_t>(step.insertAt), element);
    }
}

// The first slot a real icon may occupy: the chevron holds slot 0 when it is
// shown, and "show hidden icons" belongs at the start of the row, as it does in
// the native tray. The chevron is the only child without an icon in its Tag.
uint32_t FirstIconSlot(wuxc::StackPanel const& panel) {
    auto children = panel.Children();
    if (children.Size() == 0) {
        return 0;
    }
    auto first = children.GetAt(0).try_as<wux::FrameworkElement>();
    return (first && SerialOfCell(first) == 0) ? 1 : 0;
}


// ---------------------------------------------------------------------------
// Making the icons behave like tray icons
//
// Click forwarding itself is section 7 and is already tested; these are the
// pointer handlers that feed it. Events are marked handled so they do not bubble
// on to the taskbar, which is the other half of why the clock's menu was
// appearing.
// ---------------------------------------------------------------------------

wuxc::Border MakeCell(EmbeddedTray const& tray, HICON icon, std::wstring const& tip,
                      uint64_t serial, bool draggable);
void RefreshEmbeddedTray();
void ShowTrayContextMenu(wux::FrameworkElement const& target, uint64_t serial,
                         HMONITOR monitor);
void HideOverflowFlyout();
using SplitTray::ShiftHeld;

wuxm::SolidColorBrush TransparentBrush() {
    return wuxm::SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0});
}

wuxm::SolidColorBrush HoverBrush() {
    // Close to the native tray's hover wash: a low-alpha white over whatever the
    // taskbar material is.
    return wuxm::SolidColorBrush(winrt::Windows::UI::Color{36, 255, 255, 255});
}

wuxm::SolidColorBrush PressedBrush() {
    return wuxm::SolidColorBrush(winrt::Windows::UI::Color{64, 255, 255, 255});
}

// Which icon a cell draws: the serial it carries in its Tag (MirroredIcon::
// serial), or 0 for the chevron and the empty tray's handle.
uint64_t SerialOfCell(wux::FrameworkElement const& cell) {
    try {
        if (auto boxed = cell.Tag()) {
            return static_cast<uint64_t>(winrt::unbox_value<int64_t>(boxed));
        }
    } catch (...) {
    }
    return 0;
}

// The placement key of the icon with this serial, or empty if it has gone.
std::wstring KeyOfSerial(uint64_t serial) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const int index = SplitTray::IndexOfSerialLocked(serial);
    return index < 0 ? std::wstring()
                     : SplitTray::StableKeyOf(SplitTray::g_icons[static_cast<size_t>(index)]);
}

// The number of the tray on this display, or 0.
int TrayNumberOf(HMONITOR monitor) {
    EmbeddedTray* tray = TrayOfMonitor(monitor);
    return tray ? tray->number : 0;
}

// The real cursor position, which is what the tray callback protocol wants.
// The pointer args give island-relative device-independent coordinates, which
// would have to be converted twice to get back to the same number.
POINT CursorPoint() {
    POINT point = {};
    GetCursorPos(&point);
    return point;
}

// Records whether a display's tray is in its taskbar, so the tray thread knows
// whether it still needs a floating panel, and tells it to look.
void SetTrayEmbedded(EmbeddedTray& tray, bool embedded) {
    if (tray.active == embedded) {
        return;
    }
    tray.active = embedded;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (embedded) {
            SplitTray::g_embeddedMonitors.insert(tray.monitor);
        } else {
            SplitTray::g_embeddedMonitors.erase(tray.monitor);
        }
    }
    SplitTray::NotifyTrayWindow(SplitTray::WM_ST_REFRESH);
}

// Creates the mod's panel inside a display's taskbar, once. `anchor` must
// already be known to belong to that taskbar.
bool EnsureEmbeddedPanel(EmbeddedTray& tray, wux::FrameworkElement const& anchor) {
    if (auto existing = tray.panel.get()) {
        // Still parented? A taskbar rebuild drops it.
        if (wuxm::VisualTreeHelper::GetParent(existing)) {
            return true;
        }
        tray.panel = nullptr;
        tray.loggedEmbedded = false;
        tray.drawnAll.clear();
        tray.drawnShown.clear();
        tray.drawnOverflow.clear();
        SetTrayEmbedded(tray, false);
    }

    const TrayRow row = FindTrayRow(anchor);
    auto host = row.panel;
    if (!host) {
        Wh_Log(L"[xaml] no panel to insert into");
        return false;
    }

    // Size from the anchor's HEIGHT only.
    //
    // The anchor is whatever tray element loaded first on this taskbar, and on a
    // secondary taskbar that is the clock - a wide element (about 73x38 DIP),
    // not a square icon cell. Its width says nothing about how wide a
    // notification icon should be; taking it and halving it gave a 36 DIP icon
    // where the native one is 16.
    //
    // The heights do correspond: every element in the tray row is the same
    // height, so the anchor's height is the row height, and the native
    // proportions can be scaled off it. XAML works in device-independent pixels,
    // so these are the same numbers on a 96 DPI and a 120 DPI monitor - the
    // scaling is not this code's job.
    constexpr double kNativeCellWidthDip = 32;
    constexpr double kNativeCellHeightDip = 38;
    constexpr double kNativeIconDip = 16;

    const double anchorHeight = anchor.ActualHeight();
    double scale = 1.0;
    if (anchorHeight > 0) {
        scale = anchorHeight / kNativeCellHeightDip;
        // A taskbar resized by another mod is fine; a nonsense value is not.
        scale = std::clamp(scale, 0.5, 3.0);
    }
    tray.cellHeightDip = anchorHeight > 0 ? anchorHeight : kNativeCellHeightDip;
    tray.cellWidthDip = kNativeCellWidthDip * scale;
    tray.iconSizeDip = kNativeIconDip * scale;

    wuxc::StackPanel panel;
    panel.Orientation(wuxc::Orientation::Horizontal);
    panel.VerticalAlignment(wux::VerticalAlignment::Center);
    panel.Name(L"SplitTrayIcons");

    try {
        // In front of the element the walk came through - the clock's wrapper -
        // so the icons sit to its left, where the native tray is.
        uint32_t index = 0;
        if (!row.insertBefore ||
            !host.Children().IndexOf(row.insertBefore, index)) {
            index = 0;
        }
        host.Children().InsertAt(index, panel);
    } catch (...) {
        Wh_Log(L"[xaml] could not insert into the host panel: %08X",
               winrt::to_hresult());
        return false;
    }

    tray.panel = winrt::make_weak(panel);
    SetTrayEmbedded(tray, true);

    if (!tray.loggedEmbedded) {
        tray.loggedEmbedded = true;
        std::wstring hostClass = L"?";
        try {
            hostClass = winrt::get_class_name(host).c_str();
        } catch (...) {
        }
        auto hostElement = host.try_as<wux::FrameworkElement>();
        std::wstring anchorClass = L"?";
        try {
            anchorClass = winrt::get_class_name(anchor).c_str();
        } catch (...) {
        }
        // The anchor's own measurements are logged because the sizing is derived
        // from them: if the icons come out wrong, this line says why.
        Wh_Log(L"[xaml] inserted tray %d into %s name='%s'; anchor %s "
               L"is %.1fx%.1f DIP -> cell %.1fx%.1f, icon %.1f",
               tray.number, hostClass.c_str(),
               hostElement ? hostElement.Name().c_str() : L"",
               anchorClass.c_str(), anchor.ActualWidth(), anchor.ActualHeight(),
               tray.cellWidthDip, tray.cellHeightDip, tray.iconSizeDip);
    }
    return true;
}

// Saves the order of a tray's row, as the user has just dragged it. The order
// is one list for every tray: an icon's position only matters among the icons
// that share its tray, and keeping them in one list means an icon keeps its
// place when it is moved to another tray and back.
void CommitVisualOrder(wuxc::StackPanel const& panel) {
    LoadIconOrder();

    std::vector<std::wstring> order;
    auto children = panel.Children();
    for (uint32_t i = 0; i < children.Size(); i++) {
        auto cell = children.GetAt(i).try_as<wux::FrameworkElement>();
        if (!cell) {
            continue;
        }
        // The chevron carries no icon; only real icons take part in the order.
        if (const uint64_t serial = SerialOfCell(cell)) {
            std::wstring key = KeyOfSerial(serial);
            if (!key.empty()) {
                order.push_back(std::move(key));
            }
        }
    }

    // Anything not on screen right now - overflowed, or its application is not
    // running - keeps its saved position rather than being dropped.
    for (const auto& existing : g_iconOrder) {
        if (std::find(order.begin(), order.end(), existing) == order.end()) {
            order.push_back(existing);
        }
    }

    g_iconOrder = std::move(order);
    SaveIconOrder();
    Wh_Log(L"[xaml] icon order updated (%zu entries)", g_iconOrder.size());
}


// ---------------------------------------------------------------------------
// The tray's own context menu
//
// Plain right-click cannot be used: that belongs to the application that owns
// the icon, and taking it would break the thing tray icons are mostly for.
// Shift+right-click is the mod's own gesture instead, on an icon or on the
// chevron.
// ---------------------------------------------------------------------------

// The menu that makes a tray usable when it is empty.
//
// Without it there is no way into the mod at all from an empty tray: nothing is
// drawn, so there is nothing to right-click, and an icon can only arrive by a
// per-process rule the user has to write in the settings first.
void AppendMoveHereItems(wuxc::MenuFlyout const& menu, int trayNumber) {
    const auto known = SplitTray::KnownIcons();

    wuxc::MenuFlyoutSubItem submenu;
    submenu.Text(L"Move an icon to this tray");
    int offered = 0;
    for (const auto& entry : known) {
        if (entry.tray == trayNumber) {
            continue;
        }
        wuxc::MenuFlyoutItem item;
        item.Text(winrt::hstring{entry.label});
        const std::wstring key = entry.key;
        item.Click([key, trayNumber](wf::IInspectable const&,
                                     wux::RoutedEventArgs const&) {
            SplitTray::MoveIconToTray(key, SplitTray::Destination::Tray(trayNumber));
            RefreshEmbeddedTray();
        });
        submenu.Items().Append(item);
        offered++;
    }
    if (offered == 0) {
        wuxc::MenuFlyoutItem none;
        none.Text(L"Every icon is already here");
        none.IsEnabled(false);
        submenu.Items().Append(none);
    }
    menu.Items().Append(submenu);
}

// The handle shown when the tray holds nothing, so there is always somewhere to
// click. It is the only affordance on an empty tray.
wuxc::Border MakeTrayHandle(EmbeddedTray const& tray) {
    wuxc::Border handle;
    handle.Width(tray.cellWidthDip);
    handle.Height(tray.cellHeightDip);
    handle.Background(TransparentBrush());

    wuxc::TextBlock glyph;
    glyph.FontFamily(wuxm::FontFamily(L"Segoe Fluent Icons"));
    glyph.Text(L"");  // "See more"
    glyph.FontSize(tray.iconSizeDip * 0.75);
    glyph.Opacity(0.6);
    glyph.HorizontalAlignment(wux::HorizontalAlignment::Center);
    glyph.VerticalAlignment(wux::VerticalAlignment::Center);
    glyph.IsHitTestVisible(false);
    handle.Child(glyph);

    wuxc::ToolTipService::SetToolTip(
        handle, winrt::box_value(winrt::hstring{L"Split Tray - no icons here yet"}));

    handle.PointerEntered([](wf::IInspectable const& sender,
                             wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(HoverBrush());
        }
    });
    handle.PointerExited([](wf::IInspectable const& sender,
                            wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(TransparentBrush());
        }
    });
    handle.PointerPressed([](wf::IInspectable const&,
                             wux::Input::PointerRoutedEventArgs const& args) {
        args.Handled(true);  // do not let the taskbar underneath react
    });
    handle.PointerReleased([monitor = tray.monitor](
                               wf::IInspectable const& sender,
                               wux::Input::PointerRoutedEventArgs const& args) {
        args.Handled(true);
        if (auto border = sender.try_as<wux::FrameworkElement>()) {
            // Plain left-click: on the handle there is no application whose
            // right-click this would be taking, so it does not need Shift.
            ShowTrayContextMenu(border, 0, monitor);
        }
    });
    handle.DoubleTapped([](wf::IInspectable const&,
                           wux::Input::DoubleTappedRoutedEventArgs const& args) {
        args.Handled(true);
        SplitTray::NotifyTrayWindow(SplitTray::WM_ST_ARRANGE);
    });
    return handle;
}

// `serial` is the icon it was opened on, or 0 for the tray itself; `monitor`
// says which display's tray it was opened in.
void ShowTrayContextMenu(wux::FrameworkElement const& target, uint64_t serial,
                         HMONITOR monitor) {
    const std::wstring key = serial ? KeyOfSerial(serial) : std::wstring();
    const int here = TrayNumberOf(monitor);

    wuxc::MenuFlyout menu;

    if (!key.empty()) {
        // Every other tray there is to send it to, the primary one first.
        for (int target : SplitTray::AvailableTrayNumbers()) {
            if (target == here) {
                continue;
            }
            wuxc::MenuFlyoutItem move;
            move.Text(winrt::hstring{L"Move to " + SplitTray::TrayLabel(target)});
            move.Click([key, target](wf::IInspectable const&,
                                     wux::RoutedEventArgs const&) {
                SplitTray::MoveIconToTray(key, SplitTray::Destination::Tray(target));
                RefreshEmbeddedTray();
            });
            menu.Items().Append(move);
        }

        wuxc::MenuFlyoutItem hide;
        const bool hidden = SplitTray::IsIconHidden(key);
        hide.Text(hidden ? L"Show on the tray" : L"Hide in the overflow menu");
        hide.Click([key, hidden](wf::IInspectable const&,
                                 wux::RoutedEventArgs const&) {
            SplitTray::SetIconHidden(key, !hidden);
            RefreshEmbeddedTray();
        });
        menu.Items().Append(hide);
    }

    if (here > 0) {
        AppendMoveHereItems(menu, here);
    }

    {
        wuxc::MenuFlyoutSeparator separator;
        menu.Items().Append(separator);
    }

    // The reliable way to move icons, and the only one that works in both
    // directions: a window of the mod's own, where a drag is just a drag.
    wuxc::MenuFlyoutItem arrange;
    arrange.Text(L"Arrange icons…");
    arrange.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        SplitTray::NotifyTrayWindow(SplitTray::WM_ST_ARRANGE);
    });
    menu.Items().Append(arrange);

    wuxc::MenuFlyoutItem reset;
    reset.Text(L"Reset icon order");
    reset.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        g_iconOrder.clear();
        SaveIconOrder();
        RefreshEmbeddedTray();
        Wh_Log(L"[xaml] icon order reset");
    });
    menu.Items().Append(reset);

    wuxc::MenuFlyoutItem unhide;
    unhide.Text(L"Show every hidden icon");
    unhide.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        SplitTray::ShowAllHiddenIcons();
        RefreshEmbeddedTray();
    });
    menu.Items().Append(unhide);

    wuxc::MenuFlyoutItem forget;
    forget.Text(L"Reset moved icons");
    forget.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        {
            // Placements are otherwise only touched under g_mutex; the tray
            // thread may be re-resolving routing at this moment.
            std::lock_guard<std::mutex> lock(g_mutex);
            SplitTray::ForgetAllPlacements();
        }
        // Sends every icon back to where the rules put it, through the same
        // replay a settings change uses.
        SplitTray::NotifyTrayWindow(SplitTray::WM_ST_SETTINGS);
        RefreshEmbeddedTray();
    });
    menu.Items().Append(forget);

    try {
        menu.ShowAt(target);
    } catch (...) {
        Wh_Log(L"[xaml] could not show the tray menu: %08X", winrt::to_hresult());
    }
}

// ---------------------------------------------------------------------------
// A tray cell
// ---------------------------------------------------------------------------

// A cell for the icon with `serial`, sized for `tray`. The serial goes in the
// Tag, and everything a cell does - click, menu, drag - looks the icon up by it.
wuxc::Border MakeCell(EmbeddedTray const& tray,
                      HICON icon,
                      std::wstring const& tip,
                      uint64_t serial,
                      bool draggable) {
    const HMONITOR monitor = tray.monitor;
    wuxc::Border cell;
    cell.Width(tray.cellWidthDip);
    cell.Height(tray.cellHeightDip);
    cell.Background(TransparentBrush());  // a null background does not hit test
    // The hover and pressed washes are rounded, as the native tray's are.
    cell.CornerRadius(wux::CornerRadius{4, 4, 4, 4});
    cell.Tag(winrt::box_value(static_cast<int64_t>(serial)));

    if (icon) {
        if (auto bitmap = IconToBitmap(icon)) {
            wuxc::Image image;
            image.Source(bitmap);
            image.Width(tray.iconSizeDip);
            image.Height(tray.iconSizeDip);
            image.Stretch(wuxm::Stretch::Uniform);
            image.HorizontalAlignment(wux::HorizontalAlignment::Center);
            image.VerticalAlignment(wux::VerticalAlignment::Center);
            image.IsHitTestVisible(false);  // the cell handles the pointer
            cell.Child(image);
        }
    }

    if (!tip.empty()) {
        wuxc::ToolTipService::SetToolTip(
            cell, winrt::box_value(winrt::hstring{tip}));
    }

    cell.PointerEntered([](wf::IInspectable const& sender,
                           wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(HoverBrush());
        }
    });

    cell.PointerExited([](wf::IInspectable const& sender,
                          wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(TransparentBrush());
        }
    });

    cell.PointerCaptureLost([](wf::IInspectable const& sender,
                               wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(TransparentBrush());
            border.Opacity(1.0);
        }
        // Capture can be taken away mid-drag. Keep the rearranging done so far
        // rather than discarding it, and make sure the release that follows is
        // not mistaken for a click on whatever the icon was left sitting over.
        if (g_drag.dragging) {
            if (auto panel = g_drag.panel.get()) {
                CommitVisualOrder(panel);
            }
            g_drag.suppressClick = true;
            Wh_Log(L"[xaml][drag] capture lost mid-drag; order committed");
        }
        g_drag.pointerDown = false;
        g_drag.dragging = false;
        g_drag.outside = false;
        // Posted, not run here: rebuilding the panel from inside one of its
        // cells' own handlers would destroy the element that is handling it.
        if (g_refreshPending) {
            RequestEmbeddedRefresh();
        }
    });

    cell.PointerPressed([draggable](wf::IInspectable const& sender,
                                    wux::Input::PointerRoutedEventArgs const& args) {
        auto border = sender.try_as<wuxc::Border>();
        if (!border) {
            return;
        }
        border.Background(PressedBrush());
        // Handled here as well as on release: an unhandled press reaches the
        // taskbar underneath and opens the clock flyout.
        args.Handled(true);

        // Presses on a tray icon are rare enough to log every one.
        Wh_Log(L"[xaml][drag] PointerPressed on a cell (draggable=%d)",
               draggable ? 1 : 0);
        g_drag.loggedMove = false;

        g_drag.pointerDown = true;
        g_drag.dragging = false;
        g_drag.outside = false;
        g_drag.suppressClick = false;
        g_drag.panel = nullptr;
        if (draggable) {
            // The row this cell is in, and so the tray the drag belongs to.
            auto panel =
                wuxm::VisualTreeHelper::GetParent(border).try_as<wuxc::StackPanel>();
            if (EmbeddedTray* tray = TrayOfPanel(panel)) {
                g_drag.panel = winrt::make_weak(panel);
                g_drag.cellWidthDip = tray->cellWidthDip;
                g_drag.cellHeightDip = tray->cellHeightDip;
            } else {
                panel = nullptr;
            }
            if (panel) {
                try {
                    g_drag.startX = args.GetCurrentPoint(panel).Position().X;
                    // Without capture the pointer stops reporting the moment it
                    // leaves this 32-DIP cell, so a failure here is the whole
                    // gesture failing. Measured rather than assumed.
                    if (!border.CapturePointer(args.Pointer())) {
                        g_drag.pointerDown = false;
                        Wh_Log(L"[xaml][drag] CapturePointer refused; "
                               L"this icon cannot be dragged");
                    }
                } catch (...) {
                    g_drag.pointerDown = false;
                    Wh_Log(L"[xaml][drag] CapturePointer threw: %08X",
                           winrt::to_hresult());
                }
            }
        }
    });

    if (draggable) {
        cell.PointerMoved([](wf::IInspectable const& sender,
                             wux::Input::PointerRoutedEventArgs const& args) {
            // Once per press, not once per session. A one-shot probe is spent
            // by the first drag, which may well be one nobody was capturing -
            // and then every later attempt looks silent for the wrong reason.
            if (!g_drag.loggedMove) {
                g_drag.loggedMove = true;
                Wh_Log(L"[xaml][drag] PointerMoved on a cell (pointerDown=%d)",
                       g_drag.pointerDown ? 1 : 0);
            }
            if (!g_drag.pointerDown) {
                return;
            }
            auto border = sender.try_as<wuxc::Border>();
            auto panel = g_drag.panel.get();
            if (!border || !panel || g_drag.cellWidthDip <= 0) {
                return;
            }

            double x = 0;
            double y = 0;
            try {
                const auto position = args.GetCurrentPoint(panel).Position();
                x = position.X;
                y = position.Y;
            } catch (...) {
                return;
            }
            if (!g_drag.dragging &&
                std::abs(x - g_drag.startX) < kDragThresholdDip) {
                return;  // still a click, not yet a drag
            }
            if (!g_drag.dragging) {
                Wh_Log(L"[xaml][drag] started");
            }
            g_drag.dragging = true;
            args.Handled(true);

            // Dragged clear of the row: the gesture now means "send this icon
            // to the other tray", so stop reordering and say so by dimming it.
            const bool outside = (y < -kDragOutThresholdDip) ||
                                 (y > g_drag.cellHeightDip + kDragOutThresholdDip);
            if (outside != g_drag.outside) {
                g_drag.outside = outside;
                border.Opacity(outside ? 0.4 : 1.0);
            }
            if (outside) {
                return;
            }

            auto children = panel.Children();
            uint32_t current = 0;
            if (!children.IndexOf(border, current)) {
                return;
            }
            // Which slot the pointer is over now. The chevron keeps slot 0.
            const uint32_t firstSlot = FirstIconSlot(panel);
            int target = static_cast<int>(x / g_drag.cellWidthDip);
            target = std::clamp(target, static_cast<int>(firstSlot),
                                static_cast<int>(children.Size()) - 1);
            if (static_cast<uint32_t>(target) == current) {
                return;
            }
            try {
                ShiftCellTo(panel, current, static_cast<uint32_t>(target));
            } catch (...) {
                Wh_Log(L"[xaml][drag] reorder failed: %08X", winrt::to_hresult());
            }
        });
    }

    // `draggable` is false only for cells in the overflow popup.
    cell.PointerReleased([draggable, monitor](
                             wf::IInspectable const& sender,
                             wux::Input::PointerRoutedEventArgs const& args) {
        auto border = sender.try_as<wuxc::Border>();
        if (!border) {
            return;
        }
        border.Background(HoverBrush());
        border.Opacity(1.0);
        args.Handled(true);

        const bool wasDragging = g_drag.dragging;
        const bool wasOutside = g_drag.outside;
        const bool suppressed = g_drag.suppressClick;
        g_drag.pointerDown = false;
        g_drag.dragging = false;
        g_drag.outside = false;
        g_drag.suppressClick = false;
        // Whatever arrived while the pointer was down. Posted, so it runs after
        // this handler - including the click forwarded below, which has to
        // resolve its index against the layout the user actually clicked.
        if (g_refreshPending) {
            RequestEmbeddedRefresh();
        }
        try {
            border.ReleasePointerCapture(args.Pointer());
        } catch (...) {
        }

        // A drag is not also a click; forwarding one here would activate
        // whatever icon happened to be dropped on. That holds for a drag that
        // ended badly too - see PointerCaptureLost.
        if (wasDragging || suppressed) {
            if (wasDragging) {
                if (auto panel = g_drag.panel.get()) {
                    CommitVisualOrder(panel);
                }
            }
            // Dropped off the row: this is how an icon leaves for the primary
            // tray, and the choice is remembered like any other.
            if (wasOutside) {
                const std::wstring key = KeyOfSerial(SerialOfCell(border));
                if (!key.empty()) {
                    Wh_Log(L"[xaml][drag] dropped off the row: to the primary tray");
                    SplitTray::MoveIconToTray(key,
                                              SplitTray::Destination::Primary);
                    RefreshEmbeddedTray();
                }
            }
            return;
        }

        const uint64_t serial = SerialOfCell(border);
        if (!serial) {
            return;
        }

        UINT down = 0;
        UINT up = 0;
        try {
            const auto kind =
                args.GetCurrentPoint(nullptr).Properties().PointerUpdateKind();
            using winrt::Windows::UI::Input::PointerUpdateKind;
            switch (kind) {
                case PointerUpdateKind::LeftButtonReleased:
                    if (ShiftHeld()) {
                        // The quick way across: no menu, no window. Shift+right
                        // is already the mod's menu, so Shift+left being the
                        // mod's move keeps the pair together.
                        const std::wstring key = KeyOfSerial(serial);
                        if (!key.empty()) {
                            SplitTray::MoveIconToTray(
                                key, SplitTray::Destination::Primary);
                            RefreshEmbeddedTray();
                        }
                        return;
                    }
                    down = WM_LBUTTONDOWN;
                    up = WM_LBUTTONUP;
                    break;
                case PointerUpdateKind::RightButtonReleased:
                    if (ShiftHeld()) {
                        // The mod's own menu, not the application's.
                        ShowTrayContextMenu(border, serial, monitor);
                        return;
                    }
                    down = WM_RBUTTONDOWN;
                    up = WM_RBUTTONUP;
                    break;
                case PointerUpdateKind::MiddleButtonReleased:
                    down = WM_MBUTTONDOWN;
                    up = WM_MBUTTONUP;
                    break;
                default:
                    return;
            }
        } catch (...) {
            return;
        }

        const POINT point = CursorPoint();
        SplitTray::ForwardClick(serial, down, point);
        SplitTray::ForwardClick(serial, up, point);

        // A click in the overflow popup closes it, as the native one does:
        // the application is about to open a window or a menu of its own, and
        // the popup would otherwise sit over it.
        if (!draggable) {
            HideOverflowFlyout();
        }
    });

    cell.DoubleTapped([](wf::IInspectable const& sender,
                         wux::Input::DoubleTappedRoutedEventArgs const& args) {
        args.Handled(true);
        auto border = sender.try_as<wuxc::Border>();
        if (!border) {
            return;
        }
        if (const uint64_t serial = SerialOfCell(border)) {
            SplitTray::ForwardClick(serial, WM_LBUTTONDBLCLK, CursorPoint());
        }
    });

    return cell;
}

// ---------------------------------------------------------------------------
// The chevron and its flyout
// ---------------------------------------------------------------------------

// One popup for every tray: only one can be open at a time.
wuxc::Flyout g_overflowFlyout{nullptr};

void HideOverflowFlyout() {
    if (g_overflowFlyout) {
        try {
            g_overflowFlyout.Hide();
        } catch (...) {
        }
    }
}

wuxc::Border MakeChevron(EmbeddedTray const& tray) {
    const HMONITOR monitor = tray.monitor;
    wuxc::Border chevron;
    chevron.Width(tray.cellWidthDip);
    chevron.Height(tray.cellHeightDip);
    chevron.Background(TransparentBrush());
    chevron.CornerRadius(wux::CornerRadius{4, 4, 4, 4});

    wuxc::TextBlock glyph;
    glyph.FontFamily(wuxm::FontFamily(L"Segoe Fluent Icons"));
    glyph.Text(L"");  // chevron up, as the native "Show hidden icons" uses
    glyph.FontSize(tray.iconSizeDip * 0.75);
    glyph.HorizontalAlignment(wux::HorizontalAlignment::Center);
    glyph.VerticalAlignment(wux::VerticalAlignment::Center);
    glyph.IsHitTestVisible(false);
    chevron.Child(glyph);

    wuxc::ToolTipService::SetToolTip(chevron,
                                     winrt::box_value(winrt::hstring{L"Show hidden icons"}));

    chevron.PointerEntered([](wf::IInspectable const& sender,
                              wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(HoverBrush());
        }
    });
    chevron.PointerExited([](wf::IInspectable const& sender,
                             wux::Input::PointerRoutedEventArgs const&) {
        if (auto border = sender.try_as<wuxc::Border>()) {
            border.Background(TransparentBrush());
        }
    });
    chevron.PointerPressed([](wf::IInspectable const&,
                              wux::Input::PointerRoutedEventArgs const& args) {
        args.Handled(true);
    });
    chevron.PointerReleased([monitor](wf::IInspectable const& sender,
                                      wux::Input::PointerRoutedEventArgs const& args) {
        args.Handled(true);
        auto border = sender.try_as<wux::FrameworkElement>();
        EmbeddedTray* tray = TrayOfMonitor(monitor);
        if (!border || !tray || tray->hiddenEntries.empty()) {
            return;
        }

        if (ShiftHeld()) {
            ShowTrayContextMenu(border, 0, monitor);
            return;
        }

        // A grid of square cells, as the native overflow is: at most five to a
        // row, so a handful of icons makes a compact block rather than a strip.
        constexpr double kOverflowCellDip = 40;
        constexpr int kOverflowColumns = 5;
        wuxc::VariableSizedWrapGrid content;
        content.Orientation(wuxc::Orientation::Horizontal);
        content.ItemWidth(kOverflowCellDip);
        content.ItemHeight(kOverflowCellDip);
        content.MaximumRowsOrColumns(std::min<int>(
            kOverflowColumns,
            std::max<int>(1, static_cast<int>(tray->hiddenEntries.size()))));
        for (const auto& entry : tray->hiddenEntries) {
            // Not draggable in the flyout: ordering happens in the tray itself,
            // where there is a row to drag along.
            auto cell = MakeCell(*tray, entry.icon.get(), entry.tip, entry.serial, false);
            cell.Width(kOverflowCellDip);
            cell.Height(kOverflowCellDip);
            content.Children().Append(cell);
        }

        // Held in a global rather than built fresh on the stack. A flyout that
        // goes out of scope as the handler returns is why it appeared for a
        // moment and vanished.
        if (!g_overflowFlyout) {
            g_overflowFlyout = wuxc::Flyout();

            // Its own window. By default a Flyout is drawn inside the XAML root
            // it belongs to, and this one belongs to the taskbar - an island
            // about 48 DIP tall. The popup was squeezed into that strip, which
            // is why it came out as a wide box with the icon pushed against the
            // bottom edge and cut off. The mod's MenuFlyout never had the
            // problem because menus default to a window of their own.
            g_overflowFlyout.ShouldConstrainToRootBounds(false);

            // The presenter's defaults are for a flyout with prose in it: a
            // minimum width, generous padding, square corners. None of that
            // suits a few icons.
            wux::Style presenter{
                winrt::xaml_typename<wuxc::FlyoutPresenter>()};
            auto setters = presenter.Setters();
            setters.Append(wux::Setter(wuxc::Control::PaddingProperty(),
                                       winrt::box_value(wux::Thickness{4, 4, 4, 4})));
            setters.Append(wux::Setter(wux::FrameworkElement::MinWidthProperty(),
                                       winrt::box_value(0.0)));
            setters.Append(wux::Setter(wux::FrameworkElement::MinHeightProperty(),
                                       winrt::box_value(0.0)));
            setters.Append(wux::Setter(
                wuxc::Control::CornerRadiusProperty(),
                winrt::box_value(wux::CornerRadius{8, 8, 8, 8})));
            g_overflowFlyout.FlyoutPresenterStyle(presenter);

            // Standard, not Transient: the pointer sequence that opened it
            // would otherwise light-dismiss it immediately.
            g_overflowFlyout.ShowMode(
                wuxc::Primitives::FlyoutShowMode::Standard);
            g_overflowFlyout.Placement(
                wuxc::Primitives::FlyoutPlacementMode::Top);
        }
        g_overflowFlyout.Content(content);
        try {
            g_overflowFlyout.ShowAt(border);
        } catch (...) {
            Wh_Log(L"[xaml] could not show the overflow flyout: %08X",
                   winrt::to_hresult());
        }
    });

    return chevron;
}

// ---------------------------------------------------------------------------
// Rebuilding the tray
// ---------------------------------------------------------------------------

// Swaps a cell's picture and tooltip without replacing the cell. A picture
// taken away is taken away; one that could not be copied is left as it is
// (CellPictureOf, DECISIONS 76).
void UpdateCellInPlace(EmbeddedTray const& tray, wuxc::Border const& cell,
                       SplitTray::CellSnapshot const& entry) {
    const HICON icon = entry.icon.get();
    const std::wstring& tip = entry.tip;
    const SplitTray::CellPicture picture = SplitTray::CellPictureOf(entry);
    if (picture == SplitTray::CellPicture::Clear) {
        if (cell.Child()) {
            cell.Child(nullptr);
        }
    } else if (picture == SplitTray::CellPicture::Replace) {
        if (auto bitmap = IconToBitmap(icon)) {
            if (auto image = cell.Child().try_as<wuxc::Image>()) {
                image.Source(bitmap);
            } else {
                wuxc::Image fresh;
                fresh.Source(bitmap);
                fresh.Width(tray.iconSizeDip);
                fresh.Height(tray.iconSizeDip);
                fresh.Stretch(wuxm::Stretch::Uniform);
                fresh.HorizontalAlignment(wux::HorizontalAlignment::Center);
                fresh.VerticalAlignment(wux::VerticalAlignment::Center);
                fresh.IsHitTestVisible(false);
                cell.Child(fresh);
            }
        }
    }
    // Only when the text changed: setting a tooltip closes one that is open,
    // and SystemInformer's change every second.
    std::wstring current;
    try {
        if (auto boxed = wuxc::ToolTipService::GetToolTip(cell)) {
            current = winrt::unbox_value_or<winrt::hstring>(boxed, L"").c_str();
        }
    } catch (...) {
    }
    if (current != tip) {
        wuxc::ToolTipService::SetToolTip(
            cell, tip.empty() ? nullptr : winrt::box_value(winrt::hstring{tip}));
    }
}

// Redraws one display's tray from the icon store.
//
// This used to clear the panel and rebuild every cell on every call - and it is
// called on every change to any icon in the tray. SystemInformer redraws four
// live graphs every second, so the cell under the pointer was being destroyed
// several times a second. That ends a press before it can become a drag (a
// destroyed element loses pointer capture), closes tooltips as they open, and
// would leave an open overflow popup pointing at the wrong icons. It is the most
// likely reason dragging on the taskbar never did anything.
//
// So a change of picture or tooltip is applied to the existing cells, and the
// panel is only rebuilt when the layout itself changes - which icons, in which
// order, on the bar or in the overflow. Nothing is rebuilt while the pointer is
// down; the refresh waits for the release.
void RefreshTray(EmbeddedTray& tray) {
    auto panel = tray.panel.get();
    if (!panel) {
        return;
    }
    if (g_drag.pointerDown || g_drag.dragging) {
        g_refreshPending = true;
        return;
    }

    using Entry = SplitTray::CellSnapshot;
    std::vector<Entry> entries = SplitTray::CellSnapshotsOf(tray.number);
    int maxVisible = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        maxVisible = g_settings.maxVisibleIcons;
    }

    LoadIconOrder();
    // Stable sort: icons the user has never moved keep the order the store gave
    // them, which is the order they registered in.
    std::stable_sort(entries.begin(), entries.end(),
                     [](Entry const& a, Entry const& b) {
                         return OrderPositionOf(a.key) < OrderPositionOf(b.key);
                     });

    std::vector<std::wstring> keys;
    std::vector<uint64_t> all;
    for (const auto& entry : entries) {
        keys.push_back(entry.key);
        all.push_back(entry.serial);
    }

    // Two reasons an icon is in the chevron: the user put it there, or the row
    // ran out of space - in that order (SplitBarAndOverflow).
    const SplitTray::BarSplit split = SplitTray::SplitBarAndOverflow(
        keys, [](std::wstring const& key) { return SplitTray::IsIconHidden(key); },
        maxVisible);
    const auto& shown = split.shown;
    const auto& overflow = split.overflow;

    tray.hiddenEntries.clear();
    std::vector<uint64_t> shownSerials;
    std::vector<uint64_t> overflowSerials;
    for (size_t i : overflow) {
        overflowSerials.push_back(entries[i].serial);
        // Kept for the popup, which is opened later; a shown icon is drawn now.
        tray.hiddenEntries.push_back(std::move(entries[i]));
    }
    for (size_t i : shown) {
        shownSerials.push_back(entries[i].serial);
    }

    try {
        auto children = panel.Children();

        const uint32_t chevronSlots = overflow.empty() ? 0 : 1;
        uint32_t expectedChildren =
            chevronSlots + static_cast<uint32_t>(shown.size());
        if (expectedChildren == 0) {
            expectedChildren = 1;  // the handle
        }
        const bool sameLayout = all == tray.drawnAll &&
                                shownSerials == tray.drawnShown &&
                                overflowSerials == tray.drawnOverflow &&
                                children.Size() == expectedChildren;
        if (sameLayout) {
            for (size_t n = 0; n < shown.size(); n++) {
                auto cell = children.GetAt(chevronSlots + static_cast<uint32_t>(n))
                                .try_as<wuxc::Border>();
                if (cell) {
                    UpdateCellInPlace(tray, cell, entries[shown[n]]);
                }
            }
            return;
        }

        // The layout changed, so an open overflow popup may now hold icons that
        // have moved on. Close it rather than leave it pointing at them.
        HideOverflowFlyout();
        tray.drawnAll = all;
        tray.drawnShown = shownSerials;
        tray.drawnOverflow = overflowSerials;

        children.Clear();

        // The chevron goes first, as it does in the native tray.
        if (!tray.hiddenEntries.empty()) {
            children.Append(MakeChevron(tray));
        }
        for (size_t i : shown) {
            children.Append(MakeCell(tray, entries[i].icon.get(), entries[i].tip,
                                     entries[i].serial, true));
        }
        // An empty tray still needs somewhere to click, or the mod is invisible
        // and unreachable until a rule happens to match something.
        if (children.Size() == 0) {
            children.Append(MakeTrayHandle(tray));
        }

        // Every rebuild is logged. They should be rare - a change of which
        // icons, their order, or bar versus overflow - and a line a second here
        // would mean the in-place path is not being taken.
        Wh_Log(L"[xaml] tray %d rebuilt: %zu icon(s), %zu shown, %zu hidden",
               tray.number, entries.size(), shown.size(), tray.hiddenEntries.size());
    } catch (...) {
        Wh_Log(L"[xaml] refreshing tray %d failed: %08X", tray.number,
               winrt::to_hresult());
    }
}

// Redraws every display's tray. Called whenever the store changes, and cheap
// when nothing about the layout did (RefreshTray).
void RefreshEmbeddedTray() {
    if (g_drag.pointerDown || g_drag.dragging) {
        g_refreshPending = true;
        return;
    }
    g_refreshPending = false;
    SyncEmbeddedTrays();
    for (auto& tray : g_embeddedTrays) {
        if (tray->active) {
            RefreshTray(*tray);
        }
    }
}

// ---------------------------------------------------------------------------
// Where an icon is on screen
// ---------------------------------------------------------------------------

// An element's bounds in screen pixels. The taskbar's XAML island fills the
// taskbar window - measured on this machine, its content bridge has exactly
// the window's rect on both taskbars - so the window's corner is the island's.
bool ElementScreenRect(EmbeddedTray const& tray, wux::FrameworkElement const& element,
                       RECT* out) {
    RECT window;
    if (!element || !tray.taskbarWnd || !GetWindowRect(tray.taskbarWnd, &window)) {
        return false;
    }
    try {
        auto root = element.XamlRoot();
        if (!root) {
            return false;
        }
        const wf::Rect bounds = element.TransformToVisual(nullptr).TransformBounds(
            wf::Rect{0, 0, static_cast<float>(element.ActualWidth()),
                     static_cast<float>(element.ActualHeight())});
        if (bounds.Width <= 0 || bounds.Height <= 0) {
            return false;
        }
        *out = SplitTray::IslandBoundsToScreen(POINT{window.left, window.top}, bounds.X,
                                               bounds.Y, bounds.Width, bounds.Height,
                                               root.RasterizationScale());
        return true;
    } catch (...) {
        return false;
    }
}

bool IconScreenRect(uint64_t serial, RECT* out) {
    for (auto& tray : g_embeddedTrays) {
        auto panel = tray->panel.get();
        if (!tray->active || !panel) {
            continue;
        }
        try {
            auto children = panel.Children();
            for (uint32_t i = 0; i < children.Size(); i++) {
                auto cell = children.GetAt(i).try_as<wux::FrameworkElement>();
                if (cell && SerialOfCell(cell) == serial) {
                    return ElementScreenRect(*tray, cell, out);
                }
            }
            // Not on the bar, so in the overflow, whose chevron goes first. A
            // click there closes the popup before the application gets to ask,
            // so the chevron is where the icon is by then.
            const bool hidden = std::any_of(
                tray->hiddenEntries.begin(), tray->hiddenEntries.end(),
                [serial](SplitTray::CellSnapshot const& entry) {
                    return entry.serial == serial;
                });
            if (hidden && children.Size() > 0) {
                return ElementScreenRect(
                    *tray, children.GetAt(0).try_as<wux::FrameworkElement>(), out);
            }
        } catch (...) {
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Attaching without waiting to be handed an element
//
// The mod used to reach the taskbar only through the IconView constructor hook,
// which sees elements built after the hook is installed and nothing that already
// exists. Resolving SystemTray.dll's symbols takes several seconds - measured at
// 8.2s after Wh_ModInit on this machine - and by then the secondary taskbar's
// tray is built and loaded. No element on it ever came through, EnsureEmbeddedPanel
// was never called, and the mod sat there with a healthy log and no tray. It had
// worked before only by winning that race.
//
// So the tree is walked downwards from the target taskbar's XamlRoot instead,
// which needs nothing to happen first, and it is retried on the timer.
// ---------------------------------------------------------------------------

// Breadth-first, so the shallowest matches come first, and bounded: the taskbar
// tree is not deep and an unbounded walk over a live tree is not worth the risk.
void CollectDescendants(wux::DependencyObject const& root,
                        std::vector<wux::FrameworkElement>* out,
                        size_t limit) {
    std::vector<wux::DependencyObject> level{root};
    for (int depth = 0; depth < 24 && !level.empty() && out->size() < limit;
         depth++) {
        std::vector<wux::DependencyObject> next;
        for (const auto& node : level) {
            int count = 0;
            try {
                count = wuxm::VisualTreeHelper::GetChildrenCount(node);
            } catch (...) {
                continue;
            }
            for (int i = 0; i < count && out->size() < limit; i++) {
                wux::DependencyObject child = nullptr;
                try {
                    child = wuxm::VisualTreeHelper::GetChild(node, i);
                } catch (...) {
                    continue;
                }
                if (!child) {
                    continue;
                }
                if (auto element = child.try_as<wux::FrameworkElement>()) {
                    out->push_back(element);
                }
                next.push_back(child);
            }
        }
        level = std::move(next);
    }
}

bool g_loggedAttachCandidates = false;

void TryAttachTray(EmbeddedTray& tray) {
    if (tray.active) {
        if (auto existing = tray.panel.get()) {
            if (wuxm::VisualTreeHelper::GetParent(existing)) {
                return;
            }
        }
        // The taskbar was rebuilt under us.
        tray.panel = nullptr;
        tray.drawnAll.clear();
        tray.drawnShown.clear();
        tray.drawnOverflow.clear();
        SetTrayEmbedded(tray, false);
    }

    auto targetRoot = EnsureTargetXamlRoot(tray);
    if (!targetRoot) {
        return;  // no taskbar on that display, or not built yet
    }

    wux::UIElement content = nullptr;
    try {
        content = targetRoot.Content();
    } catch (...) {
        return;
    }
    if (!content) {
        return;
    }

    std::vector<wux::FrameworkElement> all;
    try {
        CollectDescendants(content, &all, 600);
    } catch (...) {
        Wh_Log(L"[xaml] walking the target island failed: %08X",
               winrt::to_hresult());
        return;
    }

    // Anything the tray is made of can anchor it: FindTrayRow walks up from it,
    // and the sizing comes from its height. Tried icons first and the frame
    // last (AnchorPreference). The comment said so before the code did: they
    // were tried in tree order, which puts the frame first.
    std::vector<wux::FrameworkElement> candidates;
    for (const auto& element : all) {
        const std::wstring className = ClassNameOf(element);
        if (className.rfind(L"SystemTray.", 0) == 0) {
            candidates.push_back(element);
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](wux::FrameworkElement const& a, wux::FrameworkElement const& b) {
                         const winrt::hstring nameA = a.Name();
                         const winrt::hstring nameB = b.Name();
                         return SplitTray::AnchorPreference(ClassNameOf(a), nameA) <
                                SplitTray::AnchorPreference(ClassNameOf(b), nameB);
                     });

    if (!g_loggedAttachCandidates) {
        g_loggedAttachCandidates = true;
        Wh_Log(L"[xaml] attach: %zu element(s) in the target island, "
               L"%zu SystemTray.* candidate(s)", all.size(), candidates.size());
        for (size_t i = 0; i < candidates.size() && i < 24; i++) {
            Wh_Log(L"[xaml]   candidate %zu: %s name='%s' %.1fx%.1f", i,
                   ClassNameOf(candidates[i]).c_str(),
                   candidates[i].Name().c_str(),
                   candidates[i].ActualWidth(), candidates[i].ActualHeight());
        }
        if (candidates.empty()) {
            // Nothing recognisable: say what is actually there rather than
            // failing silently, which is how this went unnoticed once already.
            Wh_Log(L"[xaml] ---- target island subtree ----");
            DumpSubtree(content, 0, 14);
            Wh_Log(L"[xaml] ---- end ----");
        }
    }

    // Try them in turn. One that yields no row costs nothing; betting the whole
    // attach on a single guess is what needed fixing.
    for (const auto& candidate : candidates) {
        if (candidate.ActualHeight() <= 0) {
            continue;
        }
        if (EnsureEmbeddedPanel(tray, candidate)) {
            Wh_Log(L"[xaml] tray %d attached from the tree walk, anchored on %s",
                   tray.number, ClassNameOf(candidate).c_str());
            RefreshTray(tray);
            return;
        }
    }
}

// Every display's tray that is not in its taskbar yet. Posted by the tray
// thread's timer while any is waiting (AnyDisplayTrayWaitingToEmbed).
void TryAttachEmbeddedTray() {
    SyncEmbeddedTrays();
    for (auto& tray : g_embeddedTrays) {
        TryAttachTray(*tray);
    }
}

bool AnyDisplayTrayWaitingToEmbed() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_settings.embedInTaskbar) {
        return false;
    }
    for (const auto& tray : SplitTray::g_trays) {
        if (tray.forDisplay && tray.available &&
            !SplitTray::g_embeddedMonitors.count(tray.monitor.handle)) {
            return true;
        }
    }
    return false;
}

// Callable from any thread: posts to the window the mod subclasses, which lives
// on the taskbar's UI thread, because XAML objects are thread-affine.
void RequestEmbeddedRefresh() {
    HWND tray = SplitTray::g_shellTrayWnd.load();
    if (tray && IsWindow(tray)) {
        PostMessageW(tray, SplitTray::GetXamlRefreshMessage(), 0, 0);
    }
}

// Called from the Shell_TrayWnd subclass, which runs on this same UI thread,
// whenever the icon store changes.
void OnIconStoreChanged() {
    if (g_unloading.load() || g_embeddedTrays.empty()) {
        return;
    }
    RefreshEmbeddedTray();
}

// Takes a tray's panel back out of its taskbar: when its display goes, when
// embedding is turned off, and when the mod unloads, so no stray element is left
// in Explorer's taskbar until the next restart.
void RemovePanel(EmbeddedTray& tray) {
    auto panel = tray.panel.get();
    tray.panel = nullptr;
    tray.loggedEmbedded = false;
    tray.drawnAll.clear();
    tray.drawnShown.clear();
    tray.drawnOverflow.clear();
    tray.hiddenEntries.clear();
    SetTrayEmbedded(tray, false);
    if (!panel) {
        return;
    }
    try {
        if (auto host = wuxm::VisualTreeHelper::GetParent(panel)
                            .try_as<wuxc::Panel>()) {
            uint32_t index = 0;
            if (host.Children().IndexOf(panel, index)) {
                host.Children().RemoveAt(index);
            }
        }
    } catch (...) {
    }
}

// ---------------------------------------------------------------------------
// Installing the hooks
//
// Neither module is loaded when Windhawk injects, for the same reason the
// taskbar window does not exist yet (DECISIONS.md 18), so this is retried from
// the tray thread's timer rather than attempted once at startup.
// ---------------------------------------------------------------------------

bool HookTaskbarSymbols() {
    if (g_taskbarSymbolsHooked.load()) {
        return true;
    }
    HMODULE module = GetModuleHandleW(L"taskbar.dll");
    if (!module) {
        return false;
    }

    WindhawkUtils::SYMBOL_HOOK taskbarDllHooks[] = {
        {{LR"(const CTaskBand::`vftable'{for `ITaskListWndSite'})"},
         &g_CTaskBand_ITaskListWndSite_vftable},
        {{LR"(const CSecondaryTaskBand::`vftable'{for `ITaskListWndSite'})"},
         &g_CSecondaryTaskBand_ITaskListWndSite_vftable},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CTaskBand::GetTaskbarHost(void)const )"},
         &g_CTaskBand_GetTaskbarHost},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CSecondaryTaskBand::GetTaskbarHost(void)const )"},
         &g_CSecondaryTaskBand_GetTaskbarHost},
        {{LR"(public: int __cdecl TaskbarHost::FrameHeight(void)const )"},
         &g_TaskbarHost_FrameHeight},
        {{LR"(public: void __cdecl std::_Ref_count_base::_Decref(void))"},
         &g_Ref_count_base_Decref},
    };

    if (!WindhawkUtils::HookSymbols(module, taskbarDllHooks,
                                    ARRAYSIZE(taskbarDllHooks))) {
        if (!g_symbolFailureLogged.exchange(true)) {
            Wh_Log(L"[xaml] could not resolve taskbar.dll symbols; the embedded "
                   L"tray cannot find which taskbar an element belongs to");
        }
        return false;
    }

    g_taskbarSymbolsHooked.store(true);
    Wh_Log(L"[xaml] taskbar.dll symbols resolved");
    return true;
}

bool HookSystemTraySymbols() {
    if (g_systemTraySymbolsHooked.load()) {
        return true;
    }
    HMODULE module = GetModuleHandleW(L"SystemTray.dll");
    if (!module) {
        return false;
    }

    // Verified present in this exact binary by tools/check-symbols.py;
    // Taskbar.View.dll does not carry it (DECISIONS.md 26).
    WindhawkUtils::SYMBOL_HOOK systemTrayDllHooks[] = {
        {{LR"(public: __cdecl winrt::SystemTray::implementation::IconView::IconView(void))"},
         &g_IconView_IconView_Original, IconView_IconView_Hook},
    };

    if (!WindhawkUtils::HookSymbols(module, systemTrayDllHooks,
                                    ARRAYSIZE(systemTrayDllHooks))) {
        if (!g_symbolFailureLogged.exchange(true)) {
            Wh_Log(L"[xaml] could not resolve the SystemTray.dll IconView "
                   L"constructor; no tray elements will be seen");
        }
        return false;
    }

    g_systemTraySymbolsHooked.store(true);
    Wh_Log(L"[xaml] SystemTray.dll IconView constructor hooked");
    return true;
}

// Called from the tray thread's timer until both modules are present.
void EnsureTaskbarXamlHooked() {
    if (g_unloading.load()) {
        return;
    }
    bool embed;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        embed = g_settings.embedInTaskbar;
    }
    if (!embed) {
        return;
    }
    if (g_taskbarSymbolsHooked.load() && g_systemTraySymbolsHooked.load()) {
        return;
    }

    const bool taskbar = HookTaskbarSymbols();
    const bool systemTray = HookSystemTraySymbols();
    // Resolving symbols takes seconds, and the mod may have begun to unload
    // meanwhile; its hooks are not to be applied after that (DECISIONS 67).
    if ((taskbar || systemTray) && !g_unloading.load()) {
        // Hooks registered after Wh_ModInit have to be applied explicitly.
        Wh_ApplyHookOperations();
    }
}

void RemoveEverything() {
    HideOverflowFlyout();
    g_overflowFlyout = nullptr;
    for (auto& tray : g_embeddedTrays) {
        RemovePanel(*tray);
    }
    g_embeddedTrays.clear();
    g_loggedTargetStack = false;
    g_loadedRevokers.clear();
}

}  // namespace SplitTrayXaml


#endif  // SPLITTRAY_NO_XAML

// ============================================================================
// Section 9 - Lifecycle
// ============================================================================

using namespace SplitTray;

BOOL Wh_ModInit() {
    Wh_Log(L"Split Tray initialising");

    // Cleared explicitly rather than relying on the initial value: Windhawk can
    // load a mod again in the same process after an unload, and a stale flag would
    // leave every hook path silently disabled.
    g_unloading.store(false);
    g_trayThreadStuck = false;
    g_handedBack.store(false);
    g_handBackStuck = false;
    g_panelsStuck = false;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_settings = LoadSettings();
        RecomputeGeometryLocked();
    }

    const auto monitors = EnumerateMonitors();
    for (size_t i = 0; i < monitors.size(); i++) {
        Wh_Log(L"monitor %zu: work area (%d,%d)-(%d,%d) dpi=%u%s", i + 1,
               monitors[i].workArea.left, monitors[i].workArea.top,
               monitors[i].workArea.right, monitors[i].workArea.bottom,
               monitors[i].dpi, monitors[i].primary ? L" [primary]" : L"");
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& tray : g_trays) {
            Wh_Log(L"tray %d: %s%s", tray.number,
                   TrayLabelLocked(tray.number).c_str(),
                   tray.available ? L"" : L" - icons meant for it wait in the primary "
                                          L"tray");
        }
        if (g_trays.empty()) {
            Wh_Log(L"one display and no extra trays: every icon stays in the "
                   L"primary tray");
        }
    }

    // A taskbar that exists already was announced before the mod arrived, so
    // the mod has to ask for its icons itself; one created from here on will be
    // announced by Explorer (ShouldAskAppsToReRegister).
    g_shellExistedAtLoad.store(FindShellTrayWindow() != nullptr);
    g_attachedBefore.store(false);
    g_missedShellAnnouncement.store(false);
    g_reRegisterSettledFor.store(nullptr);

    // The tray thread first, and the subclass only once it runs. Without it
    // nothing draws the mod's trays, and an icon sent to one would be lost; and
    // Windhawk unloads a mod whose Wh_ModInit fails without calling
    // Wh_ModUninit, so nothing may be left attached to Explorer by then.
    g_trayThreadState.store(TrayThreadState::Starting);
    g_trayThread = CreateThread(nullptr, 0, TrayThreadProc, nullptr, 0, &g_trayThreadId);
    if (!g_trayThread) {
        Wh_Log(L"failed to start the tray thread: %u", GetLastError());
        return FALSE;
    }
    // A few milliseconds. One that takes longer is left to carry on.
    const ULONGLONG deadline = GetTickCount64() + g_trayThreadStartWaitMs;
    while (g_trayThreadState.load() == TrayThreadState::Starting &&
           GetTickCount64() < deadline &&
           WaitForSingleObject(g_trayThread, 10) == WAIT_TIMEOUT) {
    }
    if (g_trayThreadState.load() == TrayThreadState::GaveUp ||
        WaitForSingleObject(g_trayThread, 0) == WAIT_OBJECT_0) {
        Wh_Log(L"the tray thread could not start; Split Tray is not loaded");
        WaitForSingleObject(g_trayThread, INFINITE);
        CloseHandle(g_trayThread);
        g_trayThread = nullptr;
        return FALSE;
    }

    if (g_trayThreadState.load() != TrayThreadState::Running) {
        // Slower than the wait. It attaches from its own timer once it runs,
        // and if it gives up instead nothing was attached (DECISIONS 69).
        Wh_Log(L"the tray thread is still starting; Split Tray attaches once it runs");
    } else if (!SubclassShellTrayWindow()) {
        // Not fatal: the taskbar may still be starting up. The tray thread's timer
        // retries until it appears.
        Wh_Log(L"Shell_TrayWnd not found yet, will retry");
    }

    return TRUE;
}

void Wh_ModAfterInit() {
    // If the taskbar already exists - which it does when the mod is loaded into a
    // running Explorer - this attaches and collects the existing icons right away.
    // On a cold Explorer start there is nothing to attach to yet, and the tray
    // thread's timer keeps trying.
    EnsureShellTrayWindowSubclassed();

    bool embed;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        embed = g_settings.embedInTaskbar;
    }
    if (embed) {
#ifndef SPLITTRAY_NO_XAML
        // Neither SystemTray.dll nor taskbar.dll is necessarily loaded yet; the
        // tray thread's timer keeps trying.
        SplitTrayXaml::EnsureTaskbarXamlHooked();
#endif
    }
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"settings changed");
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_settings = LoadSettings();
    }
    // The heavy lifting happens on the tray thread, which is the only thread
    // allowed to send replays to Explorer's taskbar thread.
    HWND hWnd = g_trayWnd.load();
    if (hWnd) {
        PostMessageW(hWnd, WM_ST_SETTINGS, 0, 0);
    }
}

// Sends `message` to the taskbar's thread and waits for it until `deadline`.
// Returns whether it was answered in time (DECISIONS 73).
//
// One that was not is posted as well, to be handled once the thread gets to it:
// a sent message that times out before it is handled is dropped, as the tests
// found - the hand-back never came, and the subclass went on swallowing icons
// into trays nothing drew. It is handled by the mod's code, so the module then
// has to stay loaded. The messages sent this way carry nothing, and one handled
// twice - begun as it was sent, and again as posted - does nothing the second
// time.
bool SendToTaskbarBy(HWND taskbar, UINT message, ULONGLONG deadline) {
    const ULONGLONG now = GetTickCount64();
    const DWORD wait = deadline > now ? static_cast<DWORD>(deadline - now) : 0;
    DWORD_PTR result = 0;
    if (SendMessageTimeoutW(taskbar, message, 0, 0, SMTO_NORMAL, wait, &result) != 0 ||
        !IsWindow(taskbar)) {
        return true;
    }
    PostMessageW(taskbar, message, 0, 0);
    return false;
}

// Takes the mod's panels out of the taskbars, on the taskbar's own thread:
// XAML objects belong to it (DECISIONS 37). Returns false if that thread did
// not answer in time (DECISIONS 73).
bool RemoveEmbeddedTrays() {
#ifndef SPLITTRAY_NO_XAML
    if (HWND taskbar = g_shellTrayWnd.load(); taskbar && IsWindow(taskbar)) {
        return SendToTaskbarBy(taskbar, GetXamlRemoveMessage(),
                               GetTickCount64() + g_taskbarWaitMs);
    }
#endif
    return true;
}

// Stops the tray thread and waits for it to end (DECISIONS 59).
//
// The shutdown goes to the thread's window, which does not exist yet when the
// mod is unloaded as soon as it has loaded. It was posted only if the window
// was already there, so an early unload lost it and the thread ran on. It is
// now posted as soon as there is a window to post it to, until the thread has
// ended or the time is up.
bool StopTrayThread() {
    if (!g_trayThread) {
        return true;
    }
    constexpr DWORD kBudgetMs = 5000;
    const ULONGLONG deadline = GetTickCount64() + kBudgetMs;
    bool posted = false;
    for (;;) {
        if (!posted) {
            if (HWND trayWnd = g_trayWnd.load()) {
                posted = PostMessageW(trayWnd, WM_ST_SHUTDOWN, 0, 0) != FALSE;
            }
        }
        if (WaitForSingleObject(g_trayThread, 20) == WAIT_OBJECT_0) {
            break;
        }
        if (GetTickCount64() >= deadline) {
            return false;
        }
    }
    CloseHandle(g_trayThread);
    g_trayThread = nullptr;
    return true;
}

// Hands every icon back to Explorer (DECISIONS 68), and waits until the mod's
// code has left the taskbar's thread (DECISIONS 73). The hand-back runs on that
// thread (HandIconsBackToShell), which then takes the subclass off itself. It
// can arrive inside a round of settling there - Explorer may run a message
// loop while it handles a record - and is then done once that round is over;
// and a call of the subclass may still be under way below it, an application's
// message Explorer was handling when it ran the loop. Returns false if the
// thread was not done in time, not answering or still in the mod's code.
//
// Nothing here waits longer than the budget. The hand-back was asked for with
// SendMessageW, before the wait began, and taking the subclass off from here
// is a message that thread has to answer too, so a taskbar thread that did not
// answer held unloading for as long as it did not. One that was not done in
// time does all of it once it answers, with the module kept loaded for it.
bool HandBackToShell() {
    HWND shellTrayWnd = g_shellTrayWnd.load();
    bool done = true;
    if (shellTrayWnd && IsWindow(shellTrayWnd)) {
        const ULONGLONG deadline = GetTickCount64() + g_taskbarWaitMs;
        SendToTaskbarBy(shellTrayWnd, GetReplayMessage(), deadline);
        auto left = [shellTrayWnd] {
            return g_subclassDepth.load() == 0 &&
                   (g_handedBack.load() || !IsWindow(shellTrayWnd));
        };
        while (!left() && GetTickCount64() < deadline) {
            Sleep(10);
        }
        // With no call of the subclass under way and none to come, what is
        // left of the last is its return; a message answered after it finds
        // the thread out of the mod's code.
        done = left() && SendToTaskbarBy(shellTrayWnd, WM_NULL, deadline);
    }
    g_shellTrayWnd.store(nullptr);
    return done;
}

// Everything unloading does before the store can go: the mod's panels out of
// the taskbars, its thread stopped, and every icon handed back to Explorer.
// The subclass keeps track of icons until that last step (DECISIONS 68).
void PrepareToUnload() {
    g_unloading.store(true);
    g_panelsStuck = !RemoveEmbeddedTrays();
    g_trayThreadStuck = !StopTrayThread();
    g_handBackStuck = !HandBackToShell();
}

// Before Windhawk takes the mod's hooks out (DECISIONS 67). The tray thread
// installs hooks of its own as the taskbar's modules appear
// (EnsureTaskbarXamlHooked), so it is stopped here, while they are all still
// in place, rather than left to install one after they have gone.
void Wh_ModBeforeUninit() {
    PrepareToUnload();
}

void Wh_ModUninit() {
    Wh_Log(L"Split Tray unloading");

    // Done already, by a Windhawk that calls Wh_ModBeforeUninit.
    if (!g_unloading.load()) {
        PrepareToUnload();
    }

    if (g_trayThreadStuck || g_handBackStuck || g_panelsStuck) {
        // The mod's code is still running, or will: the tray thread, or on the
        // taskbar's thread a round of settling with the hand-back after it, or
        // a message sent there that it has not answered yet. Keeping the
        // module loaded until Explorer exits is a leak; unloading it under
        // running code would take Explorer down with it. The store is left as
        // it is, for that code.
        const wchar_t* what =
            g_trayThreadStuck ? L"the tray thread did not stop"
            : g_panelsStuck   ? L"the taskbar did not take the mod's trays out in time"
                              : L"the icons were not all back with Explorer in time";
        HMODULE self = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(&StopTrayThread), &self)) {
            Wh_Log(L"%s; the mod stays loaded until Explorer exits rather than "
                   L"unload code it is still running",
                   what);
        } else {
            const DWORD error = GetLastError();
            Wh_Log(L"%s, and the mod could not keep itself loaded: %lu", what, error);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        // Both lists own an icon copy now, so both have to be released.
        for (auto* list : {&g_icons, &g_primaryOnly}) {
            for (auto& icon : *list) {
                if (icon.icon) {
                    DestroyIcon(icon.icon);
                    icon.icon = nullptr;
                }
            }
        }
        g_icons.clear();
        g_primaryOnly.clear();
    }

    Wh_Log(L"Split Tray unloaded");
}
