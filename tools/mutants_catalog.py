#!/usr/bin/env python3
"""The mutants for the findings of Windhawk's catalog review of 2026-10-09,
DECISIONS 95 and 96. Kept apart from tools/mutants.py, which adds them to its
catalogue.
"""


def catalog_mutants(UNIT_SUITE, INTEGRATION_SUITE, XAML_SUITE):
    """(description, text to find, replacement, suites that should kill it)"""
    return [
        # --- the TaskbarHost layout on ARM64 ------------------------------------
        (
            "the ARM64 decoder takes any load as the last instruction",
            "        (instructions[3] & 0xFFF00FE0) == 0xF8400C00) {",
            "        true) {",
            (UNIT_SUITE,),
        ),
        (
            "the ARM64 decoder takes any second instruction",
            "        (instructions[1] & 0xFFC07FFF) == 0xA9807BFD &&",
            "        true &&",
            (UNIT_SUITE,),
        ),
        (
            "the ARM64 decoder reads the offset from the wrong bits",
            "        *offset = (instructions[3] >> 12) & 0xFF;",
            "        *offset = (instructions[3] >> 10) & 0xFF;",
            (UNIT_SUITE,),
        ),
        # --- each module's symbols once ------------------------------------------
        (
            "SystemTray.dll mapped as data is taken for SystemTray.dll loaded to run",
            "    return loaded && loaded == systemTray && (flags & kNotToRun) == 0;",
            "    return loaded && loaded == systemTray;",
            (UNIT_SUITE,),
        ),
        (
            "any module's load is taken for SystemTray.dll's",
            "    return loaded && loaded == systemTray && (flags & kNotToRun) == 0;",
            "    return loaded != nullptr;",
            (UNIT_SUITE,),
        ),
        (
            "SystemTray.dll's symbols are asked for again after an answer",
            "    if (!module || g_systemTraySymbolsTried.exchange(true)) {",
            "    if (!module) {",
            (XAML_SUITE,),
        ),
        (
            "taskbar.dll's symbols are asked for again after an answer",
            "    if (!module || g_taskbarSymbolsTried.exchange(true)) {",
            "    if (!module) {",
            (XAML_SUITE,),
        ),
        (
            "two threads at once both ask for SystemTray.dll's symbols",
            "    if (!module || g_systemTraySymbolsTried.exchange(true)) {",
            "    if (!module || g_systemTraySymbolsTried.load()) {\n"
            "        return false;\n    }\n    if (false) {",
            (XAML_SUITE,),
        ),
        (
            "no module counts as SystemTray.dll's symbols asked for",
            "    if (!module || g_systemTraySymbolsTried.exchange(true)) {",
            "    if (g_systemTraySymbolsTried.exchange(true) || !module) {",
            (XAML_SUITE,),
        ),
        (
            "LoadLibraryExW is hooked although SystemTray.dll is loaded already",
            "    if (systemTray) {\n        HookSystemTraySymbols(systemTray);\n"
            "    } else if (auto loadLibraryExW",
            "    if (systemTray) {\n        HookSystemTraySymbols(systemTray);\n"
            "    }\n    if (auto loadLibraryExW",
            (XAML_SUITE,),
        ),
        (
            "the LoadLibraryExW hook hands its caller an error of its own",
            "        SetLastError(error);\n    }\n    return module;",
            "    }\n    return module;",
            (XAML_SUITE,),
        ),
        # --- unloading waits until it is done ------------------------------------
        (
            "unloading does not wait for the tray thread",
            "    } while (WaitForSingleObject(g_trayThread, 20) == WAIT_TIMEOUT);",
            "    } while (false);",
            (INTEGRATION_SUITE,),
        ),
        # --- the timer's pass -----------------------------------------------------
        (
            "the displays are asked for under the lock, every tick",
            "    const auto monitors = g_enumerateMonitors();\n    {\n"
            "        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        RecomputeGeometryLocked(monitors);\n        if (!g_unloading.load()) {",
            "    {\n        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        const auto monitors = g_enumerateMonitors();\n"
            "        RecomputeGeometryLocked(monitors);\n        if (!g_unloading.load()) {",
            (UNIT_SUITE,),
        ),
        (
            "the displays are asked for under the lock while routing is replayed",
            "    const auto monitors = g_enumerateMonitors();\n\n    {\n"
            "        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        std::vector<int> wasAvailable;",
            "    {\n        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        const auto monitors = g_enumerateMonitors();\n"
            "        std::vector<int> wasAvailable;",
            (UNIT_SUITE,),
        ),
        (
            "the displays are asked for under the lock as an icon is moved",
            "    const auto monitors = g_enumerateMonitors();\n\n    {\n"
            "        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        RememberPlacement(key, destination);",
            "    {\n        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        const auto monitors = g_enumerateMonitors();\n"
            "        RememberPlacement(key, destination);",
            (UNIT_SUITE,),
        ),
        (
            "the displays are asked for under the lock as settings change",
            "    const auto monitors = g_enumerateMonitors();\n\n    {\n"
            "        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        RecomputeGeometryLocked(monitors);\n\n"
            "        for (auto* list : {&g_icons, &g_primaryOnly}) {",
            "    {\n        std::lock_guard<std::mutex> lock(g_mutex);\n"
            "        const auto monitors = g_enumerateMonitors();\n"
            "        RecomputeGeometryLocked(monitors);\n\n"
            "        for (auto* list : {&g_icons, &g_primaryOnly}) {",
            (UNIT_SUITE,),
        ),
        (
            "a floating tray is put back on top on every tick",
            "        if (moved) {\n            SetWindowPos(",
            "        if (true) {\n            SetWindowPos(",
            (INTEGRATION_SUITE,),
        ),
        (
            "a tray that cannot be embedded is looked for on every tick",
            "    return ticksWaited < 5 || ticksWaited % 15 == 0;",
            "    return true;",
            (UNIT_SUITE,),
        ),
        (
            "a tray that cannot be embedded is never looked for after ten seconds",
            "    return ticksWaited < 5 || ticksWaited % 15 == 0;",
            "    return ticksWaited < 5;",
            (UNIT_SUITE,),
        ),
        # --- the rest -------------------------------------------------------------
        (
            "the arrange window's lists are not scaled for the display",
            "    m.listWidth = ScaleForDpi(250, dpi);",
            "    m.listWidth = 250;",
            (UNIT_SUITE,),
        ),
        (
            "the developer's XAML dump never prints",
            "    if (g_dumpXamlTree && !g_loggedTargetStack) {",
            "    if (false && g_dumpXamlTree && !g_loggedTargetStack) {",
            (XAML_SUITE,),
        ),
    ]
