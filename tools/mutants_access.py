#!/usr/bin/env python3
"""The mutants for the improvement pass of 2026-09-26: the gaps the coverage
measurement found, screen readers and the keyboard in the floating trays
(DECISIONS 85), and tray numbers that stay put (DECISIONS 86). Kept apart from
tools/mutants.py, which adds them to its catalogue, so that no module passes
400 lines.
"""


def access_mutants(UNIT_SUITE, INTEGRATION_SUITE):
    """(description, text to find, replacement, suites that should kill it)"""
    return [
        # --- the gaps coverage found ------------------------------------------------
        (
            "a floating tray's menu does nothing that is chosen",
            "    DestroyMenu(menu.menu);  // and the submenu with it\n"
            "    RunFloatingTrayMenuCommand(menu, number, command);",
            "    DestroyMenu(menu.menu);  // and the submenu with it",
            (INTEGRATION_SUITE,),
        ),
        (
            "a floating tray's menu offers the tray it is on",
            "            if (target == number) {\n                continue;\n            }\n"
            "            const std::wstring text = L\"Move to \"",
            "            const std::wstring text = L\"Move to \"",
            (UNIT_SUITE,),
        ),
        (
            "\"Move an icon to this tray\" moves the wrong one",
            "            MoveIconToTray(menu.others[index].key, Destination::Tray(number));",
            "            MoveIconToTray(menu.others[0].key, Destination::Tray(1));",
            (UNIT_SUITE,),
        ),
        (
            "H in the arrange window does not hide an icon",
            "    SetIconHidden(key, !IsIconHidden(key));",
            "    SetIconHidden(key, IsIconHidden(key));",
            (INTEGRATION_SUITE,),
        ),
        (
            "a tray's number in the arrange window moves nothing",
            "                    if (target >= 0) {\n"
            "                        ArrangeMoveSelected(list, target);",
            "                    if (false) {\n"
            "                        ArrangeMoveSelected(list, target);",
            (INTEGRATION_SUITE,),
        ),
        (
            "the keypad's numbers do nothing in the arrange window",
            "                    tray = key - VK_NUMPAD0;",
            "                    tray = -1;",
            (INTEGRATION_SUITE,),
        ),
        # --- screen readers and the keyboard, DECISIONS 85 ----------------------------
        (
            "a floating tray does not answer screen readers",
            "            if (static_cast<LONG>(lParam) == UiaRootObjectId ||",
            "            if (false &&",
            (INTEGRATION_SUITE,),
        ),
        (
            "a floating tray's buttons have no names",
            "            *out = StringVariant(FloatingCellName(serial_));",
            "            break;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "a floating tray's buttons are in the wrong order",
            "        case NavigateDirection_NextSibling:\n            sibling = index + 1;",
            "        case NavigateDirection_NextSibling:\n            sibling = index - 1;",
            (UNIT_SUITE,),
        ),
        (
            "a button whose icon has gone still reads as there",
            "    *index = CellIndexOfSerial(*view, serial);\n    return *index >= 0;",
            "    *index = CellIndexOfSerial(*view, serial);\n    return true;",
            (UNIT_SUITE,),
        ),
        (
            "pressing a floating tray's button does nothing",
            "        PostMessageW(wnd, WM_ST_CELL_ACTIVATE, static_cast<WPARAM>(serial_), 0);",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "Enter and Space on a floating tray do nothing",
            "            if (wParam == VK_RETURN || wParam == VK_SPACE) {\n"
            "                ActivateFloatingCell(hWnd, tray->focusSerial, /*contextMenu=*/false);",
            "            if (false) {\n"
            "                ActivateFloatingCell(hWnd, tray->focusSerial, /*contextMenu=*/false);",
            (INTEGRATION_SUITE,),
        ),
        (
            "the menu key on a floating tray is taken for a selection",
            "                    ActivateFloatingCell(hWnd, tray->focusSerial, /*contextMenu=*/true);",
            "                    ActivateFloatingCell(hWnd, tray->focusSerial, /*contextMenu=*/false);",
            (INTEGRATION_SUITE,),
        ),
        (
            "Down on a floating tray moves one cell, not a row",
            "            return current + columns < count ? current + columns : current;",
            "            return current + 1 < count ? current + 1 : current;",
            (UNIT_SUITE,),
        ),
        (
            "a screen reader cannot move to a floating tray's icon",
            "        PostMessageW(wnd, WM_ST_CELL_FOCUS, static_cast<WPARAM>(serial_), 0);",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "NIM_SETFOCUS does not reach a floating tray",
            "            PostMessageW(trayWnd, WM_ST_FOCUS_ICON, static_cast<WPARAM>(serial), 0);",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "the keyboard stays on an icon that has gone",
            "    if (CellIndexOfSerial(view, tray.focusSerial) < 0) {\n"
            "        tray.focusSerial = SerialOfCellIndex(view, 0);",
            "    if (false) {\n"
            "        tray.focusSerial = SerialOfCellIndex(view, 0);",
            (INTEGRATION_SUITE,),
        ),
        (
            "a floating tray's provider outlives its window",
            "                if (tray->uia) {\n"
            "                    UiaReturnRawElementProvider(hWnd, 0, 0, nullptr);",
            "                if (false) {\n"
            "                    UiaReturnRawElementProvider(hWnd, 0, 0, nullptr);",
            (INTEGRATION_SUITE,),
        ),
        # Escape going back to the window that had the foreground has no mutant:
        # the integration test's desktop takes no input and has no foreground
        # window, so no suite can see it. It is checked in Explorer.
        # --- tray numbers, DECISIONS 86 ---------------------------------------------
        (
            "a display's tray is renumbered while the display is away",
            "        trays.push_back(tray);\n    }\n    for (const auto& extra : settings.extraTrays) {",
            "        if (tray.available) {\n            trays.push_back(tray);\n        }\n"
            "    }\n    for (const auto& extra : settings.extraTrays) {",
            (UNIT_SUITE,),
        ),
        (
            "a display seen for the first time always gets a new number",
            "        if (free != held.end()) {",
            "        if (false) {",
            (UNIT_SUITE,),
        ),
        (
            "the primary display keeps a tray number",
            "    for (const auto& monitor : monitors) {\n        if (monitor.primary) {\n"
            "            continue;\n        }\n        bool found = false;",
            "    for (const auto& monitor : monitors) {\n        bool found = false;",
            (UNIT_SUITE,),
        ),
        (
            "tray numbers are not remembered across restarts",
            "        SaveDisplaySlotsLocked();\n",
            "",
            (UNIT_SUITE,),
        ),
    ]
