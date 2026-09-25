#!/usr/bin/env python3
"""The mutants tools/mutate.py injects: plausible defects in the mod, several of
them defects it actually shipped, each with the suites that should notice it.

Kept apart from the runner so the catalogue can grow without the runner
growing with it.
"""

from mutants_audit import audit_mutants
from mutants_reviews import review_mutants

UNIT_SUITE = "unit"
INTEGRATION_SUITE = "integration"

# (description, exact text to find, replacement, suites that should kill it)
MUTANTS = [
    (
        "icons in Split Tray's trays are no longer suppressed from the primary tray",
        "    return {destination.alsoPrimary, true, destination.tray};",
        "    return {true, true, destination.tray};",
        (UNIT_SUITE,),
    ),
    (
        "the missing-tray fallback is removed",
        "    if (destination.tray <= 1 || !trayAvailable) {",
        "    if (destination.tray <= 1) {",
        (UNIT_SUITE,),
    ),
    (
        "the primary display gets a tray of its own",
        "        if (monitor.primary) {\n            continue;\n        }\n        TrayTarget tray;",
        "        TrayTarget tray;",
        (UNIT_SUITE,),
    ),
    (
        "a display's tray is marked as not connected",
        "        tray.forDisplay = true;\n        tray.available = true;",
        "        tray.forDisplay = true;\n        tray.available = false;",
        (UNIT_SUITE,),
    ),
    (
        "an extra tray on a missing display gives up its number",
        "            ResolveDisplay(monitors, extra.display, &tray.monitor) && !extra.disabled;\n"
        "        trays.push_back(tray);",
        "            ResolveDisplay(monitors, extra.display, &tray.monitor) && !extra.disabled;\n"
        "        if (tray.available) trays.push_back(tray);",
        (UNIT_SUITE,),
    ),
    (
        "a disabled extra tray is shown anyway",
        "            ResolveDisplay(monitors, extra.display, &tray.monitor) && !extra.disabled;",
        "            ResolveDisplay(monitors, extra.display, &tray.monitor);",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "an extra tray saved without the switch reads as disabled",
        '        extra.disabled = Wh_GetIntSetting(L"extraTrays[%d].disabled", i) != 0;',
        '        extra.disabled = Wh_GetIntSetting(L"extraTrays[%d].disabled", i) == 0;',
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "a disabled tray is described as if it were there",
        "    if (tray.disabled) {\n        return L\"disabled\";\n    }\n",
        "",
        (UNIT_SUITE,),
    ),
    (
        "the last display cannot be named by its number",
        "    if (display < 0 || static_cast<size_t>(display) > monitors.size()) {",
        "    if (display < 0 || static_cast<size_t>(display) >= monitors.size()) {",
        (UNIT_SUITE,),
    ),
    (
        "tray 2's placements are written in a form the two-tray versions cannot read",
        "    if (destination.tray == 2) {\n        return L\"s\";\n    }",
        "",
        (UNIT_SUITE,),
    ),
    (
        "an icon is shown in tray 2 whichever tray it was sent to",
        "    icon.shownTray = mirror ? plan.tray : 0;",
        "    icon.shownTray = mirror ? 2 : 0;",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "re-sorting shows an icon its application hid",
        "    const bool mirror = plan.mirror && !(appHidden && !g_settings.mirrorHiddenIcons);",
        "    const bool mirror = plan.mirror;",
        (UNIT_SUITE,),
    ),
    (
        "a floating tray ignores its own corner",
        "                          tray.monitor.dpi, tray.corner);",
        "                          tray.monitor.dpi, g_settings.corner);",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "an empty floating tray has nothing to click",
        "            ComputeLayout(tray.monitor.workArea, std::max(count, 1), g_settings,",
        "            ComputeLayout(tray.monitor.workArea, count, g_settings,",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "a serial finds whichever icon comes first",
        "        if (g_icons[i].serial == serial) {",
        "        if (g_icons[i].serial != 0) {",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "the bottom-right anchor drifts off the monitor",
        "            layout.x = workArea.right - offsetX - layout.width;\n"
        "            layout.y = workArea.bottom - offsetY - layout.height;\n"
        "            break;\n        case Corner::BottomLeft:",
        "            layout.x = workArea.right + offsetX + layout.width;\n"
        "            layout.y = workArea.bottom - offsetY - layout.height;\n"
        "            break;\n        case Corner::BottomLeft:",
        (UNIT_SUITE,),
    ),
    (
        "executable-name matching becomes case sensitive",
        "        if (towlower(a[i]) != towlower(b[i])) {",
        "        if (a[i] != b[i]) {",
        (UNIT_SUITE,),
    ),
    (
        "szTip is read from the wrong wire offset",
        "constexpr size_t kTip = 0x020;          // WCHAR[128]",
        "constexpr size_t kTip = 0x024;          // WCHAR[128]",
        (UNIT_SUITE,),
    ),
    (
        "a partial NIM_MODIFY clobbers the tooltip",
        "    if (n.flags & NIF_TIP) {\n        existing->tip = n.tip;\n    }",
        "    {\n        existing->tip = n.tip;\n    }",
        (UNIT_SUITE,),
    ),
    (
        "routing stops being sticky and is re-decided per message",
        "    const bool wasSticky = (existing != nullptr);\n    if (existing) {",
        "    const bool wasSticky = (existing != nullptr);\n    if (false && existing) {",
        (UNIT_SUITE,),
    ),
    (
        "GUID identity is ignored in favour of hWnd+uID",
        "    if (UsableGuid(icon.hasGuid, icon.guid) && UsableGuid(n.hasGuid, n.guid)) {\n"
        "        return memcmp(&icon.guid, &n.guid, sizeof(GUID)) == 0;\n    }",
        "    if (false) {\n"
        "        return memcmp(&icon.guid, &n.guid, sizeof(GUID)) == 0;\n    }",
        (UNIT_SUITE,),
    ),
    (
        "the wire signature check is dropped",
        "    if (ReadDword(data, wire::kSignature) != kTrayDataSignature) {\n"
        "        return false;\n    }",
        "    if (false) {\n        return false;\n    }",
        (UNIT_SUITE,),
    ),
    (
        "hit testing is off by one row",
        "    const int index = row * layout.columns + column;",
        "    const int index = (row + 1) * layout.columns + column;",
        (UNIT_SUITE,),
    ),
    # The two defects the live run in Explorer found. Only the integration suite
    # exercises the message handler and the retry timer, so only it can kill these.
    (
        "the tray window is never looked for again after startup "
        "(inert on every cold Explorer boot)",
        "            EnsureShellTrayWindowSubclassed();\n#ifndef SPLITTRAY_NO_XAML",
        "\n#ifndef SPLITTRAY_NO_XAML",
        (INTEGRATION_SUITE,),
    ),
    (
        "a settings change no longer moves icons that are already on screen",
        "        case WM_ST_SETTINGS:\n"
        "            // New rules can change where an icon that is already on screen\n"
        "            // belongs, so re-resolve every tracked icon and replay the\n"
        "            // difference into the shell before re-laying out.\n"
        "            ApplySettingsToTrackedIcons();\n",
        "        case WM_ST_SETTINGS:\n",
        (INTEGRATION_SUITE,),
    ),
    (
        "g_unloading is not cleared on load (mod inert after a reload)",
        "    g_unloading.store(false);\n",
        "",
        (INTEGRATION_SUITE,),
    ),
    # An icon moved to the secondary tray and back came back as its last
    # partial modify: no callback, no tooltip, no path, a destroyed picture.
    (
        "a primary-tray icon stores its last message instead of the folded record",
        "        FoldTrayRecord(&existing->payload, payload);",
        "        existing->payload = payload;",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "a secondary-tray icon stores its last message instead of the folded record",
        "    FoldTrayRecord(&existing->payload, payload);\n    existing->revision++;\n    return true;",
        "    existing->payload = payload;\n    existing->revision++;\n    return true;",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "a replayed add draws with the application's picture, long since destroyed",
        "    if (record.size() >= wire::kFlags + sizeof(DWORD) &&\n"
        "        record.size() >= wire::kIcon + sizeof(DWORD)) {\n        const DWORD flags",
        "    if (false && record.size() >= wire::kFlags + sizeof(DWORD) &&\n"
        "        record.size() >= wire::kIcon + sizeof(DWORD)) {\n        const DWORD flags",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "a modify's empty path overwrites the owner's path",
        "        (in[wire::kExePath] || in[wire::kExePath + 1])) {",
        "        true) {",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "a balloon is kept in the record and shown again on every replay",
        "        *state = incoming;\n"
        "        WriteDword(state->data(), wire::kFlags, flags & kLastingFlags);",
        "        *state = incoming;",
        (UNIT_SUITE,),
    ),
    (
        "a re-added icon loses the version its application negotiated",
        "    if (icon.version) {",
        "    if (false) {",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    # Desk Tray's WhatsApp icon: registered into a tray that was not ready,
    # because the mod asked as well as Explorer.
    (
        "the mod asks applications to re-register on every attach again",
        "    return (firstAttach && shellExistedAtLoad) || missedShellAnnouncement;",
        "    return true;",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "an attachment made by Wh_ModInit is never settled (running Explorer: no icons)",
        "    if (!watched) {\n"
        "        if (!SubclassShellTrayWindow()) {\n"
        "            return;\n"
        "        }\n"
        "        watched = g_shellTrayWnd.load();\n"
        "    }",
        "    if (watched) {\n"
        "        return;\n"
        "    }\n"
        "    if (!SubclassShellTrayWindow()) {\n"
        "        return;\n"
        "    }\n"
        "    watched = g_shellTrayWnd.load();",
        (INTEGRATION_SUITE,),
    ),
    (
        "Explorer's announcement heard while not watching is ignored",
        "        g_missedShellAnnouncement.store(true);",
        "",
        (INTEGRATION_SUITE,),
    ),
    (
        "NIM_SETVERSION is passed to a shell that was never given the icon",
        "            *outForwardToShell = existing->forwardedToShell;\n        } else {",
        "            *outForwardToShell = true;\n        } else {",
        (UNIT_SUITE, INTEGRATION_SUITE),
    ),
    (
        "nobody says where a secondary-only icon is, so Tauri drops its clicks",
        "        if (SecondaryIconScreenRect(rectQuery, &rect)) {",
        "        if (false && SecondaryIconScreenRect(rectQuery, &rect)) {",
        (INTEGRATION_SUITE,),
    ),
    (
        "the mod answers for icons Explorer has",
        "            return g_icons[i].forwardedToShell ? -1 : static_cast<int>(i);",
        "            return static_cast<int>(i);",
        (UNIT_SUITE,),
    ),
    (
        "the size and position answers are swapped",
        "    if (part == kIconRectSize) {\n        const int width",
        "    if (part == kIconRectPosition) {\n        const int width",
        (UNIT_SUITE,),
    ),
    (
        "an empty rect is reported as found",
        "        if (width <= 0 || height <= 0) {\n            return 0;",
        "        if (false) {\n            return 0;",
        (UNIT_SUITE,),
    ),
    (
        "the rect question's owner is read from the wrong offset",
        "constexpr size_t kOwnerWnd = 0x10;   // DWORD, HWND of the icon owner",
        "constexpr size_t kOwnerWnd = 0x0C;   // DWORD, HWND of the icon owner",
        (UNIT_SUITE,),
    ),
    (
        "the rect question's GUID is ignored",
        "    if (UsableGuid(icon.hasGuid, icon.guid) && !IsEmptyGuid(query.guid)) {",
        "    if (false) {",
        (UNIT_SUITE,),
    ),
    (
        "a floating-tray cell is placed by its row instead of its column",
        "    out->left = layout.x + column * layout.cell;",
        "    out->left = layout.x + row * layout.cell;",
        (UNIT_SUITE,),
    ),
    (
        "island bounds ignore the display scale",
        "    r.left = islandOrigin.x + static_cast<LONG>(std::lround(x * scale));",
        "    r.left = islandOrigin.x + static_cast<LONG>(std::lround(x));",
        (UNIT_SUITE,),
    ),
]

MUTANTS += review_mutants(UNIT_SUITE, INTEGRATION_SUITE)
MUTANTS += audit_mutants(UNIT_SUITE, INTEGRATION_SUITE)
