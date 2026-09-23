// ============================================================================
// Module:  tests/harness/windhawk_api.h
// Purpose: Stand in for Windhawk's own windhawk_api.h so that
//          src/split-tray.wh.cpp can be compiled into a plain test executable.
//          The tests then exercise the shipped source, not a transcription of
//          it.
// Deps:    windows.h only. Put this directory first on the include path.
// ============================================================================

#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#define WH_MOD_ID L"split-tray-test"
#define WH_MOD_VERSION L"1.0.0"

namespace SplitTrayTestHarness {

// Settings the test sets up before calling into the mod. Keys are the resolved
// setting names, e.g. L"perProcessRouting[0].exe".
inline std::map<std::wstring, std::wstring>& StringSettings() {
    static std::map<std::wstring, std::wstring> map;
    return map;
}

inline std::map<std::wstring, int>& IntSettings() {
    static std::map<std::wstring, int> map;
    return map;
}

inline std::vector<std::wstring>& LogLines() {
    static std::vector<std::wstring> lines;
    return lines;
}

// Windhawk's Wh_Log may be called from any thread, and the mod does - its tray
// thread logs while a test reads what was logged - so the stand-in takes a lock
// as the real one is safe to (DECISIONS.md 21).
inline std::mutex& LogMutex() {
    static std::mutex mutex;
    return mutex;
}

// A copy of the log so far, safe to read while the mod's threads are logging.
inline std::vector<std::wstring> LogSnapshot() {
    std::lock_guard<std::mutex> lock(LogMutex());
    return LogLines();
}

inline bool& LogToStdout() {
    static bool enabled = false;
    return enabled;
}

// The mod's own storage, which Windhawk persists per mod. Backed by a real map
// rather than stubbed away: the placement and order state round-trips through
// it, so a stub that silently dropped writes would make those tests pass while
// the feature was broken (DECISIONS.md 21).
inline std::map<std::wstring, std::wstring>& StoredValues() {
    static std::map<std::wstring, std::wstring> map;
    return map;
}

inline void ResetSettings() {
    StringSettings().clear();
    IntSettings().clear();
    StoredValues().clear();
    std::lock_guard<std::mutex> lock(LogMutex());
    LogLines().clear();
}

inline std::wstring FormatName(PCWSTR format, va_list args) {
    wchar_t buffer[512];
    _vsnwprintf(buffer, 511, format, args);
    buffer[511] = L'\0';
    return std::wstring(buffer);
}

}  // namespace SplitTrayTestHarness

inline void Wh_Log(PCWSTR format, ...) {
    va_list args;
    va_start(args, format);
    wchar_t buffer[1024];
    _vsnwprintf(buffer, 1023, format, args);
    buffer[1023] = L'\0';
    va_end(args);
    std::lock_guard<std::mutex> lock(SplitTrayTestHarness::LogMutex());
    SplitTrayTestHarness::LogLines().push_back(buffer);
    if (SplitTrayTestHarness::LogToStdout()) {
        wprintf(L"    [mod] %ls\n", buffer);
    }
}

inline int Wh_GetIntSetting(PCWSTR valueName, ...) {
    va_list args;
    va_start(args, valueName);
    const std::wstring name = SplitTrayTestHarness::FormatName(valueName, args);
    va_end(args);
    auto& map = SplitTrayTestHarness::IntSettings();
    auto it = map.find(name);
    return it == map.end() ? 0 : it->second;
}

inline PCWSTR Wh_GetStringSetting(PCWSTR valueName, ...) {
    va_list args;
    va_start(args, valueName);
    const std::wstring name = SplitTrayTestHarness::FormatName(valueName, args);
    va_end(args);
    auto& map = SplitTrayTestHarness::StringSettings();
    auto it = map.find(name);
    const std::wstring value = (it == map.end()) ? std::wstring() : it->second;
    // Windhawk hands out a buffer the caller frees with Wh_FreeStringSetting.
    wchar_t* copy = new wchar_t[value.size() + 1];
    memcpy(copy, value.c_str(), (value.size() + 1) * sizeof(wchar_t));
    return copy;
}

inline void Wh_FreeStringSetting(PCWSTR string) {
    delete[] string;
}

inline size_t Wh_GetStringValue(PCWSTR valueName, PWSTR stringBuffer,
                                size_t bufferChars) {
    auto& map = SplitTrayTestHarness::StoredValues();
    auto it = map.find(valueName);
    if (it == map.end() || bufferChars == 0) {
        if (bufferChars > 0) {
            stringBuffer[0] = L'\0';
        }
        return 0;
    }
    const size_t copied = (it->second.size() < bufferChars - 1)
                              ? it->second.size()
                              : bufferChars - 1;
    memcpy(stringBuffer, it->second.c_str(), copied * sizeof(wchar_t));
    stringBuffer[copied] = L'\0';
    return copied;
}

inline BOOL Wh_SetStringValue(PCWSTR valueName, PCWSTR value) {
    SplitTrayTestHarness::StoredValues()[valueName] = value ? value : L"";
    return TRUE;
}

inline BOOL Wh_SetFunctionHook(void*, void*, void**) {
    return TRUE;
}

inline BOOL Wh_ApplyHookOperations() {
    return TRUE;
}
