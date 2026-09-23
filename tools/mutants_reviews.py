#!/usr/bin/env python3
"""The mutants for defects an external review found, or a live reload did,
each a defect the mod actually had. Kept apart from tools/mutants.py, which
adds them to its catalogue.
"""


def review_mutants(UNIT_SUITE, INTEGRATION_SUITE):
    """(description, text to find, replacement, suites that should kill it)"""
    return [
        # --- the review of 2026-09-23 -------------------------------------------
        (
            "a record of another shape is read at the captured offsets",
            "                   wire::kExpectedNidCbSize);\n        }\n        return false;\n    }",
            "                   wire::kExpectedNidCbSize);\n        }\n    }",
            (UNIT_SUITE,),
        ),
        (
            "whether an icon is hidden is read from the message alone",
            "    DWORD state = existing ? existing->state : 0;",
            "    DWORD state = 0;",
            (UNIT_SUITE,),
        ),
        (
            "an icon re-registered by GUID keeps its old owner and uID",
            "        existing->ownerWnd = n.ownerWnd;\n        existing->uID = n.uID;",
            "",
            (UNIT_SUITE,),
        ),
        (
            "a floating tray draws the store's own handles",
            "            view.icons.push_back(OwnedIcon::CopyOf(g_icons[i].icon));",
            "            view.icons.push_back(OwnedIcon(g_icons[i].icon));",
            (UNIT_SUITE,),
        ),
        (
            "version 3 icons get neither NIN_SELECT nor WM_CONTEXTMENU",
            "    if (version >= NOTIFYICON_VERSION) {",
            "    if (version >= NOTIFYICON_VERSION_4) {",
            (UNIT_SUITE,),
        ),
        (
            "a left click is not followed by NIN_SELECT",
            "            callbacks.push_back(pack(NIN_SELECT));",
            "",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "an unknown TaskbarHost layout is taken at a guessed offset",
            "        *offset = code[7];\n        return true;\n    }\n    return false;",
            "        *offset = code[7];\n        return true;\n    }\n    *offset = 0x10;\n    return true;",
            (UNIT_SUITE,),
        ),
        (
            "a move is recorded as done when it is asked for, not when Explorer takes it",
            "        icon.shellTarget = plan.forwardToShell;\n        icon.shellRefusals = 0;",
            "        icon.shellTarget = plan.forwardToShell;\n"
            "        icon.forwardedToShell = plan.forwardToShell;\n"
            "        icon.shellRefusals = 0;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "each of the arrange window's lists destroys the shared image list",
            "LVS_SHOWSELALWAYS |\n            LVS_SHAREIMAGELISTS,",
            "LVS_SHOWSELALWAYS,",
            (INTEGRATION_SUITE,),
        ),
        (
            "the arrange window's class is left registered when the mod unloads",
            "    UnregisterModClass(kArrangeClassName);\n",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "unloading tries the shutdown once, before the tray thread's window may exist",
            "            if (HWND trayWnd = g_trayWnd.load()) {\n"
            "                posted = PostMessageW(trayWnd, WM_ST_SHUTDOWN, 0, 0) != FALSE;\n"
            "            }",
            "            posted = true;\n"
            "            if (HWND trayWnd = g_trayWnd.load()) {\n"
            "                PostMessageW(trayWnd, WM_ST_SHUTDOWN, 0, 0);\n"
            "            }",
            (INTEGRATION_SUITE,),
        ),
        # --- loading into a running Explorer (found live, 2026-09-23) -----------
        (
            "an icon new to the mod is left in Explorer's tray as well",
            "    if (retractFromShell) {\n        DeliverToShell(",
            "    if (retractFromShell && false) {\n        DeliverToShell(",
            (INTEGRATION_SUITE,),
        ),
        (
            "a first modify for an icon never seen added is answered with success",
            "        if (n.message == NIM_MODIFY) {\n            return FALSE;\n        }\n",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "a message without a path is never looked up",
            "    n->exePath = g_lookUpProcessPath(n->ownerWnd);",
            "",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "the tree walk tries the frame first",
            "        return 3;\n    }\n    return 2;",
            "        return -1;\n    }\n    return 2;",
            (UNIT_SUITE,),
        ),
        # --- the second external review (2026-09-23), DECISIONS 66 and 67 --------
        (
            "an add Explorer refused is recorded as taken",
            "    if (taken) {\n        icon->forwardedToShell = true;",
            "    if (true) {\n        icon->forwardedToShell = true;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "Explorer is not asked whether it has an icon it refused to add",
            "    if (delivery.add && !taken) {",
            "    if (false) {",
            (UNIT_SUITE,),
        ),
        (
            "an add Explorer refuses is asked for again for ever",
            "    return !icon.shellTarget || icon.shellRefusals < kShellAttempts;",
            "    return true;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "an icon Explorer would not take back is swallowed, not left to its application",
            "        forwardToShell = existing->forwardedToShell || owed;",
            "        forwardToShell = existing->forwardedToShell;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "Explorer taking an application's own add back is not recorded",
            "                icon.forwardedToShell = true;\n                icon.shellRefusals = 0;\n                return;",
            "                return;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "the subclass does not record what Explorer answered",
            "            RecordShellAnswerLocked(n, answer != FALSE);",
            "",
            (INTEGRATION_SUITE,),
        ),
        (
            "an icon removed while Explorer took it back is left there",
            "                // Its application removed it while Explorer was taking it.\n"
            "                deliver(delivery.senderWnd,\n"
            "                        PayloadWithMessage(delivery.record, NIM_DELETE));",
            "",
            (UNIT_SUITE,),
        ),
        (
            "an icon put back in Explorer's tray is not given its version",
            "                out->versionRecord = SetVersionRecordFor(icon.payload, icon.version);",
            "",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "an embedded tray draws the store's own handles",
            "            {OwnedIcon::CopyOf(icon.icon), icon.tip, StableKeyOf(icon), icon.serial});",
            "            {OwnedIcon(icon.icon), icon.tip, StableKeyOf(icon), icon.serial});",
            (UNIT_SUITE,),
        ),
        (
            "the mod attaches without waiting for its tray thread",
            "    while (g_trayThreadState.load() == TrayThreadState::Starting &&",
            "    while (false &&",
            (INTEGRATION_SUITE,),
        ),
        (
            "the tray thread is stopped only after Windhawk has taken the hooks out",
            "    RemoveEmbeddedTrays();\n    g_trayThreadStuck = !StopTrayThread();\n}\n\nvoid Wh_ModUninit() {",
            "    RemoveEmbeddedTrays();\n}\n\nvoid Wh_ModUninit() {",
            (INTEGRATION_SUITE,),
        ),
    ]
