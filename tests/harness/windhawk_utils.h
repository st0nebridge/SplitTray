// ============================================================================
// Module:  tests/harness/windhawk_utils.h
// Purpose: Stand in for Windhawk's windhawk_utils.h. Only the pieces the mod
//          actually uses are provided: the StringSetting RAII wrapper and the
//          cross-thread subclass helpers.
// Deps:    tests/harness/windhawk_api.h, commctrl.h.
// ============================================================================

#pragma once

#include <windhawk_api.h>

#include <commctrl.h>

#include <cstdio>
#include <memory>

namespace WindhawkUtils {

using WH_SUBCLASSPROC = LRESULT(CALLBACK*)(HWND hWnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam,
                                           DWORD_PTR dwRefData);

inline int& SubclassCallCount() {
    static int count = 0;
    return count;
}

inline int& UnsubclassCallCount() {
    static int count = 0;
    return count;
}

// A real subclass, not a stub: the integration test drives the mod's
// Shell_TrayWnd handler through this, including from the mod's own tray thread,
// so the cross-thread path has to work exactly as Windhawk's does. The
// implementation below mirrors Windhawk's: a CALLWNDPROC hook on the window's
// thread plus a registered message, because SetWindowSubclass may only be called
// on the thread that owns the window.
namespace detail {

inline UINT GetSubclassRegisteredMsg() {
    static UINT msg = RegisterWindowMessageW(
        L"SplitTrayTestHarness_SetWindowSubclassFromAnyThread");
    return msg;
}

// Forwards comctl32's subclass callback shape to Windhawk's, and handles the
// registered "remove yourself" message.
inline LRESULT CALLBACK SubclassWrapper(HWND hWnd,
                                       UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData) {
    if (uMsg == WM_NCDESTROY ||
        (uMsg == GetSubclassRegisteredMsg() && !wParam &&
         static_cast<UINT_PTR>(lParam) == uIdSubclass)) {
        RemoveWindowSubclass(hWnd, SubclassWrapper, uIdSubclass);
    }
    auto proc = reinterpret_cast<WH_SUBCLASSPROC>(uIdSubclass);
    return proc(hWnd, uMsg, wParam, lParam, dwRefData);
}

inline int& HookFireCount() {
    static int count = 0;
    return count;
}

struct SubclassRequest {
    UINT_PTR uIdSubclass;
    DWORD_PTR dwRefData;
    BOOL result;
};

inline LRESULT CALLBACK CallWndProcToSetWindowSubclass(int nCode,
                                                      WPARAM wParam,
                                                      LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const CWPSTRUCT* cwp = reinterpret_cast<const CWPSTRUCT*>(lParam);
        HookFireCount()++;
        if (cwp->message == GetSubclassRegisteredMsg() && cwp->wParam) {
            auto* request = reinterpret_cast<SubclassRequest*>(cwp->lParam);
            request->result =
                SetWindowSubclass(cwp->hwnd, SubclassWrapper, request->uIdSubclass,
                                  request->dwRefData);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

}  // namespace detail

inline BOOL SetWindowSubclassFromAnyThread(HWND hWnd,
                                           WH_SUBCLASSPROC pfnSubclass,
                                           DWORD_PTR dwRefData) {
    SubclassCallCount()++;
    const DWORD threadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (threadId == 0) {
        return FALSE;
    }
    if (threadId == GetCurrentThreadId()) {
        return SetWindowSubclass(hWnd, detail::SubclassWrapper,
                                 reinterpret_cast<UINT_PTR>(pfnSubclass), dwRefData);
    }

    HHOOK hook = SetWindowsHookExW(WH_CALLWNDPROC,
                                   detail::CallWndProcToSetWindowSubclass, nullptr,
                                   threadId);
    if (!hook) {
        printf("    [harness] SetWindowsHookExW(thread %lu) failed: %lu\n", threadId,
               GetLastError());
        return FALSE;
    }
    detail::SubclassRequest request{reinterpret_cast<UINT_PTR>(pfnSubclass),
                                    dwRefData, FALSE};
    SendMessageW(hWnd, detail::GetSubclassRegisteredMsg(), TRUE,
                 reinterpret_cast<LPARAM>(&request));
    UnhookWindowsHookEx(hook);
    if (!request.result) {
        printf("    [harness] cross-thread subclass of %p (thread %lu) did not take;"
               " hook fired=%d, last error %lu\n",
               hWnd, threadId, detail::HookFireCount(), GetLastError());
    }
    return request.result;
}

inline void RemoveWindowSubclassFromAnyThread(HWND hWnd,
                                              WH_SUBCLASSPROC pfnSubclass) {
    UnsubclassCallCount()++;
    SendMessageW(hWnd, detail::GetSubclassRegisteredMsg(), FALSE,
                 reinterpret_cast<LPARAM>(pfnSubclass));
}

class StringSetting {
   public:
    StringSetting() = default;
    explicit StringSetting(PCWSTR value) : m_value(value) {}
    operator PCWSTR() const { return m_value.get(); }
    PCWSTR get() const { return m_value.get(); }

    template <typename... Args>
    static StringSetting make(PCWSTR valueName, Args... args) {
        return StringSetting(Wh_GetStringSetting(valueName, args...));
    }

   private:
    template <auto fn>
    struct deleter_from_fn {
        template <typename T>
        constexpr void operator()(T* arg) const {
            fn(arg);
        }
    };
    using unique_string =
        std::unique_ptr<const WCHAR[], deleter_from_fn<Wh_FreeStringSetting>>;

    unique_string m_value;
};

}  // namespace WindhawkUtils
