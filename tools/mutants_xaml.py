#!/usr/bin/env python3
"""The mutants for the mod's XAML section (section 10), which only the XAML
suite (tests/xaml/xaml_tests.cpp) compiles. Kept apart from tools/mutants.py,
which adds them to its catalogue.
"""


def xaml_mutants(XAML_SUITE):
    """(description, text to find, replacement, suites that should kill it)"""
    return [
        (
            "the tray goes into the anchor's nearest panel, inside its button",
            "        if (auto stack = chain[i].try_as<wuxc::StackPanel>()) {",
            "        if (auto stack = chain[i].try_as<wuxc::Panel>()) {",
            (XAML_SUITE,),
        ),
        (
            "the tray goes at the start of the row, not beside the anchor",
            "        host.Children().InsertAt(index, panel);",
            "        host.Children().InsertAt(0, panel);",
            (XAML_SUITE,),
        ),
        (
            "cells are sized from the anchor's width",
            "    const double anchorHeight = anchor.ActualHeight();",
            "    const double anchorHeight = anchor.ActualWidth();",
            (XAML_SUITE,),
        ),
        (
            "any scale is taken, however far out",
            "        scale = std::clamp(scale, 0.5, 3.0);",
            "",
            (XAML_SUITE,),
        ),
        (
            "a panel the taskbar dropped is taken as still there",
            "        if (wuxm::VisualTreeHelper::GetParent(existing)) {\n"
            "            return true;\n        }\n        tray.panel = nullptr;\n"
            "        tray.loggedEmbedded = false;",
            "        return true;\n        tray.panel = nullptr;\n"
            "        tray.loggedEmbedded = false;",
            (XAML_SUITE,),
        ),
        (
            "an empty tray has no handle",
            "        if (children.Size() == 0) {\n"
            "            children.Append(MakeTrayHandle(tray));\n        }",
            "",
            (XAML_SUITE,),
        ),
        (
            "icons past the limit get no chevron",
            "        if (!tray.hiddenEntries.empty()) {\n"
            "            children.Append(MakeChevron(tray));\n        }",
            "",
            (XAML_SUITE,),
        ),
        (
            "every refresh rebuilds the row",
            "        if (sameLayout) {",
            "        if (false && sameLayout) {",
            (XAML_SUITE,),
        ),
        (
            "a new tooltip is not put on the cell in place",
            "    if (current != tip) {",
            "    if (false) {",
            (XAML_SUITE,),
        ),
        (
            "a new name is not given to the cell in place",
            "        wux::Automation::AutomationProperties::SetName(face, winrt::hstring{entry.name});\n"
            "    }\n    // Only when the text changed",
            "    }\n    // Only when the text changed",
            (XAML_SUITE,),
        ),
        (
            "a new picture is not drawn in place",
            "                image.Source(bitmap);\n                drawn = true;",
            "                drawn = true;",
            (XAML_SUITE,),
        ),
        (
            "a picture taken away stays on the cell",
            "        if (face) {\n            face.Content(nullptr);\n        }",
            "",
            (XAML_SUITE,),
        ),
        (
            "the saved order is not applied to the row",
            "                         return OrderPositionOf(a.key) < OrderPositionOf(b.key);",
            "                         return false;",
            (XAML_SUITE,),
        ),
        (
            "a saved order loses the icons not on screen",
            "        if (std::find(order.begin(), order.end(), existing) == order.end()) {\n"
            "            order.push_back(existing);",
            "        if (std::find(order.begin(), order.end(), existing) == order.end()) {",
            (XAML_SUITE,),
        ),
        (
            "an icon's screen rect leaves out where the taskbar is",
            "        *out = SplitTray::IslandBoundsToScreen(POINT{window.left, window.top}, bounds.X,",
            "        *out = SplitTray::IslandBoundsToScreen(POINT{0, 0}, bounds.X,",
            (XAML_SUITE,),
        ),
        (
            "invoking a cell asks for its menu",
            "        SplitTray::ForwardKey(serial, false, AnchorOfSerial(serial));",
            "        SplitTray::ForwardKey(serial, true, AnchorOfSerial(serial));",
            (XAML_SUITE,),
        ),
        (
            "taking a tray out leaves its panel in the taskbar",
            "                host.Children().RemoveAt(index);",
            "",
            (XAML_SUITE,),
        ),
        (
            "embedding switched off leaves the trays in the taskbar",
            "        if (g_settings.embedInTaskbar && !g_unloading.load()) {",
            "        if (!g_unloading.load()) {",
            (XAML_SUITE,),
        ),
        # --- the pointer on a cell, the keyboard, the popup, the menus ----------
        (
            "a shaky click is taken for a drag",
            "    if (!g_drag.dragging && std::abs(x - g_drag.startX) < kDragThresholdDip) {",
            "    if (false) {",
            (XAML_SUITE,),
        ),
        (
            "a drag off the row reorders it anyway",
            "    if (outside) {\n        return true;\n    }\n\n    auto children = panel.Children();",
            "\n    auto children = panel.Children();",
            (XAML_SUITE,),
        ),
        (
            "a drag is also a click",
            "    if (wasDragging || suppressed) {",
            "    if (false) {",
            (XAML_SUITE,),
        ),
        (
            "an icon dropped off the row stays",
            "        if (wasOutside) {",
            "        if (false) {",
            (XAML_SUITE,),
        ),
        (
            "a drag cut short loses its order",
            "        if (auto panel = g_drag.panel.get()) {\n"
            "            CommitVisualOrder(panel);\n        }\n"
            "        g_drag.suppressClick = true;",
            "        g_drag.suppressClick = true;",
            (XAML_SUITE,),
        ),
        (
            "the release after a drag cut short is a click",
            "        g_drag.suppressClick = true;\n"
            "        Wh_Log(L\"[xaml][drag] capture lost mid-drag; order committed\");",
            "        Wh_Log(L\"[xaml][drag] capture lost mid-drag; order committed\");",
            (XAML_SUITE,),
        ),
        (
            "Shift and a click is an ordinary click",
            "            if (shift) {\n                // The quick way across",
            "            if (false) {\n                // The quick way across",
            (XAML_SUITE,),
        ),
        (
            "the right button is taken for the left",
            "            down = WM_RBUTTONDOWN;\n            up = WM_RBUTTONUP;",
            "            down = WM_LBUTTONDOWN;\n            up = WM_LBUTTONUP;",
            (XAML_SUITE,),
        ),
        (
            "a refresh that waited for the release is never asked for",
            "    if (g_refreshPending) {\n        RequestEmbeddedRefresh();\n    }\n"
            "    releaseCapture();",
            "    releaseCapture();",
            (XAML_SUITE,),
        ),
        (
            "the keyboard's menu is sent twice for one press",
            "    if (serial == lastSerial && now - lastTick < 500) {\n        return;\n    }",
            "",
            (XAML_SUITE,),
        ),
        (
            "hovering is not passed on to the application",
            "        SplitTray::ForwardHover(serial, screenPoint);\n        WaitToOpenPopup(serial);",
            "        WaitToOpenPopup(serial);",
            (XAML_SUITE,),
        ),
        (
            "an icon's own popup never opens",
            "    g_popupTimer.Stop();\n    g_popupTimer.Start();",
            "    g_popupTimer.Stop();",
            (XAML_SUITE,),
        ),
        (
            "an icon's own popup stays open when the pointer goes",
            "    if (g_popupOpen == serial) {\n        ClosePopup();\n    }",
            "",
            (XAML_SUITE,),
        ),
        (
            "the overflow popup is empty",
            "        cell.Height(kOverflowCellDip);\n        content.Children().Append(cell);",
            "        cell.Height(kOverflowCellDip);",
            (XAML_SUITE,),
        ),
        (
            "the menu's hide does not hide",
            "            SplitTray::SetIconHidden(key, !hidden);",
            "            SplitTray::SetIconHidden(key, hidden);",
            (XAML_SUITE,),
        ),
        (
            "the menu's move goes nowhere",
            "                SplitTray::MoveIconToTray(key, SplitTray::Destination::Tray(target));\n",
            "",
            (XAML_SUITE,),
        ),
        (
            "the empty tray offers icons already in it",
            "        if (entry.tray == trayNumber) {\n            continue;\n        }",
            "",
            (XAML_SUITE,),
        ),
        (
            "a change to the store does not reach the tray",
            "    if (g_unloading.load() || g_embeddedTrays.empty()) {\n        return;\n    }\n"
            "    RefreshEmbeddedTray();",
            "    if (g_unloading.load() || g_embeddedTrays.empty()) {\n        return;\n    }",
            (XAML_SUITE,),
        ),
        (
            "another process's second taskbar is taken for this one's",
            "            if (owner != GetCurrentProcessId()) {\n                return TRUE;\n"
            "            }\n            if (MonitorFromWindow(wnd, MONITOR_DEFAULTTONULL)",
            "            if (MonitorFromWindow(wnd, MONITOR_DEFAULTTONULL)",
            (XAML_SUITE,),
        ),
    ]
