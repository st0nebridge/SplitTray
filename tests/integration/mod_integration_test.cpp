// ============================================================================
// Module:  tests/integration/mod_integration_test.cpp
// Purpose: Drive the whole mod end to end with the real shell32 producing the
//          messages, in an isolated process, with no risk to the live shell.
//
//          The unit tests in tests/regression cover the pure logic. This covers
//          the part that cannot be unit tested: that subclassing Shell_TrayWnd
//          actually intercepts what applications send, that suppression removes
//          an icon from the shell, that the secondary tray window really appears
//          with the right geometry, that a click on it reaches the owning
//          application, and that unloading puts swallowed icons back.
//
// How it stays isolated
//          The test creates a private desktop and puts its own window of class
//          Shell_TrayWnd on it, so FindWindowW inside both shell32 and the mod
//          resolves to the test's window rather than Explorer's. Everything -
//          the fake shell, the mod's tray window, the icon owner - lives on that
//          desktop and is destroyed with it. The user's own tray is untouched.
//
// Build/run: tools/build.ps1 (or tools/run-tests.ps1)
// ============================================================================

#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <set>
#include <string>
#include <utility>

#include "../../src/split-tray.wh.cpp"

using namespace SplitTray;

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool condition, const char* expression, int line) {
    g_checks++;
    if (!condition) {
        g_failures++;
        printf("  FAIL  line %d: %s\n", line, expression);
    }
}

#define CHECK(expr) Check((expr), #expr, __LINE__)
#define CHECK_EQ(a, b)                                                       \
    do {                                                                    \
        auto a_ = (a);                                                      \
        auto b_ = (b);                                                      \
        g_checks++;                                                         \
        if (!(a_ == b_)) {                                                  \
            g_failures++;                                                   \
            printf("  FAIL  line %d: %s == %s (got %lld, want %lld)\n",     \
                   __LINE__, #a, #b, static_cast<long long>(a_),            \
                   static_cast<long long>(b_));                             \
        }                                                                   \
    } while (0)

// --- The stand-in for Explorer ---------------------------------------------

// What the "shell" actually received, i.e. what the mod chose to forward.
struct ShellObservations {
    int adds = 0;
    int modifies = 0;
    int deletes = 0;
    int setVersions = 0;
    UINT lastUID = 0;
    std::wstring lastTip;

    // The last NIM_ADD in full, since an add is what recreates an icon: every
    // field it leaves out is a field the shell does not have.
    UINT addFlags = 0;
    UINT addCallback = 0;
    std::wstring addTip;
    std::wstring addExePath;
    bool addIconAlive = false;  // checked on arrival, before anything is freed
    UINT lastVersion = 0;

    void Reset() { *this = ShellObservations(); }
};

// Whether a handle is a live icon at this moment.
bool IconIsAlive(HICON icon) {
    ICONINFO info = {};
    if (!icon || !GetIconInfo(icon, &info)) {
        return false;
    }
    if (info.hbmColor) {
        DeleteObject(info.hbmColor);
    }
    if (info.hbmMask) {
        DeleteObject(info.hbmMask);
    }
    return true;
}

ShellObservations g_shell;
HWND g_fakeShellWnd = nullptr;
HWND g_iconOwnerWnd = nullptr;

// The icons the stand-in shell holds, by owner and uID, so it can answer
// Shell_NotifyIconGetRect the way Explorer does: for its own icons only.
std::set<std::pair<HWND, UINT>> g_shellHeld;

// Where the stand-in shell says its icons are.
constexpr RECT kShellIconRect = {1500, 1040, 1524, 1080};

// Set to make the stand-in refuse every add, the way Explorer refuses one it
// will not take for its own reasons.
bool g_shellRefusesAdds = false;

// Shell_NotifyIconGetRect's question, decoded with the offsets the wire probe
// captured (tests/probe/probe-rect-output-26100.txt) rather than the mod's own
// parser, so the stand-in does not agree with the mod by construction:
// dwData 3, 40 bytes, 0x04 = 1 for the position or 2 for the size, 0x10 the
// owner window, 0x14 the uID. The answer is packed like a mouse position, and
// a size of 0 means the icon was not found.
LRESULT AnswerIconRectQuery(const COPYDATASTRUCT* cds) {
    if (!cds || cds->dwData != 3 || cds->cbData < 0x18 || !cds->lpData) {
        return 0;
    }
    const BYTE* data = static_cast<const BYTE*>(cds->lpData);
    DWORD part = 0;
    DWORD owner = 0;
    UINT uID = 0;
    memcpy(&part, data + 0x04, sizeof(part));
    memcpy(&owner, data + 0x10, sizeof(owner));
    memcpy(&uID, data + 0x14, sizeof(uID));
    const HWND ownerWnd = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(owner));
    if (!g_shellHeld.count({ownerWnd, uID})) {
        return 0;
    }
    const RECT& r = kShellIconRect;
    if (part == 2) {
        return MAKELONG(r.right - r.left, r.bottom - r.top);
    }
    return MAKELONG(static_cast<WORD>(r.left), static_cast<WORD>(r.top));
}

// Asks where an icon is, the way Tauri's tray library does before it handles
// any click on it.
HRESULT AskWhereIconIs(UINT uID, RECT* rect) {
    NOTIFYICONIDENTIFIER id = {};
    id.cbSize = sizeof(id);
    id.hWnd = g_iconOwnerWnd;
    id.uID = uID;
    *rect = RECT{};
    return Shell_NotifyIconGetRect(&id, rect);
}

// Set when the icon owner receives its tray callback, which is how the click
// forwarding path is verified.
volatile LONG g_ownerClicks = 0;
volatile LONG g_ownerLastMouseMessage = 0;
constexpr UINT kOwnerCallbackMessage = WM_APP + 0x777;

// Every callback the owner received, in order: the message in the low word of
// lParam, which is where both protocol versions put it. The owner's window is
// on this thread, so only this thread touches it.
std::vector<UINT> g_ownerMessages;

LRESULT CALLBACK FakeShellProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COPYDATA) {
        auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (cds && cds->dwData == 3) {
            return AnswerIconRectQuery(cds);
        }
        TrayNotification n;
        if (cds && ParseTrayNotification(cds->dwData, cds->lpData, cds->cbData, &n)) {
            // Answered as Explorer answers: an add fails for an icon it has
            // already, and anything else fails for one it does not have. It
            // used to take everything, which could not tell the mod apart from
            // one that records a refusal as success.
            const std::pair<HWND, UINT> id = {n.ownerWnd, n.uID};
            const bool held = g_shellHeld.count(id) != 0;
            LRESULT answer = TRUE;
            if (n.message == NIM_ADD) {
                if (held || g_shellRefusesAdds) {
                    answer = FALSE;
                } else {
                    g_shellHeld.insert(id);
                }
            } else if (n.message == NIM_MODIFY || n.message == NIM_DELETE ||
                       n.message == NIM_SETVERSION) {
                if (!held) {
                    answer = FALSE;
                } else if (n.message == NIM_DELETE) {
                    g_shellHeld.erase(id);
                }
            }
            switch (n.message) {
                case NIM_ADD:
                    g_shell.adds++;
                    g_shell.addFlags = n.flags;
                    g_shell.addCallback = n.callbackMessage;
                    g_shell.addTip = (n.flags & NIF_TIP) ? n.tip : L"";
                    g_shell.addExePath = n.exePath;
                    g_shell.addIconAlive = (n.flags & NIF_ICON) && IconIsAlive(n.icon);
                    break;
                case NIM_MODIFY: g_shell.modifies++; break;
                case NIM_DELETE: g_shell.deletes++; break;
                case NIM_SETVERSION:
                    g_shell.setVersions++;
                    g_shell.lastVersion = n.version;
                    break;
                default: break;
            }
            g_shell.lastUID = n.uID;
            if (n.flags & NIF_TIP) {
                g_shell.lastTip = n.tip;
            }
            return answer;
        }
        return TRUE;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK IconOwnerProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == kOwnerCallbackMessage) {
        InterlockedIncrement(&g_ownerClicks);
        // Version 0 protocol: wParam is the uID, lParam the mouse message.
        InterlockedExchange(&g_ownerLastMouseMessage, static_cast<LONG>(lParam));
        g_ownerMessages.push_back(LOWORD(lParam));
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --- Pumping -------------------------------------------------------------

void Pump() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// Pumps until `predicate` holds or the budget runs out. Returns whether it held.
template <typename Predicate>
bool PumpUntil(Predicate predicate, DWORD timeoutMs = 3000) {
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        Pump();
        if (predicate()) {
            return true;
        }
        if (GetTickCount64() >= deadline) {
            return false;
        }
        Sleep(15);
    }
}

// --- Helpers over the mod's state ----------------------------------------

size_t MirroredCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_icons.size();
}

size_t TrackedPrimaryCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_primaryOnly.size();
}

std::wstring MirroredTip(size_t index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return index < g_icons.size() ? g_icons[index].tip : L"";
}

bool MirroredHasIcon(size_t index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return index < g_icons.size() && g_icons[index].icon != nullptr;
}

// Where the icon with this uID is drawn in tray `number`, counting that tray's
// cells from the first; -1 if that tray does not show it.
int IndexInTray(int number, UINT uID) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto inTray = IconsInTrayLocked(number);
    for (size_t i = 0; i < inTray.size(); i++) {
        const auto& icon = g_icons[inTray[i]];
        if (icon.ownerWnd == g_iconOwnerWnd && icon.uID == uID) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

HWND FloatingWindowOf(int number) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_floatingWnds.find(number);
    return found == g_floatingWnds.end() ? nullptr : found->second;
}

TrayLayout FloatingLayoutOf(int number) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_floatingLayouts.find(number);
    return found == g_floatingLayouts.end() ? TrayLayout{} : found->second;
}

TrayTarget TrayNumbered(int number) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const TrayTarget* tray = FindTrayLocked(number);
    return tray ? *tray : TrayTarget{};
}

int LastTrayNumber() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_trays.empty() ? 0 : g_trays.back().number;
}

bool WindowIsAt(HWND wnd, const TrayLayout& layout) {
    RECT rect = {};
    return wnd && GetWindowRect(wnd, &rect) && rect.left == layout.x &&
           rect.top == layout.y && rect.right - rect.left == layout.width &&
           rect.bottom - rect.top == layout.height;
}

// Whether the mod has logged a line containing `text` since `from` lines in.
bool LoggedSince(size_t from, const wchar_t* text) {
    const auto lines = SplitTrayTestHarness::LogSnapshot();
    for (size_t i = from; i < lines.size(); i++) {
        if (lines[i].find(text) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

constexpr const wchar_t* kWouldAsk = L"would be asked to re-register";
constexpr const wchar_t* kNotAsking = L"not asking applications to re-register";



// Sends a real tray notification, exactly as any application would.
BOOL SendTrayNotification(DWORD message, UINT uID, PCWSTR tip, UINT extraFlags = 0) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_iconOwnerWnd;
    nid.uID = uID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | extraFlags;
    nid.uCallbackMessage = kOwnerCallbackMessage;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (tip) {
        wcsncpy(nid.szTip, tip, 127);
    }
    return Shell_NotifyIconW(message, &nid);
}

// A NIM_MODIFY carrying only what `flags` names, the way most applications
// update a live icon: a new picture, or a new tooltip, never both at once.
BOOL SendPartialModify(UINT uID, UINT flags, HICON icon, PCWSTR tip) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_iconOwnerWnd;
    nid.uID = uID;
    nid.uFlags = flags;
    nid.hIcon = icon;
    if (tip) {
        wcsncpy(nid.szTip, tip, 127);
    }
    return Shell_NotifyIconW(NIM_MODIFY, &nid);
}

BOOL SendSetVersion(UINT uID, UINT version) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_iconOwnerWnd;
    nid.uID = uID;
    nid.uVersion = version;
    return Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

// An icon the test owns and may destroy, as an application's own icons are.
HICON MakeOwnedIcon(LPCWSTR stock) {
    return CopyIcon(LoadIconW(nullptr, stock));
}

// Whether floating tray `number` is laid out, and its window placed, for the
// icons it holds now. The store changes on this thread at once; the layout and
// the window follow on the mod's own thread, and a click computed in between
// lands on the old grid.
bool TrayLaidOutForItsIcons(int number) {
    const TrayTarget tray = TrayNumbered(number);
    TrayLayout expected;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        expected = ComputeLayout(tray.monitor.workArea,
                                 static_cast<int>(IconsInTrayLocked(number).size()),
                                 g_settings, tray.monitor.dpi, tray.corner);
    }
    const TrayLayout actual = FloatingLayoutOf(number);
    return actual.columns == expected.columns && actual.width == expected.width &&
           actual.height == expected.height &&
           WindowIsAt(FloatingWindowOf(number), actual);
}

// Posts a click on the cell of icon `uID` in floating tray `number`.
void ClickInTray(int number, UINT uID, UINT down, UINT up) {
    const int index = IndexInTray(number, uID);
    const TrayLayout layout = FloatingLayoutOf(number);
    if (index < 0 || layout.columns <= 0) {
        return;
    }
    const int column = index % layout.columns;
    const int row = index / layout.columns;
    const LPARAM point = MAKELPARAM(column * layout.cell + layout.cell / 2,
                                    row * layout.cell + layout.cell / 2);
    PostMessageW(FloatingWindowOf(number), down, 0, point);
    PostMessageW(FloatingWindowOf(number), up, 0, point);
}

// The list views in a window, and how many of them share their image lists.
struct ListViewCount {
    int lists = 0;
    int sharing = 0;
};

ListViewCount CountListViews(HWND parent) {
    ListViewCount count;
    EnumChildWindows(
        parent,
        [](HWND child, LPARAM param) -> BOOL {
            WCHAR className[64] = {};
            GetClassNameW(child, className, ARRAYSIZE(className));
            if (_wcsicmp(className, WC_LISTVIEWW) == 0) {
                auto* count = reinterpret_cast<ListViewCount*>(param);
                count->lists++;
                if (GetWindowLongPtrW(child, GWL_STYLE) & LVS_SHAREIMAGELISTS) {
                    count->sharing++;
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&count));
    return count;
}

// Whether a window class of that name is registered for this module.
bool ClassRegistered(PCWSTR name) {
    WNDCLASSEXW wc = {sizeof(wc)};
    return GetClassInfoExW(GetModuleHandleW(nullptr), name, &wc) != FALSE;
}

void SetSetting(PCWSTR name, PCWSTR value) {
    SplitTrayTestHarness::StringSettings()[name] = value;
}

void SetSetting(PCWSTR name, int value) {
    SplitTrayTestHarness::IntSettings()[name] = value;
}

std::wstring ThisExeName() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* slash = wcsrchr(path, L'\\');
    return slash ? slash + 1 : path;
}

void SeedBaselineSettings() {
    SplitTrayTestHarness::ResetSettings();
    SetSetting(L"defaultTray", L"primary");
    // Two extra trays on the primary display, so the test has at least two of
    // Split Tray's trays on any machine: with one display they are trays 2 and
    // 3, with two they are 3 and 4, after the second display's own.
    SetSetting(L"extraTrays[0].display", L"primary");
    SetSetting(L"extraTrays[0].corner", L"bottomLeft");
    SetSetting(L"extraTrays[1].display", L"primary");
    SetSetting(L"extraTrays[1].corner", L"topLeft");
    SetSetting(L"trayPosition", L"bottomRight");
    SetSetting(L"offsetX", 8);
    SetSetting(L"offsetY", 8);
    SetSetting(L"iconSize", 16);
    SetSetting(L"cellSize", 28);
    SetSetting(L"maxColumns", 12);
    SetSetting(L"backgroundColor", L"202020");
    SetSetting(L"opacity", 235);
    SetSetting(L"alwaysOnTop", 1);
    SetSetting(L"showTooltips", 1);
    SetSetting(L"mirrorHiddenIcons", 1);
    // Deliberately off: broadcasting TaskbarCreated would reach the user's real
    // applications and make them all re-register their icons.
    SetSetting(L"repopulateOnLoad", 0);
}

// --- The test ------------------------------------------------------------

int RunTests() {
    printf("split-tray integration test (private desktop, real shell32)\n\n");

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    // The stand-in for Explorer's tray window. Registering the class under the
    // real name is what makes shell32 and the mod both target it.
    WNDCLASSW shellClass = {};
    shellClass.lpfnWndProc = FakeShellProc;
    shellClass.hInstance = hInst;
    shellClass.lpszClassName = L"Shell_TrayWnd";
    if (!RegisterClassW(&shellClass)) {
        printf("  FATAL RegisterClassW(Shell_TrayWnd) failed: %lu\n", GetLastError());
        return 1;
    }
    g_fakeShellWnd = CreateWindowExW(0, L"Shell_TrayWnd", nullptr, 0, 0, 0, 16, 16,
                                    nullptr, nullptr, hInst, nullptr);
    CHECK(g_fakeShellWnd != nullptr);

    HWND found = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (found != g_fakeShellWnd) {
        printf("  FATAL this desktop is not isolated (FindWindowW found %p, ours is "
               "%p) - refusing to run against the real shell\n",
               found, g_fakeShellWnd);
        return 1;
    }
    printf("  isolated: Shell_TrayWnd resolves to the test's own window\n");

    WNDCLASSW ownerClass = {};
    ownerClass.lpfnWndProc = IconOwnerProc;
    ownerClass.hInstance = hInst;
    ownerClass.lpszClassName = L"SplitTrayIntegrationOwner";
    RegisterClassW(&ownerClass);
    g_iconOwnerWnd = CreateWindowExW(0, L"SplitTrayIntegrationOwner", L"owner", 0, 0,
                                    0, 0, 0, nullptr, nullptr, hInst, nullptr);
    CHECK(g_iconOwnerWnd != nullptr);

    // ---- load the mod -------------------------------------------------
    printf("\n[1] mod load\n");
    SeedBaselineSettings();
    const size_t logAtLoad = SplitTrayTestHarness::LogSnapshot().size();
    CHECK(Wh_ModInit() == TRUE);
    // Its tray thread is running before it attaches to anything: without it
    // nothing draws the mod's trays (DECISIONS 67).
    CHECK(g_trayWnd.load() != nullptr);
    Wh_ModAfterInit();
    CHECK_EQ(WindhawkUtils::SubclassCallCount() > 0, true);
    CHECK(g_shellTrayWnd.load() == g_fakeShellWnd);
    // Loaded into a taskbar that already existed: Explorer announced it before
    // the mod arrived, so collecting the icons already there is the mod's job.
    // Wh_ModInit attaches in this case, and the attachment used to be skipped
    // by the check that decides whether to ask.
    CHECK(LoggedSince(logAtLoad, kWouldAsk));

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& tray : g_trays) {
            printf("      tray %d: %ls\n", tray.number,
                   TrayLabelLocked(tray.number).c_str());
        }
    }
    CHECK(LastTrayNumber() >= 3);
    CHECK(PumpUntil([] { return g_trayWnd.load() != nullptr; }));
    // Every tray floats here (no XAML in this build), each with a window of its
    // own - even empty, since an empty one still shows a handle.
    CHECK(PumpUntil([] { return FloatingWindowOf(2) && FloatingWindowOf(3); }));
    printf("      tray windows created\n");

    // ---- primary routing: the shell must still get the icon ------------
    printf("\n[2] defaultTray=primary forwards to the shell and does not mirror\n");
    g_shell.Reset();
    CHECK(SendTrayNotification(NIM_ADD, 101, L"primary icon") == TRUE);
    Pump();
    CHECK_EQ(g_shell.adds, 1);
    CHECK_EQ(g_shell.lastUID, 101u);
    CHECK(g_shell.lastTip == L"primary icon");
    CHECK_EQ(static_cast<int>(MirroredCount()), 0);
    CHECK_EQ(static_cast<int>(TrackedPrimaryCount()), 1);

    // ---- secondary routing: the shell must NOT get the icon ------------
    printf("\n[3] a rule for this process sends the next icon to the secondary tray\n");
    {
        const std::wstring exe = ThisExeName();
        SetSetting(L"perProcessRouting[0].exe", exe.c_str());
        SetSetting(L"perProcessRouting[0].destination", L"secondary");
        printf("      rule: %ls -> secondary\n", exe.c_str());
    }
    Wh_ModSettingsChanged();
    // The settings change also moves the icon already on screen: the shell should
    // be told to delete it.
    CHECK(PumpUntil([] { return g_shell.deletes >= 1; }));
    CHECK_EQ(static_cast<int>(MirroredCount()), 1);
    CHECK_EQ(static_cast<int>(TrackedPrimaryCount()), 0);
    printf("      the existing icon moved trays live (shell deletes=%d)\n",
           g_shell.deletes);

    g_shell.Reset();
    CHECK(SendTrayNotification(NIM_ADD, 102, L"secondary icon") == TRUE);
    Pump();
    CHECK_EQ(g_shell.adds, 0);  // swallowed: this is the whole point of the mod
    CHECK_EQ(static_cast<int>(MirroredCount()), 2);

    // ---- the mirrored icon carries the real data ----------------------
    printf("\n[4] the mirrored icon has the real tooltip and a copied HICON\n");
    bool tipFound = false;
    for (size_t i = 0; i < MirroredCount(); i++) {
        if (MirroredTip(i) == L"secondary icon") {
            tipFound = true;
            CHECK(MirroredHasIcon(i));
        }
    }
    CHECK(tipFound);

    // ---- geometry ----------------------------------------------------
    printf("\n[5] tray 2's window sits inside its monitor at the right size\n");
    const TrayTarget second = TrayNumbered(2);
    const RECT monitorWork = second.monitor.workArea;
    const TrayLayout expected = [&] {
        std::lock_guard<std::mutex> lock(g_mutex);
        return ComputeLayout(monitorWork,
                             static_cast<int>(IconsInTrayLocked(2).size()), g_settings,
                             second.monitor.dpi, second.corner);
    }();
    CHECK_EQ(expected.columns, 2);  // both of this process's icons
    HWND trayWnd = FloatingWindowOf(2);
    RECT actual = {};
    CHECK(PumpUntil([&] {
        GetWindowRect(trayWnd, &actual);
        return actual.left == expected.x && actual.top == expected.y &&
               (actual.right - actual.left) == expected.width;
    }));
    printf("      window at (%ld,%ld) %ldx%ld; monitor work area (%ld,%ld)-(%ld,%ld)\n",
           actual.left, actual.top, actual.right - actual.left,
           actual.bottom - actual.top, monitorWork.left, monitorWork.top,
           monitorWork.right, monitorWork.bottom);
    CHECK(actual.left >= monitorWork.left);
    CHECK(actual.top >= monitorWork.top);
    CHECK(actual.right <= monitorWork.right);
    CHECK(actual.bottom <= monitorWork.bottom);
    CHECK(IsWindowVisible(trayWnd));

    // ---- clicking the mirror reaches the application ------------------
    printf("\n[6] clicking a mirrored icon reaches the owning application\n");
    InterlockedExchange(&g_ownerClicks, 0);
    {
        const TrayLayout layout = FloatingLayoutOf(2);
        const int centre = layout.cell / 2;
        const LPARAM point = MAKELPARAM(centre, centre);
        PostMessageW(trayWnd, WM_LBUTTONDOWN, 0, point);
        PostMessageW(trayWnd, WM_LBUTTONUP, 0, point);
    }
    CHECK(PumpUntil([] { return InterlockedCompareExchange(&g_ownerClicks, 0, 0) >= 2; }));
    printf("      owner received %ld callback messages, last mouse message 0x%lX\n",
           InterlockedCompareExchange(&g_ownerClicks, 0, 0),
           InterlockedCompareExchange(&g_ownerLastMouseMessage, 0, 0));
    CHECK_EQ(InterlockedCompareExchange(&g_ownerLastMouseMessage, 0, 0),
             static_cast<LONG>(WM_LBUTTONUP));

    // ---- the application can find its icon ---------------------------
    // From a live report: Telemachus stopped responding to clicks once its
    // icon was in the secondary tray - no menu, no window - and came back when
    // it was moved back. Tauri's tray library asks Shell_NotifyIconGetRect
    // where the icon is before it handles any click, and drops the click when
    // that fails. The shell does not have an icon that lives in the secondary
    // tray, so it cannot say; the mod has to.
    printf("\n[7] an application can find where its icon is\n");
    {
        RECT rect;
        const HRESULT hr = AskWhereIconIs(102, &rect);
        CHECK_EQ(hr, S_OK);

        // Where the mod drew it: its cell in the mod's own window.
        const int index = IndexInTray(2, 102);
        const TrayLayout layout = FloatingLayoutOf(2);
        RECT window = {};
        GetWindowRect(trayWnd, &window);
        CHECK(index >= 0 && layout.columns > 0);
        if (index >= 0 && layout.columns > 0) {
            const int column = index % layout.columns;
            const int row = index / layout.columns;
            CHECK_EQ(rect.left, window.left + column * layout.cell);
            CHECK_EQ(rect.top, window.top + row * layout.cell);
            CHECK_EQ(rect.right - rect.left, layout.cell);
            CHECK_EQ(rect.bottom - rect.top, layout.cell);
        }
        printf("      secondary icon: hr=0x%08lX at (%ld,%ld)-(%ld,%ld); tray window "
               "at (%ld,%ld)\n",
               static_cast<unsigned long>(hr), rect.left, rect.top, rect.right,
               rect.bottom, window.left, window.top);

        // An icon in the primary tray is the shell's to answer for.
        const std::wstring key = ThisExeName() + L"#101";
        MoveIconToTray(key, Destination::Primary);
        CHECK(PumpUntil([] { return g_shellHeld.count({g_iconOwnerWnd, 101u}) > 0; }));
        CHECK_EQ(AskWhereIconIs(101, &rect), S_OK);
        CHECK_EQ(rect.left, kShellIconRect.left);
        CHECK_EQ(rect.top, kShellIconRect.top);
        CHECK_EQ(rect.right, kShellIconRect.right);
        CHECK_EQ(rect.bottom, kShellIconRect.bottom);
        MoveIconToTray(key, Destination::Secondary);
        CHECK(PumpUntil([] { return g_shellHeld.count({g_iconOwnerWnd, 101u}) == 0; }));

        // And an icon nobody has is still not found.
        CHECK_EQ(AskWhereIconIs(999, &rect), E_FAIL);
    }

    // ---- a third tray ------------------------------------------------
    // More than one of Split Tray's trays: here an extra tray at the top left
    // of the primary display, which is how a machine with two displays gets a
    // third tray to test with.
    printf("\n[8] a third tray, elsewhere on the primary display\n");
    {
        const int third = LastTrayNumber();
        const TrayTarget target = TrayNumbered(third);
        CHECK(target.available);
        CHECK(target.monitor.primary);
        CHECK(!target.forDisplay);
        CHECK(target.corner == Corner::TopLeft);

        HWND thirdWnd = FloatingWindowOf(third);
        CHECK(thirdWnd != nullptr);
        CHECK(IsWindowVisible(thirdWnd));
        CHECK(PumpUntil([&] { return WindowIsAt(thirdWnd, FloatingLayoutOf(third)); }));
        RECT rect = {};
        GetWindowRect(thirdWnd, &rect);
        CHECK(rect.left >= target.monitor.workArea.left);
        CHECK(rect.top >= target.monitor.workArea.top);
        CHECK(rect.left < (target.monitor.workArea.left + target.monitor.workArea.right) / 2);
        CHECK(rect.top < (target.monitor.workArea.top + target.monitor.workArea.bottom) / 2);
        printf("      tray %d empty at (%ld,%ld): a handle, %ldx%ld\n", third, rect.left,
               rect.top, rect.right - rect.left, rect.bottom - rect.top);

        // An icon moves into it without Explorer hearing a thing: both trays
        // are Split Tray's.
        g_shell.Reset();
        const std::wstring key = ThisExeName() + L"#102";
        MoveIconToTray(key, Destination::Tray(third));
        CHECK_EQ(IndexInTray(third, 102), 0);
        CHECK_EQ(IndexInTray(2, 102), -1);
        Pump();
        CHECK_EQ(g_shell.adds, 0);
        CHECK_EQ(g_shell.deletes, 0);
        CHECK(PumpUntil([&] { return WindowIsAt(thirdWnd, FloatingLayoutOf(third)); }));

        // A click on it there reaches its application.
        InterlockedExchange(&g_ownerClicks, 0);
        const TrayLayout layout = FloatingLayoutOf(third);
        const LPARAM point = MAKELPARAM(layout.cell / 2, layout.cell / 2);
        PostMessageW(thirdWnd, WM_LBUTTONDOWN, 0, point);
        PostMessageW(thirdWnd, WM_LBUTTONUP, 0, point);
        CHECK(PumpUntil(
            [] { return InterlockedCompareExchange(&g_ownerClicks, 0, 0) >= 2; }));

        // And its application can find it there.
        RECT where = {};
        CHECK_EQ(AskWhereIconIs(102, &where), S_OK);
        GetWindowRect(thirdWnd, &rect);
        CHECK_EQ(where.left, rect.left);
        CHECK_EQ(where.top, rect.top);
        CHECK_EQ(where.right - where.left, layout.cell);
        printf("      icon 102 in tray %d at (%ld,%ld)-(%ld,%ld), clicks delivered\n",
               third, where.left, where.top, where.right, where.bottom);

        // Disabled, the tray's window goes and its icon waits in the shell's
        // tray; enabled again, the icon goes back where it was put.
        g_shell.Reset();
        SetSetting(L"extraTrays[1].disabled", 1);
        Wh_ModSettingsChanged();
        CHECK(PumpUntil([&] { return FloatingWindowOf(third) == nullptr; }));
        CHECK(!TrayNumbered(third).available);
        CHECK_EQ(LastTrayNumber(), third);  // it keeps its number
        CHECK(PumpUntil([] { return g_shell.adds >= 1; }));
        CHECK_EQ(IndexInTray(third, 102), -1);
        printf("      tray %d disabled: no window, icon 102 back with the shell "
               "(adds=%d)\n", third, g_shell.adds);

        g_shell.Reset();
        SetSetting(L"extraTrays[1].disabled", 0);
        Wh_ModSettingsChanged();
        CHECK(PumpUntil([&] { return FloatingWindowOf(third) != nullptr; }));
        CHECK(PumpUntil([] { return g_shell.deletes >= 1; }));
        CHECK_EQ(IndexInTray(third, 102), 0);
        printf("      tray %d enabled again: icon 102 is back in it (deletes=%d)\n",
               third, g_shell.deletes);

        // Back to tray 2, still without the shell.
        g_shell.Reset();
        MoveIconToTray(key, Destination::Secondary);
        CHECK(IndexInTray(2, 102) >= 0);
        Pump();
        CHECK_EQ(g_shell.adds, 0);
        CHECK_EQ(g_shell.deletes, 0);
    }

    // ---- a version 4 click is the whole click ------------------------
    // Explorer follows a left button-up with NIN_SELECT and a right one with
    // WM_CONTEXTMENU for icons at version 3 and up, and applications written
    // to the version 4 protocol - Microsoft's own sample among them - act on
    // those rather than on the raw buttons.
    printf("\n[8b] a version 4 icon is clicked the way Explorer clicks it\n");
    {
        constexpr UINT kV4 = 107;
        CHECK(SendTrayNotification(NIM_ADD, kV4, L"version 4") == TRUE);
        CHECK(SendSetVersion(kV4, NOTIFYICON_VERSION_4) == TRUE);
        Pump();
        CHECK(IndexInTray(2, kV4) >= 0);
        CHECK(PumpUntil([] { return TrayLaidOutForItsIcons(2); }));

        g_ownerMessages.clear();
        ClickInTray(2, kV4, WM_LBUTTONDOWN, WM_LBUTTONUP);
        CHECK(PumpUntil([] { return g_ownerMessages.size() >= 3; }));
        Pump();
        CHECK_EQ(g_ownerMessages.size(), static_cast<size_t>(3));
        if (g_ownerMessages.size() == 3) {
            CHECK_EQ(g_ownerMessages[0], static_cast<UINT>(WM_LBUTTONDOWN));
            CHECK_EQ(g_ownerMessages[1], static_cast<UINT>(WM_LBUTTONUP));
            CHECK_EQ(g_ownerMessages[2], static_cast<UINT>(NIN_SELECT));
        }

        g_ownerMessages.clear();
        ClickInTray(2, kV4, WM_RBUTTONDOWN, WM_RBUTTONUP);
        CHECK(PumpUntil([] { return g_ownerMessages.size() >= 3; }));
        Pump();
        CHECK_EQ(g_ownerMessages.size(), static_cast<size_t>(3));
        if (g_ownerMessages.size() == 3) {
            CHECK_EQ(g_ownerMessages[2], static_cast<UINT>(WM_CONTEXTMENU));
        }
        printf("      left click: button down, button up, NIN_SELECT; right click ends "
               "with WM_CONTEXTMENU\n");

        SendTrayNotification(NIM_DELETE, kV4, nullptr);
        Pump();
    }

    // ---- modify and delete ------------------------------------------
    printf("\n[9] modify updates the mirror, delete removes it\n");
    g_shell.Reset();
    CHECK(SendTrayNotification(NIM_MODIFY, 102, L"renamed") == TRUE);
    Pump();
    CHECK_EQ(g_shell.modifies, 0);  // still swallowed
    bool renamed = false;
    for (size_t i = 0; i < MirroredCount(); i++) {
        if (MirroredTip(i) == L"renamed") {
            renamed = true;
        }
    }
    CHECK(renamed);

    const size_t before = MirroredCount();
    CHECK(SendTrayNotification(NIM_DELETE, 102, nullptr) == TRUE);
    Pump();
    CHECK_EQ(static_cast<int>(MirroredCount()), static_cast<int>(before) - 1);
    CHECK_EQ(g_shell.deletes, 0);  // the shell never had it, so it is not told

    // ---- an application that removes its icon while a move is on its way --
    // Applications send their messages; a move waits for the taskbar's
    // thread. So an application's NIM_DELETE is handled before a move into the
    // primary tray
    // that was asked for first, and delivering that move afterwards put back an
    // icon its application had already removed.
    printf("\n[9b] an icon removed while its move to the primary tray waits\n");
    {
        constexpr UINT kLeaving = 106;
        CHECK(SendTrayNotification(NIM_ADD, kLeaving, L"leaving") == TRUE);
        Pump();
        CHECK(IndexInTray(2, kLeaving) >= 0);

        g_shell.Reset();
        MoveIconToTray(ThisExeName() + L"#" + std::to_wstring(kLeaving),
                       Destination::Primary);
        // Sent from this thread, which owns the stand-in Shell_TrayWnd: handled at
        // once, ahead of anything queued for it.
        CHECK(SendTrayNotification(NIM_DELETE, kLeaving, nullptr) == TRUE);
        Pump();
        Sleep(50);
        Pump();
        CHECK_EQ(g_shell.adds, 0);
        CHECK_EQ(g_shell.deletes, 0);  // the shell never had it
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kLeaving}) == 0);
        printf("      shell adds=%d deletes=%d after the application removed it\n",
               g_shell.adds, g_shell.deletes);
    }

    // ---- a partial modify must not wipe fields ----------------------
    printf("\n[10] a modify carrying only NIF_ICON keeps the tooltip\n");
    CHECK(SendTrayNotification(NIM_ADD, 103, L"keep me") == TRUE);
    Pump();
    {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = g_iconOwnerWnd;
        nid.uID = 103;
        nid.uFlags = NIF_ICON;  // no NIF_TIP
        nid.hIcon = LoadIconW(nullptr, IDI_WARNING);
        CHECK(Shell_NotifyIconW(NIM_MODIFY, &nid) == TRUE);
    }
    Pump();
    bool keptTip = false;
    for (size_t i = 0; i < MirroredCount(); i++) {
        if (MirroredTip(i) == L"keep me") {
            keptTip = true;
        }
    }
    CHECK(keptTip);

    // ---- an icon moved away and back comes back whole ----------------
    // From a live report: the Claude usage monitor vanished after being moved
    // to the secondary tray and back. It adds its icon once, then changes the
    // picture and the tooltip in separate partial modifies, and destroys each
    // picture it replaces. Putting it back in the shell has to recreate all of
    // that - not replay whichever partial message happened to arrive last.
    printf("\n[11] an icon moved to the secondary tray and back returns whole\n");
    {
        constexpr UINT kMoved = 105;
        HICON first = MakeOwnedIcon(IDI_APPLICATION);
        {
            NOTIFYICONDATAW nid = {};
            nid.cbSize = sizeof(nid);
            nid.hWnd = g_iconOwnerWnd;
            nid.uID = kMoved;
            nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            nid.uCallbackMessage = kOwnerCallbackMessage;
            nid.hIcon = first;
            wcsncpy(nid.szTip, L"usage 9%", 127);
            CHECK(Shell_NotifyIconW(NIM_ADD, &nid) == TRUE);
        }
        Pump();

        // The rule sends this process to the secondary tray, so the shell has
        // never seen the icon - and must not be asked to version it either.
        g_shell.Reset();
        CHECK(SendSetVersion(kMoved, NOTIFYICON_VERSION_4) == TRUE);
        Pump();
        CHECK_EQ(g_shell.setVersions, 0);

        const std::wstring key = ThisExeName() + L"#" + std::to_wstring(kMoved);

        // Out to the primary tray.
        g_shell.Reset();
        MoveIconToTray(key, Destination::Primary);
        CHECK(PumpUntil([] { return g_shell.adds >= 1; }));
        CHECK((g_shell.addFlags & NIF_MESSAGE) != 0);
        CHECK(g_shell.addIconAlive);
        CHECK(PumpUntil([] { return g_shell.setVersions >= 1; }));
        CHECK_EQ(g_shell.lastVersion, static_cast<UINT>(NOTIFYICON_VERSION_4));

        // Updated while it is there, the way the usage monitor updates.
        HICON second = MakeOwnedIcon(IDI_WARNING);
        CHECK(SendPartialModify(kMoved, NIF_ICON, second, nullptr) == TRUE);
        DestroyIcon(first);
        CHECK(SendPartialModify(kMoved, NIF_TIP, nullptr, L"usage 12%") == TRUE);
        Pump();

        // Into the secondary tray...
        g_shell.Reset();
        MoveIconToTray(key, Destination::Secondary);
        CHECK(PumpUntil([] { return g_shell.deletes >= 1; }));

        // ...updated while it is away, by an application that releases its
        // picture as soon as the shell has been handed it...
        HICON third = MakeOwnedIcon(IDI_INFORMATION);
        CHECK(SendPartialModify(kMoved, NIF_ICON, third, nullptr) == TRUE);
        DestroyIcon(third);
        DestroyIcon(second);
        Pump();

        // ...and back. The shell has to be given the icon as it now stands.
        g_shell.Reset();
        MoveIconToTray(key, Destination::Primary);
        CHECK(PumpUntil([] { return g_shell.adds >= 1; }));
        printf("      re-added with flags 0x%X, callback 0x%X, tip '%ls', icon %s\n",
               g_shell.addFlags, g_shell.addCallback, g_shell.addTip.c_str(),
               g_shell.addIconAlive ? "alive" : "DEAD");
        CHECK((g_shell.addFlags & NIF_MESSAGE) != 0);
        CHECK((g_shell.addFlags & NIF_ICON) != 0);
        CHECK((g_shell.addFlags & NIF_TIP) != 0);
        CHECK_EQ(g_shell.addCallback, kOwnerCallbackMessage);
        CHECK(g_shell.addTip == L"usage 12%");
        CHECK(g_shell.addIconAlive);
        // Windows keeps an icon's placement - on the taskbar or in the
        // overflow - against the executable, so an add without one is a new,
        // unknown icon.
        CHECK(!g_shell.addExePath.empty());
        CHECK(PumpUntil([] { return g_shell.setVersions >= 1; }));
        CHECK_EQ(g_shell.lastVersion, static_cast<UINT>(NOTIFYICON_VERSION_4));
        CHECK_EQ(g_shell.deletes, 0);

        SendTrayNotification(NIM_DELETE, kMoved, nullptr);
        Pump();
    }

    // ---- the arrange window ------------------------------------------
    // One image list is shared by every list in it, so each list has to be
    // told not to destroy it; and its window class belongs to the mod, so it
    // has to go when the mod does (checked in [12]).
    printf("\n[11b] the arrange window opens, closes and opens again\n");
    {
        HWND modWnd = g_trayWnd.load();
        CHECK(modWnd != nullptr);
        PostMessageW(modWnd, WM_ST_ARRANGE, 0, 0);
        // Shown last, once its lists are made: the window exists a moment
        // before they do, on the mod's own thread.
        HWND arrange = nullptr;
        CHECK(PumpUntil([&] {
            arrange = FindWindowW(kArrangeClassName, nullptr);
            return arrange != nullptr && IsWindowVisible(arrange);
        }));
        const ListViewCount count = CountListViews(arrange);
        CHECK(count.lists >= 2);
        CHECK_EQ(count.sharing, count.lists);
        printf("      %d list(s), %d sharing the image list\n", count.lists,
               count.sharing);

        PostMessageW(arrange, WM_CLOSE, 0, 0);
        CHECK(PumpUntil([] { return FindWindowW(kArrangeClassName, nullptr) == nullptr; }));
        PostMessageW(modWnd, WM_ST_ARRANGE, 0, 0);
        CHECK(PumpUntil([] {
            HWND reopened = FindWindowW(kArrangeClassName, nullptr);
            return reopened != nullptr && IsWindowVisible(reopened);
        }));
        // Left open: unloading has to take it down.
    }

    // ---- what arrives while a move waits goes with it --------------------
    // The move into the main tray was queued as a finished record, so an
    // update that came before the taskbar's thread got to it - swallowed,
    // since Explorer did not have the icon yet - was missing from the add.
    printf("\n[11c] an update that arrives while a move waits is in the add\n");
    {
        constexpr UINT kWaiting = 107;
        CHECK(SendTrayNotification(NIM_ADD, kWaiting, L"before") == TRUE);
        Pump();
        const std::wstring key = ThisExeName() + L"#" + std::to_wstring(kWaiting);

        g_shell.Reset();
        MoveIconToTray(key, Destination::Primary);
        // Sent from this thread, which owns the stand-in: handled at once,
        // ahead of the wake-up posted for the move.
        CHECK(SendPartialModify(kWaiting, NIF_TIP, nullptr, L"after") == TRUE);
        CHECK(PumpUntil([] { return g_shell.adds >= 1; }));
        CHECK(g_shell.addTip == L"after");
        printf("      added with tip '%ls'\n", g_shell.addTip.c_str());

        SendTrayNotification(NIM_DELETE, kWaiting, nullptr);
        Pump();
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kWaiting}) == 0);
    }

    // ---- an icon Explorer will not take back -------------------------
    // A refused add was recorded as taken: the mod believed Explorer had the
    // icon, never asked again, and the icon was in neither tray.
    printf("\n[11d] an icon Explorer will not take back is left to its application\n");
    {
        constexpr UINT kRefused = 108;
        CHECK(SendTrayNotification(NIM_ADD, kRefused, L"refused") == TRUE);
        Pump();
        const std::wstring key = ThisExeName() + L"#" + std::to_wstring(kRefused);

        g_shell.Reset();
        g_shellRefusesAdds = true;
        MoveIconToTray(key, Destination::Primary);
        // Asked again on later wake-ups - the tray thread's timer makes them;
        // these are sooner - and no more than kShellAttempts times.
        for (int i = 0; i < kShellAttempts + 2; i++) {
            Pump();
            PostMessageW(g_fakeShellWnd, GetReplayMessage(), 0, 0);
            Pump();
        }
        CHECK_EQ(g_shell.adds, kShellAttempts);
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kRefused}) == 0);
        printf("      asked %d time(s), then left to the application\n", g_shell.adds);

        // Its application's own messages go to Explorer now, as with no mod:
        // an update is refused, and the application adds its icon again.
        g_shellRefusesAdds = false;
        CHECK(SendPartialModify(kRefused, NIF_TIP, nullptr, L"still here") == FALSE);
        CHECK(SendTrayNotification(NIM_ADD, kRefused, L"added again") == TRUE);
        Pump();
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kRefused}) == 1);

        // Recorded as Explorer's: moving it away takes it out.
        g_shell.Reset();
        MoveIconToTray(key, Destination::Secondary);
        CHECK(PumpUntil([] { return g_shell.deletes >= 1; }));
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kRefused}) == 0);

        SendTrayNotification(NIM_DELETE, kRefused, nullptr);
        Pump();
    }

    // ---- unload restores swallowed icons ---------------------------
    printf("\n[12] unloading returns swallowed icons to the shell\n");
    const int swallowed = static_cast<int>(MirroredCount());
    printf("      %d icon(s) currently only in the secondary tray\n", swallowed);
    g_shell.Reset();
    Wh_ModBeforeUninit();
    Wh_ModUninit();
    Pump();
    CHECK_EQ(g_shell.adds, swallowed);
    CHECK_EQ(static_cast<int>(MirroredCount()), 0);
    CHECK(g_trayWnd.load() == nullptr);
    CHECK(WindhawkUtils::UnsubclassCallCount() > 0);
    printf("      shell received %d restoring NIM_ADD(s), tray window gone\n",
           g_shell.adds);
    // A window class outlives the module that registered it unless it is
    // unregistered, and then points at a window procedure that is gone: the
    // next load's arrange window would have been created with it.
    CHECK(FindWindowW(kArrangeClassName, nullptr) == nullptr);
    CHECK(!ClassRegistered(kArrangeClassName));
    CHECK(!ClassRegistered(kFloatingClassName));
    CHECK(!ClassRegistered(kControllerClassName));

    // ---- the subclass really is gone -------------------------------
    printf("\n[13] after unload the shell sees traffic directly again\n");
    g_shell.Reset();
    CHECK(SendTrayNotification(NIM_ADD, 104, L"after unload") == TRUE);
    Pump();
    CHECK_EQ(g_shell.adds, 1);
    CHECK(g_shell.lastTip == L"after unload");
    SendTrayNotification(NIM_DELETE, 104, nullptr);

    // ---- loaded into an Explorer that already has the icons -------------
    // Found live, switching the mod off and on in Windhawk. Installing,
    // updating and switching it on all load it into a running Explorer, which
    // already holds icons - including every one an earlier load put back as it
    // unloaded. One its application registers again was swallowed and stayed
    // in the main tray as well; one whose application only ever updates it
    // carried no path, so no rule could place it.
    printf("\n[13b] loaded into an Explorer that already has the icons\n");
    {
        constexpr UINT kKnown = 401;       // its application registers it again
        constexpr UINT kUpdatesOnly = 402; // its application only updates it
        CHECK(SendTrayNotification(NIM_ADD, kKnown, L"known to Explorer") == TRUE);
        CHECK(SendTrayNotification(NIM_ADD, kUpdatesOnly, L"updates only") == TRUE);
        Pump();
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kKnown}) == 1);
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kUpdatesOnly}) == 1);

        SeedBaselineSettings();
        SetSetting(L"perProcessRouting[0].exe", ThisExeName().c_str());
        SetSetting(L"perProcessRouting[0].destination", L"secondary");
        CHECK(Wh_ModInit() == TRUE);
        Wh_ModAfterInit();
        CHECK(PumpUntil([] { return g_trayWnd.load() != nullptr; }));

        CHECK(SendTrayNotification(NIM_ADD, kKnown, L"known to Explorer") == TRUE);
        // A modify for an icon the mod has never seen added fails, as it would
        // with an Explorer that did not have the icon...
        CHECK(SendPartialModify(kUpdatesOnly, NIF_TIP, nullptr, L"updated") == FALSE);
        Pump();
        CHECK(IndexInTray(2, kKnown) >= 0);
        CHECK(IndexInTray(2, kUpdatesOnly) >= 0);
        // Taken out of the main tray, not left in both.
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kKnown}) == 0);
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kUpdatesOnly}) == 0);

        // ...so an application that recovers adds it again, whole: with the
        // callback a click in tray 2 is sent with, which a modify does not
        // carry. Taken, and kept out of the main tray.
        CHECK(SendTrayNotification(NIM_ADD, kUpdatesOnly, L"added again") == TRUE);
        Pump();
        CHECK(IndexInTray(2, kUpdatesOnly) >= 0);
        CHECK(g_shellHeld.count({g_iconOwnerWnd, kUpdatesOnly}) == 0);
        const int index = IndexInTray(2, kUpdatesOnly);  // takes the lock itself
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            const auto inTray = IconsInTrayLocked(2);
            CHECK(index >= 0 && static_cast<size_t>(index) < inTray.size() &&
                  g_icons[inTray[static_cast<size_t>(index)]].callbackMessage ==
                      kOwnerCallbackMessage);
        }
        printf("      both in tray 2 and gone from the shell's tray; the one that "
               "only updated was added again, whole\n");

        SendTrayNotification(NIM_DELETE, kKnown, nullptr);
        SendTrayNotification(NIM_DELETE, kUpdatesOnly, nullptr);
        Wh_ModBeforeUninit();
        Wh_ModUninit();
        Pump();
    }

    // ---- the tray window does not exist yet at load time -------------
    // Windhawk injects into explorer.exe before the shell has created its
    // taskbar, so on a cold Explorer start there is no Shell_TrayWnd to attach to
    // when Wh_ModInit runs, and the mod has to keep looking. This reproduces
    // that: the mod is loaded with no tray window in existence at all, and the
    // window appears afterwards. Getting this wrong makes the mod completely
    // inert on every real boot while looking healthy in its log.
    printf("\n[14] mod loaded before any tray window exists\n");
    // This phase depends on the mod noticing something on a timer, so show its log
    // rather than leaving a failure here to be guessed at.
    SplitTrayTestHarness::LogToStdout() = true;
    DestroyWindow(g_fakeShellWnd);
    g_fakeShellWnd = nullptr;
    Pump();
    CHECK(FindWindowW(L"Shell_TrayWnd", nullptr) == nullptr);

    SeedBaselineSettings();
    {
        const std::wstring exe = ThisExeName();
        SetSetting(L"perProcessRouting[0].exe", exe.c_str());
        SetSetting(L"perProcessRouting[0].destination", L"secondary");
    }
    const size_t logAtColdStart = SplitTrayTestHarness::LogSnapshot().size();
    CHECK(Wh_ModInit() == TRUE);
    Wh_ModAfterInit();
    CHECK(g_shellTrayWnd.load() == nullptr);  // nothing to attach to yet
    printf("      loaded with no tray window: not attached, as expected\n");

    // The taskbar appears.
    g_fakeShellWnd = CreateWindowExW(0, L"Shell_TrayWnd", nullptr, 0, 0, 0, 16, 16,
                                    nullptr, nullptr, hInst, nullptr);
    CHECK(g_fakeShellWnd != nullptr);

    // The tray thread's timer must notice within a couple of ticks.
    CHECK(PumpUntil([] { return g_shellTrayWnd.load() != nullptr; }, 8000));
    CHECK(g_shellTrayWnd.load() == g_fakeShellWnd);
    printf("      attached to the tray window after it appeared\n");

    // This is how Explorer starts: the taskbar did not exist when the mod
    // loaded, so Explorer will announce it once its tray is ready. Asking as
    // well made every application register twice, the first time into a tray
    // that dropped it - which is how Desk Tray's WhatsApp icon went missing.
    CHECK(PumpUntil([&] { return LoggedSince(logAtColdStart, kNotAsking); }));
    CHECK(!LoggedSince(logAtColdStart, kWouldAsk));

    // And interception really works on the window it found late.
    g_shell.Reset();
    CHECK(SendTrayNotification(NIM_ADD, 201, L"late attach") == TRUE);
    Pump();
    CHECK_EQ(g_shell.adds, 0);  // swallowed, so the subclass is live
    CHECK_EQ(static_cast<int>(MirroredCount()), 1);
    printf("      notifications on the new window are intercepted\n");

    // ---- a taskbar Explorer announced while the mod was not watching ----
    // Explorer recreates its taskbar on some display and theme changes and
    // announces the new one itself. If that announcement goes out before the
    // mod has found the new window, every application has re-registered where
    // the mod could not see it, and it has to ask again.
    printf("\n[15] a taskbar announced before the mod was watching it\n");
    {
        const size_t logAtRecreate = SplitTrayTestHarness::LogSnapshot().size();
        DestroyWindow(g_fakeShellWnd);
        g_fakeShellWnd = nullptr;
        Pump();

        HWND modWnd = g_trayWnd.load();
        CHECK(modWnd != nullptr);
        if (modWnd) {
            // Explorer's announcement, reaching the mod's own top-level window.
            SendMessageW(modWnd, TaskbarCreatedMessage(), 0, 0);
        }
        CHECK(LoggedSince(logAtRecreate, L"before the mod was watching it"));

        g_fakeShellWnd = CreateWindowExW(0, L"Shell_TrayWnd", nullptr, 0, 0, 0, 16,
                                        16, nullptr, nullptr, hInst, nullptr);
        CHECK(PumpUntil([] { return g_shellTrayWnd.load() == g_fakeShellWnd; }, 8000));
        CHECK(PumpUntil([&] { return LoggedSince(logAtRecreate, kWouldAsk); }));
        printf("      re-attached, and asked for the icons it missed\n");

        // Heard while watching, the same announcement changes nothing.
        const size_t logWhileWatching = SplitTrayTestHarness::LogSnapshot().size();
        SendMessageW(modWnd, TaskbarCreatedMessage(), 0, 0);
        Pump();
        CHECK(!LoggedSince(logWhileWatching, L"before the mod was watching it"));
        CHECK(!LoggedSince(logWhileWatching, kWouldAsk));
    }

    Wh_ModBeforeUninit();
    Wh_ModUninit();
    Pump();

    // ---- unloaded straight after loading ----------------------------
    // The shutdown was posted to the tray thread's window only if that window
    // already existed. Unloading while the thread was still creating it lost
    // the shutdown; unloading then waited, gave up, and carried on with the
    // thread still running code that was about to be unloaded.
    printf("\n[16] unloaded straight after loading\n");
    {
        const size_t logAtQuick = SplitTrayTestHarness::LogSnapshot().size();
        // Not waiting for the tray thread, as Wh_ModInit does not for one
        // slower than its budget: the unload then comes before the thread's
        // window exists, and the shutdown has to be sent once it does.
        g_trayThreadStartWaitMs = 0;
        CHECK(Wh_ModInit() == TRUE);
        g_trayThreadStartWaitMs = 5000;
        Wh_ModBeforeUninit();
        // Stopped before Windhawk takes the hooks out, which it does between
        // the two: the thread installs hooks of its own (DECISIONS 67).
        CHECK(g_trayThread == nullptr);
        Wh_ModUninit();
        CHECK(!LoggedSince(logAtQuick, L"did not"));
        CHECK(g_trayThread == nullptr);
        Pump();
        Sleep(300);
        Pump();
        CHECK(FindWindowW(kControllerClassName, nullptr) == nullptr);
        printf("      the tray thread stopped before unloading finished\n");
    }

    // ---- a replay message from outside the mod -------------------------
    // The message is registered by name, so any process on the desktop can
    // post it. It used to carry a pointer that the subclass delivered and
    // deleted; now it only says "look at the icon store".
    printf("\n[17] a replay message from outside the mod carries nothing it trusts\n");
    {
        CHECK(Wh_ModInit() == TRUE);
        Wh_ModAfterInit();
        CHECK(PumpUntil([] { return g_trayWnd.load() != nullptr; }));
        PostMessageW(g_fakeShellWnd, GetReplayMessage(), 0, static_cast<LPARAM>(0x10));
        Pump();
        CHECK(IsWindow(g_fakeShellWnd));
        printf("      a stray replay message with a bogus lParam was ignored\n");
        Wh_ModBeforeUninit();
        Wh_ModUninit();
        Pump();
    }

    DestroyWindow(g_iconOwnerWnd);
    if (g_fakeShellWnd) {
        DestroyWindow(g_fakeShellWnd);
    }

    printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

// Per-process, so two runs (for example the mutation check and a plain build)
// cannot end up sharing a desktop and finding each other's Shell_TrayWnd.
const std::wstring& DesktopName() {
    static const std::wstring name =
        L"SplitTrayIntegration_" + std::to_wstring(GetCurrentProcessId());
    return name;
}

// Re-launches this executable on the private desktop and mirrors its exit code.
//
// SetThreadDesktop would only move the calling thread: threads created afterwards,
// including the mod's own tray thread, stay on the desktop the *process* was
// started on. That splits the test across two desktops, and
// SetWindowsHookEx - which is how a subclass reaches a window on another thread -
// then fails with ERROR_ACCESS_DENIED because hooks are desktop-scoped. Starting
// a whole process on the desktop puts every thread there, which is also what
// happens inside Explorer, where the mod's threads and the taskbar always share a
// desktop.
int RunChildOnDesktop() {
    WCHAR exePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        printf("GetModuleFileNameW failed: %lu\n", GetLastError());
        return 1;
    }
    std::wstring commandLine = L"\"";
    commandLine += exePath;
    commandLine += L"\" --child";

    STARTUPINFOW si = {sizeof(si)};
    std::wstring desktopName = DesktopName();
    si.lpDesktop = desktopName.data();
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0,
                        nullptr, nullptr, &si, &pi)) {
        printf("CreateProcessW on the private desktop failed: %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);

    DWORD exitCode = 1;
    if (WaitForSingleObject(pi.hProcess, 180000) == WAIT_TIMEOUT) {
        printf("integration test timed out; terminating\n");
        TerminateProcess(pi.hProcess, 1);
    } else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }
    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);

    const bool isChild = argc > 1 && strcmp(argv[1], "--child") == 0;
    if (isChild) {
        return RunTests();
    }

    HDESK desktop = CreateDesktopW(DesktopName().c_str(), nullptr, nullptr, 0,
                                   GENERIC_ALL,
                                   nullptr);
    if (!desktop) {
        printf("CreateDesktopW failed: %lu\n", GetLastError());
        return 1;
    }
    const int result = RunChildOnDesktop();
    CloseDesktop(desktop);
    return result;
}
