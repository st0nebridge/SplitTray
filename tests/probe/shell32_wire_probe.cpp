// ============================================================================
// Module:  tests/probe/shell32_wire_probe.cpp
// Purpose: Capture the EXACT byte layout that the real shell32.dll puts on the
//          wire when an application calls Shell_NotifyIcon, so the mod's parser
//          is written against observed reality instead of guesswork.
//
// How it works
//   Shell_NotifyIcon(W|A) does not talk to the shell through an API. It does:
//       hTray = FindWindowW(L"Shell_TrayWnd", NULL);
//       SendMessageTimeout(hTray, WM_COPYDATA, (WPARAM)nid.hWnd, &cds);
//   FindWindowW only sees windows on the *calling thread's desktop*. So the
//   probe creates a private desktop, puts its own window of class
//   "Shell_TrayWnd" on it, and then calls Shell_NotifyIcon from that desktop.
//   Real shell32 therefore hands us the real payload, with zero risk to the
//   live shell and no code injection anywhere.
//
// Output: a hex dump plus a decoded field table for each notification shape,
//         written to stdout. Used to author src/split-tray.wh.cpp's parser and
//         to generate the golden buffers in tests/regression. Then the same for
//         Shell_NotifyIconGetRect, which asks the tray where an icon is
//         (tests/probe/probe-rect-output-26100.txt).
// Deps:   user32, shell32, kernel32 only.
// ============================================================================

#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// --- Captured payloads ------------------------------------------------------

struct Capture {
    std::string label;
    ULONG_PTR dwData = 0;
    DWORD cbData = 0;
    std::vector<BYTE> bytes;
    bool received = false;
};

Capture* g_current = nullptr;
HWND g_ownerWnd = nullptr;

// Shell_NotifyIconGetRect travels the same window with its own dwData. While a
// query runs, every non-notification message is recorded here and answered
// from g_rectReplies, one reply per message in the order they arrive.
std::vector<Capture>* g_rectQueries = nullptr;
std::vector<LRESULT> g_rectReplies;

// --- The fake Shell_TrayWnd -------------------------------------------------

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COPYDATA && g_rectQueries) {
        auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (!cds) {
            return 0;
        }
        Capture cap;
        cap.received = true;
        cap.dwData = cds->dwData;
        cap.cbData = cds->cbData;
        if (cds->lpData) {
            cap.bytes.assign(static_cast<const BYTE*>(cds->lpData),
                             static_cast<const BYTE*>(cds->lpData) + cds->cbData);
        }
        const size_t index = g_rectQueries->size();
        g_rectQueries->push_back(cap);
        const LRESULT reply = index < g_rectReplies.size() ? g_rectReplies[index] : 0;
        printf("  [recv] query %zu: dwData=%llu cbData=%lu sender=0x%llX -> "
               "replying 0x%llX\n",
               index, static_cast<unsigned long long>(cds->dwData), cds->cbData,
               static_cast<unsigned long long>(wParam),
               static_cast<unsigned long long>(reply));
        return reply;
    }
    if (msg == WM_COPYDATA) {
        auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (g_current && cds) {
            g_current->received = true;
            g_current->dwData = cds->dwData;
            g_current->cbData = cds->cbData;
            g_current->bytes.assign(
                static_cast<const BYTE*>(cds->lpData),
                static_cast<const BYTE*>(cds->lpData) + cds->cbData);
            printf("  [recv] sender hWnd (wParam) = 0x%llX\n",
                   static_cast<unsigned long long>(wParam));
        }
        return TRUE;  // shell returns TRUE for a handled tray notification
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --- Reporting --------------------------------------------------------------

void HexDump(const std::vector<BYTE>& b, size_t maxBytes) {
    size_t n = b.size() < maxBytes ? b.size() : maxBytes;
    for (size_t i = 0; i < n; i += 16) {
        printf("  %04zX  ", i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < n)
                printf("%02X ", b[i + j]);
            else
                printf("   ");
        }
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < n; j++) {
            BYTE c = b[i + j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("|\n");
    }
    if (b.size() > n) {
        printf("  ... (%zu more bytes)\n", b.size() - n);
    }
}

DWORD Rd32(const std::vector<BYTE>& b, size_t off) {
    DWORD v = 0;
    if (off + 4 <= b.size()) {
        memcpy(&v, b.data() + off, 4);
    }
    return v;
}

// Print the fields we believe live at fixed offsets, so the layout can be
// confirmed against the values the probe actually passed in.
void Decode(const Capture& c) {
    printf("  dwData  = %llu\n", static_cast<unsigned long long>(c.dwData));
    printf("  cbData  = %lu\n", c.cbData);
    if (c.bytes.size() < 8) {
        printf("  (payload too small to decode)\n");
        return;
    }
    printf("  [+0x00] dwSignature      = 0x%08lX\n", Rd32(c.bytes, 0));
    printf("  [+0x04] dwMessage        = %lu\n", Rd32(c.bytes, 4));
    printf("  --- embedded NOTIFYICONDATA (starts at +0x08) ---\n");
    printf("  [+0x08] nid.cbSize       = %lu (0x%lX)\n", Rd32(c.bytes, 8),
           Rd32(c.bytes, 8));
    printf("  [+0x0C] nid.hWnd         = 0x%08lX\n", Rd32(c.bytes, 12));
    printf("  [+0x10] nid.uID          = %lu\n", Rd32(c.bytes, 16));
    printf("  [+0x14] nid.uFlags       = 0x%08lX\n", Rd32(c.bytes, 20));
    printf("  [+0x18] nid.uCallbackMsg = 0x%08lX\n", Rd32(c.bytes, 24));
    printf("  [+0x1C] nid.hIcon        = 0x%08lX\n", Rd32(c.bytes, 28));

    // szTip: try both encodings at +0x20 and report which one looks right.
    const size_t tipOff = 32;
    if (tipOff + 4 <= c.bytes.size()) {
        const BYTE* p = c.bytes.data() + tipOff;
        printf("  [+0x20] first 8 bytes of tip area: %02X %02X %02X %02X %02X "
               "%02X %02X %02X\n",
               p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        if (p[1] == 0 && p[0] != 0) {
            wchar_t buf[130] = {};
            size_t avail = (c.bytes.size() - tipOff) / sizeof(wchar_t);
            if (avail > 128) avail = 128;
            memcpy(buf, p, avail * sizeof(wchar_t));
            buf[128] = 0;
            printf("  [+0x20] szTip as UTF-16  = \"%ls\"\n", buf);
        } else {
            char buf[130] = {};
            size_t avail = c.bytes.size() - tipOff;
            if (avail > 128) avail = 128;
            memcpy(buf, p, avail);
            buf[128] = 0;
            printf("  [+0x20] szTip as ANSI    = \"%s\"\n", buf);
        }
    }
}

// --- Notification shapes we exercise ---------------------------------------

struct Shape {
    const char* label;
    DWORD cbSize;    // what the app sets in nid.cbSize
    UINT uFlags;
    DWORD uVersion;  // 0 = skip NIM_SETVERSION
    bool ansi;
    bool useGuid;
};

HWND CreateHiddenOwner(HINSTANCE hInst) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = L"SplitTrayProbeOwner";
    RegisterClassW(&wc);
    return CreateWindowExW(0, L"SplitTrayProbeOwner", L"probe owner", 0, 0, 0, 0,
                           0, nullptr, nullptr, hInst, nullptr);
}

void PumpBriefly() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

bool RunShape(const Shape& shape, HICON hIcon, std::vector<Capture>& out) {
    Capture cap;
    cap.label = shape.label;
    g_current = &cap;

    const UINT kCallbackMsg = WM_APP + 0x123;
    // A stable GUID for the NIF_GUID shape.
    static const GUID kProbeGuid = {0x11223344,
                                    0x5566,
                                    0x7788,
                                    {0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
                                     0x00}};
    BOOL ok = FALSE;

    if (shape.ansi) {
        NOTIFYICONDATAA nid = {};
        nid.cbSize = shape.cbSize;
        nid.hWnd = g_ownerWnd;
        nid.uID = 4242;
        nid.uFlags = shape.uFlags;
        nid.uCallbackMessage = kCallbackMsg;
        nid.hIcon = hIcon;
        strcpy(nid.szTip, "PROBE-ANSI-TIP");
        if (shape.cbSize >= sizeof(NOTIFYICONDATAA)) {
            strcpy(nid.szInfo, "PROBE-ANSI-INFO");
            strcpy(nid.szInfoTitle, "PROBE-ANSI-TITLE");
        }
        if (shape.useGuid) {
            nid.guidItem = kProbeGuid;
        }
        ok = Shell_NotifyIconA(NIM_ADD, &nid);
    } else {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = shape.cbSize;
        nid.hWnd = g_ownerWnd;
        nid.uID = 4242;
        nid.uFlags = shape.uFlags;
        nid.uCallbackMessage = kCallbackMsg;
        nid.hIcon = hIcon;
        wcscpy(nid.szTip, L"PROBE-TIP-W");
        if (shape.cbSize >= sizeof(NOTIFYICONDATAW)) {
            wcscpy(nid.szInfo, L"PROBE-INFO-W");
            wcscpy(nid.szInfoTitle, L"PROBE-TITLE-W");
        }
        if (shape.useGuid) {
            nid.guidItem = kProbeGuid;
        }
        ok = Shell_NotifyIconW(NIM_ADD, &nid);
    }

    PumpBriefly();

    printf("\n=== shape: %s ===\n", shape.label);
    printf("  app passed cbSize=%lu uFlags=0x%X ansi=%d guid=%d -> "
           "Shell_NotifyIcon returned %d\n",
           shape.cbSize, shape.uFlags, shape.ansi ? 1 : 0,
           shape.useGuid ? 1 : 0, ok ? 1 : 0);
    printf("  app owner hWnd = 0x%llX, hIcon = 0x%llX, uID = 4242, "
           "uCallbackMessage = 0x%X\n",
           reinterpret_cast<unsigned long long>(g_ownerWnd),
           reinterpret_cast<unsigned long long>(hIcon), kCallbackMsg);

    if (!cap.received) {
        printf("  !! no WM_COPYDATA captured\n");
        g_current = nullptr;
        return false;
    }
    Decode(cap);
    printf("  hex dump:\n");
    HexDump(cap.bytes, 64);
    out.push_back(cap);
    g_current = nullptr;
    return true;
}

// Emit a C++ literal of one capture so it can be pasted into the regression
// tests as a golden buffer.
void EmitGolden(const Capture& c, const char* name) {
    printf("\n// golden: %s (dwData=%llu, cbData=%lu)\n", c.label.c_str(),
           static_cast<unsigned long long>(c.dwData), c.cbData);
    printf("static const unsigned char %s[] = {", name);
    for (size_t i = 0; i < c.bytes.size(); i++) {
        if (i % 16 == 0) printf("\n    ");
        printf("0x%02X,", c.bytes[i]);
    }
    printf("\n};\n");
}

// Shell_NotifyIconGetRect: what it sends, and how it reads the answer back.
// Each round answers the messages with chosen values and prints the RECT the
// caller ends up with, so the reply encoding is read off the result rather
// than assumed. Tray libraries such as Tauri's ask this before handling a
// click, and drop the click when it fails.
void RunRectQueries() {
    struct Round {
        const char* label;
        std::vector<LRESULT> replies;
        bool useGuid;
    };
    auto pack = [](int x, int y) {
        return static_cast<LRESULT>(static_cast<DWORD>(
            MAKELONG(static_cast<WORD>(x), static_cast<WORD>(y))));
    };
    const Round rounds[] = {
        {"distinct replies per message", {pack(111, 222), pack(333, 444), pack(555, 666)},
         false},
        {"negative coordinates", {pack(-1800, 1040), pack(-1768, 1080), pack(-1700, 1100)},
         false},
        {"tray answers 0", {0, 0, 0}, false},
        {"icon at the screen origin", {pack(24, 24), 0}, false},
        {"identified by GUID", {pack(10, 20), pack(30, 40), pack(50, 60)}, true},
    };
    static const GUID kProbeGuid = {0x11223344,
                                    0x5566,
                                    0x7788,
                                    {0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
                                     0x00}};

    for (const auto& round : rounds) {
        std::vector<Capture> queries;
        g_rectQueries = &queries;
        g_rectReplies = round.replies;

        NOTIFYICONIDENTIFIER id = {};
        id.cbSize = sizeof(id);
        id.hWnd = g_ownerWnd;
        id.uID = 4242;
        if (round.useGuid) {
            id.guidItem = kProbeGuid;
        }
        RECT rect = {-1, -1, -1, -1};
        printf("\n=== Shell_NotifyIconGetRect: %s ===\n", round.label);
        const HRESULT hr = Shell_NotifyIconGetRect(&id, &rect);
        PumpBriefly();
        g_rectQueries = nullptr;

        printf("  returned hr=0x%08lX rect={%ld, %ld, %ld, %ld} after %zu message(s)\n",
               static_cast<unsigned long>(hr), rect.left, rect.top, rect.right,
               rect.bottom, queries.size());
        for (size_t i = 0; i < queries.size(); i++) {
            printf("  query %zu payload (dwData=%llu, cbData=%lu):\n", i,
                   static_cast<unsigned long long>(queries[i].dwData),
                   queries[i].cbData);
            HexDump(queries[i].bytes, 64);
        }
    }
}

DWORD WINAPI ProbeThread(LPVOID param) {
    HDESK hDesk = static_cast<HDESK>(param);
    if (!SetThreadDesktop(hDesk)) {
        printf("SetThreadDesktop failed: %lu\n", GetLastError());
        return 1;
    }

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"Shell_TrayWnd";
    if (!RegisterClassW(&wc)) {
        printf("RegisterClassW(Shell_TrayWnd) failed: %lu\n", GetLastError());
        return 1;
    }
    HWND hTray = CreateWindowExW(0, L"Shell_TrayWnd", nullptr, 0, 0, 0, 16, 16,
                                 nullptr, nullptr, hInst, nullptr);
    if (!hTray) {
        printf("CreateWindowExW(Shell_TrayWnd) failed: %lu\n", GetLastError());
        return 1;
    }

    HWND found = FindWindowW(L"Shell_TrayWnd", nullptr);
    printf("private desktop tray window = 0x%llX, FindWindowW found 0x%llX %s\n",
           reinterpret_cast<unsigned long long>(hTray),
           reinterpret_cast<unsigned long long>(found),
           (found == hTray) ? "(MATCH - probe is isolated)" : "(MISMATCH!)");
    if (found != hTray) {
        printf("!! refusing to continue: would be talking to the real shell\n");
        return 1;
    }

    g_ownerWnd = CreateHiddenOwner(hInst);
    HICON hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    const Shape shapes[] = {
        {"V4 Unicode, NIF_MESSAGE|NIF_ICON|NIF_TIP",
         sizeof(NOTIFYICONDATAW),
         NIF_MESSAGE | NIF_ICON | NIF_TIP,
         0,
         false,
         false},
        {"V4 Unicode + NIF_GUID",
         sizeof(NOTIFYICONDATAW),
         NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID,
         0,
         false,
         true},
        {"V4 Unicode + NIF_STATE (hidden)",
         sizeof(NOTIFYICONDATAW),
         NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE,
         0,
         false,
         false},
        {"V3 Unicode (NOTIFYICONDATAW_V3_SIZE)",
         NOTIFYICONDATAW_V3_SIZE,
         NIF_MESSAGE | NIF_ICON | NIF_TIP,
         0,
         false,
         false},
        {"V2 Unicode (NOTIFYICONDATAW_V2_SIZE)",
         NOTIFYICONDATAW_V2_SIZE,
         NIF_MESSAGE | NIF_ICON | NIF_TIP,
         0,
         false,
         false},
        {"V1 Unicode (NOTIFYICONDATAW_V1_SIZE)",
         NOTIFYICONDATAW_V1_SIZE,
         NIF_MESSAGE | NIF_ICON | NIF_TIP,
         0,
         false,
         false},
        {"V4 ANSI", sizeof(NOTIFYICONDATAA),
         NIF_MESSAGE | NIF_ICON | NIF_TIP, 0, true, false},
        {"V1 ANSI (NOTIFYICONDATAA_V1_SIZE)", NOTIFYICONDATAA_V1_SIZE,
         NIF_MESSAGE | NIF_ICON | NIF_TIP, 0, true, false},
    };

    std::vector<Capture> caps;
    for (const auto& s : shapes) {
        RunShape(s, hIcon, caps);
    }

    // NIM_SETVERSION / NIM_DELETE also travel the same wire; confirm dwMessage.
    {
        Capture cap;
        cap.label = "NIM_SETVERSION(4)";
        g_current = &cap;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = g_ownerWnd;
        nid.uID = 4242;
        nid.uVersion = NOTIFYICON_VERSION_4;
        BOOL ok = Shell_NotifyIconW(NIM_SETVERSION, &nid);
        PumpBriefly();
        printf("\n=== shape: NIM_SETVERSION(4) === returned %d\n", ok ? 1 : 0);
        if (cap.received) {
            Decode(cap);
            printf("  [+0x%03X] uVersion field (expect 4)= %lu\n", 8 + 800,
                   Rd32(cap.bytes, 8 + 800));
            caps.push_back(cap);
        } else {
            printf("  !! no capture\n");
        }
        g_current = nullptr;
    }
    {
        Capture cap;
        cap.label = "NIM_DELETE";
        g_current = &cap;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = g_ownerWnd;
        nid.uID = 4242;
        BOOL ok = Shell_NotifyIconW(NIM_DELETE, &nid);
        PumpBriefly();
        printf("\n=== shape: NIM_DELETE === returned %d\n", ok ? 1 : 0);
        if (cap.received) Decode(cap);
        g_current = nullptr;
    }

    RunRectQueries();

    printf("\n================ GOLDEN BUFFERS ================\n");
    for (size_t i = 0; i < caps.size(); i++) {
        char name[64];
        snprintf(name, sizeof(name), "kGolden%zu", i);
        EmitGolden(caps[i], name);
    }

    printf("\n================ SUMMARY ================\n");
    printf("%-42s %8s %8s %10s\n", "shape", "dwData", "cbData", "nid.cbSize");
    for (const auto& c : caps) {
        printf("%-42s %8llu %8lu %10lu\n", c.label.c_str(),
               static_cast<unsigned long long>(c.dwData), c.cbData,
               Rd32(c.bytes, 8));
    }
    return 0;
}

}  // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("shell32 tray wire probe\n");

    OSVERSIONINFOEXW osv = {sizeof(osv)};
    // GetVersionEx is lie-prone; read the real build from the registry instead.
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR buildLab[256] = {};
        DWORD cb = sizeof(buildLab);
        if (RegQueryValueExW(hKey, L"BuildLabEx", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(buildLab),
                            &cb) != ERROR_SUCCESS) {
            cb = sizeof(buildLab);
            RegQueryValueExW(hKey, L"CurrentBuild", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buildLab), &cb);
        }
        printf("windows build: %ls\n", buildLab);
        RegCloseKey(hKey);
    }
    (void)osv;

    HDESK hDesk = CreateDesktopW(L"SplitTrayWireProbe", nullptr, nullptr, 0,
                                 GENERIC_ALL, nullptr);
    if (!hDesk) {
        printf("CreateDesktopW failed: %lu\n", GetLastError());
        return 1;
    }

    DWORD tid = 0;
    HANDLE hThread = CreateThread(nullptr, 0, ProbeThread, hDesk, 0, &tid);
    if (!hThread) {
        printf("CreateThread failed: %lu\n", GetLastError());
        CloseDesktop(hDesk);
        return 1;
    }
    WaitForSingleObject(hThread, 30000);
    DWORD exitCode = 1;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    CloseDesktop(hDesk);
    return static_cast<int>(exitCode);
}
