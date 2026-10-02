#!/usr/bin/env python3
"""The mutants for Split Tray's trays working like Explorer's own, DECISIONS 78
to 83: balloons, hover and popups, the keyboard, screen readers, drawing only
the pictures that changed, and the arrange window's names. Kept apart from
tools/mutants.py, which adds them to its catalogue, so that no module passes
400 lines.
"""


def parity_mutants(UNIT_SUITE, INTEGRATION_SUITE):
    """(description, text to find, replacement, suites that should kill it)"""
    return [
        # --- balloons, DECISIONS 78 ------------------------------------------------
        (
            "a balloon for an icon in Split Tray's trays is swallowed",
            "    if ((n.message == NIM_ADD || n.message == NIM_MODIFY) && (n.flags & NIF_INFO)) {",
            "    if (false) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "the copy Explorer holds for balloons is not hidden",
            "                       (hidden ? NIS_HIDDEN : 0));",
            "                       0);",
            (UNIT_SUITE,),
        ),
        (
            "a balloon goes without its text",
            "    take(wire::kInfo, wire::kInfoChars * sizeof(wchar_t));\n",
            "",
            (UNIT_SUITE,),
        ),
        (
            "the hidden copy is not given its version before a balloon",
            "        if (!version.empty()) {\n            deliver(senderWnd, version);\n"
            "        }\n        return deliver(senderWnd, balloon) != FALSE;",
            "        return deliver(senderWnd, balloon) != FALSE;",
            (UNIT_SUITE,),
        ),
        (
            "a hidden copy lost with a restarted Explorer is not made again",
            "    if (!shown && hidden) {\n        shown = show(true);\n    }",
            "",
            (UNIT_SUITE,),
        ),
        (
            "an icon Explorer holds hidden is added again rather than shown",
            "            out->change = icon.forwardedToShell || icon.shellHidden ? ShellChange::Update",
            "            out->change = icon.forwardedToShell ? ShellChange::Update",
            (UNIT_SUITE,),
        ),
        (
            "an icon Explorer holds hidden stays hidden when it moves into Explorer's tray",
            "            if (icon.shellHidden) {\n"
            "                out->record = WithHiddenState(out->record, (icon.state & NIS_HIDDEN) != 0);\n"
            "            }",
            "",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "a hidden copy shown or refused is still thought hidden",
            "    icon->shellHidden = false;\n    if (taken) {",
            "    if (taken) {",
            (UNIT_SUITE,),
        ),
        (
            "an application's delete leaves its hidden copy in Explorer",
            "            *outRetractFromShell = existing->shellHidden;",
            "            *outRetractFromShell = false;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        # --- hover and popups, DECISIONS 79 ----------------------------------------
        (
            "a version 4 icon without NIF_SHOWTIP gets the tray's tooltip",
            "    return version < NOTIFYICON_VERSION_4 || (recordFlags & NIF_SHOWTIP) != 0;",
            "    return true;",
            (UNIT_SUITE,),
        ),
        (
            "the pointer moving over an icon is not passed on",
            "                ForwardHover(view.serials[static_cast<size_t>(index)],\n"
            "                             ScreenPointOf(hWnd, lParam));",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "an icon's own popup is never opened",
            "                if (ForwardPopup(serial, true, ScreenPointOf(hWnd, lParam))) {",
            "                if (false) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "an icon's own popup is never closed",
            "        ForwardPopup(tray->popupSerial, false, cursor);\n",
            "",
            (INTEGRATION_SUITE,),
        ),
        # --- the keyboard, DECISIONS 80 --------------------------------------------
        (
            "a version 3 icon selected from the keyboard gets clicks, not NIN_KEYSELECT",
            "    if (version >= NOTIFYICON_VERSION) {\n        return {PackTrayCallback",
            "    if (false) {\n        return {PackTrayCallback",
            (UNIT_SUITE,),
        ),
        (
            "NIM_SETFOCUS for an icon in Split Tray's trays is passed to Explorer",
            "        *outForwardToShell = !(existing && existingIsMirrored && !existing->forwardedToShell);",
            "        *outForwardToShell = true;",
            (UNIT_SUITE,),
        ),
        # --- screen readers, DECISIONS 81 ------------------------------------------
        (
            "a cell has no name for screen readers",
            "        cell.name = IconLabel(icon);\n",
            "",
            (UNIT_SUITE,),
        ),
        # --- drawing only what changed, DECISIONS 82 --------------------------------
        (
            "every refresh copies and draws every picture again",
            "            cell.pictureUnchanged =\n"
            "                shown != drawn->end() && shown->second == icon.pictureRevision;",
            "            cell.pictureUnchanged = false;",
            (UNIT_SUITE,),
        ),
        (
            "a changed picture is not noticed",
            "    icon->icon = copy;\n    icon->pictureRevision++;",
            "    icon->icon = copy;",
            (UNIT_SUITE,),
        ),
        # --- the arrange window's names, DECISIONS 83 --------------------------------
        (
            "the arrange window's rows keep the names they were filled with",
            "        } else {\n            UpdateArrangeLabels();\n        }",
            "        }",
            (INTEGRATION_SUITE,),
        ),
        (
            "an icon of the main tray renamed does not reach the arrange window",
            "               (g_arrangeOpen.load() && (n.flags & NIF_TIP))) {",
            "               false) {",
            (INTEGRATION_SUITE,),
        ),
    ]
