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
            "        if (delivery.change == ShellChange::Add && !taken) {",
            "        if (false) {",
            (UNIT_SUITE,),
        ),
        (
            "an add Explorer refuses is asked for again for ever",
            "    return icon.shellRefusals < kShellAttempts;",
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
            "    } else if (!icon->forwardedToShell) {\n        icon->forwardedToShell = true;",
            "    } else if (false) {\n        icon->forwardedToShell = true;",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "the subclass does not record what Explorer answered",
            "                ask = RecordShellAnswerLocked(n, answer != FALSE);",
            "                ask = false;",
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
            "        cells.push_back({OwnedIcon::CopyOf(icon.icon),",
            "        cells.push_back({OwnedIcon(icon.icon),",
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
            "void Wh_ModBeforeUninit() {\n    PrepareToUnload();\n}",
            "void Wh_ModBeforeUninit() {\n}",
            (INTEGRATION_SUITE,),
        ),
        # --- the third external review (2026-09-24), DECISIONS 68 to 72 ----------
        (
            "an unloading mod stops keeping track before its icons are back",
            "    if (msg != WM_COPYDATA || g_handedBack.load()) {",
            "    if (msg != WM_COPYDATA || g_unloading.load()) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "a mod loaded again still thinks its icons were handed back last time",
            "    g_handedBack.store(false);\n    g_handBackStuck = false;\n",
            "    g_handBackStuck = false;\n",
            (INTEGRATION_SUITE,),
        ),
        (
            "icons that arrive while the icons are handed back are left out",
            "        if (!SettleShellIcons(deliver)) {\n            break;\n        }",
            "        SettleShellIcons(deliver);\n        break;",
            (UNIT_SUITE,),
        ),
        (
            "the hand-back asks Explorer again for icons it refused their applications",
            "                    if (target != icon.shellTarget) {\n"
            "                        icon.shellTarget = target;\n"
            "                        icon.shellRefusals = 0;",
            "                    if (true) {\n"
            "                        icon.shellTarget = target;\n"
            "                        icon.shellRefusals = 0;",
            (UNIT_SUITE,),
        ),
        (
            "a hand-back that arrives during a round of settling is dropped",
            "        SettleShellIcons(deliver);\n    }\n    if (g_unloading.load()) {\n"
            "        HandIconsBackToShell(deliver);",
            "        SettleShellIcons(deliver);\n    } else {\n"
            "        HandIconsBackToShell(deliver);",
            (UNIT_SUITE,),
        ),
        (
            "a round of settling starts inside another",
            "    if (g_settlingShell) {\n        return false;\n    }\n    g_settlingShell = true;",
            "    g_settlingShell = true;",
            (UNIT_SUITE,),
        ),
        (
            "unloading ends with the hand-back still to come and the mod not kept loaded",
            "    if (g_trayThreadStuck || g_handBackStuck || g_panelsStuck) {",
            "    if (g_trayThreadStuck || g_panelsStuck) {",
            (INTEGRATION_SUITE,),
        ),
        (
            "the mod attaches while its tray thread is still starting",
            "    if (g_trayThreadState.load() != TrayThreadState::Running) {\n"
            "        return false;\n    }\n    HWND hWnd = FindShellTrayWindow();",
            "    HWND hWnd = FindShellTrayWindow();",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "Explorer's answer is recorded only for an icon it had refused to take back",
            "        *outRecordShellAnswer = forwardToShell;",
            "        *outRecordShellAnswer = existing && OwedToShell(*existing);",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "an application's message Explorer refused is recorded as nothing",
            "    if (!holds) {\n        icon->forwardedToShell = false;",
            "    if (false) {\n        icon->forwardedToShell = false;",
            (UNIT_SUITE,),
        ),
        (
            "an application's add Explorer refused is taken as absent, not asked about",
            "            if (!taken && n.message == NIM_ADD) {",
            "            if (false) {",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "the modify asking about a refused add shows its balloon again",
            "                   ReadDword(record.data(), wire::kFlags) & kLastingFlags &\n",
            "                   ReadDword(record.data(), wire::kFlags) &\n",
            (UNIT_SUITE,),
        ),
        (
            "the modify asking about a refused add carries the picture the record names",
            "                       ~static_cast<DWORD>(NIF_ICON));\n        WriteDword(record.data(), wire::kIcon, 0);",
            "                       ~static_cast<DWORD>(0));",
            (UNIT_SUITE,),
        ),
        (
            "a refused add is asked about in the middle of its application's message",
            "            if (ask) {\n                WakeReplayDelivery();\n            }",
            "            if (ask) {\n"
            "                SettleShellIcons([hWnd](HWND senderWnd, const std::vector<BYTE>& r) {\n"
            "                    return DeliverToShell(hWnd, senderWnd, r);\n"
            "                });\n"
            "            }",
            (INTEGRATION_SUITE,),
        ),
        (
            "a refused add is never asked about",
            "    if (icon.shellUnconfirmed) {\n        return true;\n    }",
            "",
            (UNIT_SUITE, INTEGRATION_SUITE),
        ),
        (
            "what arrives while Explorer takes an icon back is not followed up",
            "        icon->shellBehind = icon->revision != delivery.revision || !versionTaken;",
            "        icon->shellBehind = !versionTaken;",
            (UNIT_SUITE,),
        ),
        (
            "a version Explorer refuses is recorded as taken",
            "            versionTaken = deliver(delivery.senderWnd, delivery.versionRecord) != FALSE;",
            "            deliver(delivery.senderWnd, delivery.versionRecord);",
            (UNIT_SUITE,),
        ),
        (
            "a version Explorer refuses is asked for again for ever",
            "            return ShellOutcome::Settled;\n        }\n    } else {",
            "            return ShellOutcome::Settled;\n        }\n        return ShellOutcome::Refused;\n"
            "    } else {",
            (UNIT_SUITE,),
        ),
        (
            "an add with no picture of the mod's own goes with the application's old handle",
            "        WriteDword(record.data(), wire::kIcon,\n"
            "                   static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(icon)));\n"
            "        WriteDword(record.data(), wire::kFlags,\n"
            "                   icon ? flags | NIF_ICON : flags & ~static_cast<DWORD>(NIF_ICON));",
            "        if (icon) {\n"
            "            WriteDword(record.data(), wire::kIcon,\n"
            "                       static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(icon)));\n"
            "            WriteDword(record.data(), wire::kFlags, flags | NIF_ICON);\n"
            "        }",
            (UNIT_SUITE,),
        ),
        (
            "a picture that cannot be copied throws away the one before",
            "        if (!copy) {\n            return;\n        }",
            "",
            (UNIT_SUITE,),
        ),
    ]
