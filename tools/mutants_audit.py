#!/usr/bin/env python3
"""The mutants for the defects a code audit of 2026-09-24 found, DECISIONS 73
to 77, each a defect the mod actually had. Kept apart from tools/mutants.py,
which adds them to its catalogue, and from tools/mutants_reviews.py, so that no
module passes 400 lines.
"""


def audit_mutants(UNIT_SUITE, INTEGRATION_SUITE):
    """(description, text to find, replacement, suites that should kill it)"""
    return [
        # --- unloading, DECISIONS 73 ---------------------------------------------
        (
            "unloading waits on the taskbar's thread with no time limit",
            "    if (SendMessageTimeoutW(taskbar, message, 0, 0, SMTO_NORMAL, wait, &result) != 0 ||",
            "    if ((SendMessageW(taskbar, message, 0, 0), true) ||",
            (UNIT_SUITE,),
        ),
        (
            "a message the taskbar did not answer in time is dropped",
            "    PostMessageW(taskbar, message, 0, 0);\n    return false;",
            "    return false;",
            (UNIT_SUITE,),
        ),
        (
            "the subclass stays on the window once the icons are back",
            "        if (g_handedBack.load()) {\n"
            "            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd, ShellTrayWndSubclassProc);\n"
            "        }",
            "",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "unloading takes the subclass off from its own thread",
            "    g_shellTrayWnd.store(nullptr);\n    return done;",
            "    WindhawkUtils::RemoveWindowSubclassFromAnyThread(shellTrayWnd,\n"
            "                                                     ShellTrayWndSubclassProc);\n"
            "    g_shellTrayWnd.store(nullptr);\n    return done;",
            (UNIT_SUITE,),
        ),
        (
            "unloading ends with a call of the subclass still under way",
            "            return g_subclassDepth.load() == 0 &&\n                   (",
            "            return (",
            (UNIT_SUITE,),
        ),
        (
            "the subclass does not count its calls",
            "    const SubclassCall call;\n",
            "",
            (UNIT_SUITE,),
        ),
        # --- the version of an icon added afresh, DECISIONS 74 -------------------
        (
            "an add the mod answers keeps the version the icon had",
            "    if (existing && n.message == NIM_ADD && !forwardToShell) {\n"
            "        existing->version = 0;\n    }",
            "",
            (UNIT_SUITE,),
        ),
        (
            "an add passed on to Explorer starts at version 0 whatever Explorer answers",
            "    if (existing && n.message == NIM_ADD && !forwardToShell) {",
            "    if (existing && n.message == NIM_ADD) {",
            (UNIT_SUITE,),
        ),
        (
            "an add Explorer takes keeps the version the icon had",
            "            if (n.message == NIM_ADD) {\n                icon.version = 0;\n            }",
            "",
            (UNIT_SUITE,),
        ),
        # --- the arrange window, DECISIONS 75 ------------------------------------
        (
            "the arrange window is filled again only when asked",
            "               (repopulate || ArrangeLayoutNow() != g_arrangeLayout)) {",
            "               (repopulate)) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "the arrange window is filled again in the middle of a drag",
            "    } else if (!g_arrangeDragging &&\n               (repopulate",
            "    } else if ((repopulate",
            (INTEGRATION_SUITE,),
        ),
        (
            "the end of a drag leaves the arrange window behind",
            "    // Posted: this can run while the window is being destroyed.\n"
            "    NotifyTrayWindow(WM_ST_REFRESH);",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "an icon of the main tray coming or going does not reach the arrange window",
            "    } else if (n.message == NIM_ADD || n.message == NIM_DELETE) {",
            "    } else if (false) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "filling the arrange window again drops the selection",
            "        if (inserted >= 0 && !wasSelected.empty() && wasSelected == row.key) {",
            "        if (false) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "the arrange window does not see the main tray's icons come and go",
            "        layout += std::to_wstring(icon.serial) + L\":1;\";",
            "",
            (UNIT_SUITE,),
        ),
        (
            "the arrange window does not see an icon go into its tray's overflow",
            "                  (IsIconHidden(StableKeyOf(icon)) ? L\"h;\" : L\";\");",
            "                  L\";\";",
            (UNIT_SUITE,),
        ),
        # --- the embedded tray, DECISIONS 76 and 77 -------------------------------
        (
            "a cell keeps showing a picture its application took away",
            "    if (!cell.hasPicture) {\n        return CellPicture::Clear;\n    }",
            "",
            (UNIT_SUITE,),
        ),
        (
            "a picture that could not be copied for a cell clears it",
            "    return cell.icon.get() ? CellPicture::Replace : CellPicture::Keep;",
            "    return cell.icon.get() ? CellPicture::Replace : CellPicture::Clear;",
            (UNIT_SUITE,),
        ),
        (
            "an embedded tray shows tooltips whatever the setting says",
            "                         g_settings.showTooltips ? icon.tip : std::wstring(),",
            "                         icon.tip,",
            (UNIT_SUITE,),
        ),
        (
            "a floating tray keeps its tooltip when tooltips are switched off",
            "        } else if (!tooltips && tray.tooltip) {",
            "        } else if (false) {",
            (INTEGRATION_SUITE,),
        ),
    ]
