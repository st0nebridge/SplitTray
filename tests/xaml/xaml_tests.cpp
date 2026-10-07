// =============================================================================
// Module:  tests/xaml/xaml_tests.cpp
// Purpose: The mod's XAML section (section 10) run against a real XAML tree.
//          The other suites compile it out (SPLITTRAY_NO_XAML). This one hosts
//          a XAML island in a window of its own, builds a row shaped like the
//          taskbar's tray row in it, and drives the embedded tray's code: where
//          the panel goes, how its cells are sized and drawn, the chevron and
//          the handle, updates in place, the saved order, where an icon is on
//          screen, a cell invoked as a screen reader invokes it, and taking it
//          all back out.
//
//          What it cannot reach is the way in: resolving Explorer's private
//          symbols and recognising Explorer's own SystemTray.* elements. A test
//          process has neither, so the harness's HookSymbols finds nothing,
//          and the tests start from an anchor element, below the hooks.
//
//          The window is never shown and nothing is typed or clicked: the suite
//          does not take the focus or the pointer from whoever is at the
//          machine. Hosting XAML in a Win32 window needs the process manifest
//          to name Windows 10, so tools/build.ps1 writes xaml_tests.exe.manifest
//          beside it.
// Deps:    the mod's source, tests/harness, tests/regression/golden_payloads.h.
// =============================================================================

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "../../src/split-tray.wh.cpp"

#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

#include "../regression/golden_payloads.h"

using namespace SplitTray;
namespace X = SplitTrayXaml;
namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxmi = winrt::Windows::UI::Xaml::Media::Imaging;
namespace wuxh = winrt::Windows::UI::Xaml::Hosting;
namespace wuxap = winrt::Windows::UI::Xaml::Automation::Peers;
namespace wuxapr = winrt::Windows::UI::Xaml::Automation::Provider;

namespace {

int g_failures = 0;
int g_checks = 0;
const char* g_currentTest = "";

void Check(bool condition, const char* expression, int line) {
    g_checks++;
    if (!condition) {
        g_failures++;
        printf("  FAIL  %s:%d  %s\n", g_currentTest, line, expression);
    }
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        auto a_ = (actual);                                                 \
        auto e_ = (expected);                                               \
        g_checks++;                                                         \
        if (!(a_ == e_)) {                                                  \
            g_failures++;                                                   \
            printf("  FAIL  %s:%d  %s == %s (got %lld, want %lld)\n",       \
                   g_currentTest, __LINE__, #actual, #expected,             \
                   static_cast<long long>(a_), static_cast<long long>(e_)); \
        }                                                                   \
    } while (0)

#define CHECK_WSTR(actual, expected)                                 \
    do {                                                             \
        std::wstring a_ = (actual);                                  \
        std::wstring e_ = (expected);                                \
        g_checks++;                                                  \
        if (a_ != e_) {                                              \
            g_failures++;                                            \
            printf("  FAIL  %s:%d  %s == L\"%ls\" (got L\"%ls\")\n", \
                   g_currentTest, __LINE__, #actual, e_.c_str(),     \
                   a_.c_str());                                      \
        }                                                            \
    } while (0)

// ---------------------------------------------------------------------------
// The island
// ---------------------------------------------------------------------------

// Where the stand-in taskbar window is, in screen pixels. It is never shown;
// IconScreenRect only reads its rect.
constexpr RECT kTaskbarRect = {100, 200, 900, 320};

struct Island {
    HWND host = nullptr;
    wuxh::WindowsXamlManager manager{nullptr};
    wuxh::DesktopWindowXamlSource source{nullptr};
    wuxc::Grid root{nullptr};
};

Island g_island;

void Pump(DWORD milliseconds = 30) {
    const ULONGLONG until = GetTickCount64() + milliseconds;
    do {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_island.root) {
            g_island.root.UpdateLayout();
        }
        Sleep(1);
    } while (GetTickCount64() < until);
}

bool OpenIsland() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SplitTrayXamlTestTaskbar";
    RegisterClassW(&wc);
    g_island.host = CreateWindowExW(
        WS_EX_TOOLWINDOW, wc.lpszClassName, L"stand-in taskbar", WS_POPUP,
        kTaskbarRect.left, kTaskbarRect.top, kTaskbarRect.right - kTaskbarRect.left,
        kTaskbarRect.bottom - kTaskbarRect.top, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_island.host) {
        printf("could not create the host window: %lu\n", GetLastError());
        return false;
    }
    try {
        g_island.manager = wuxh::WindowsXamlManager::InitializeForCurrentThread();
        g_island.source = wuxh::DesktopWindowXamlSource();
        auto native = g_island.source.as<IDesktopWindowXamlSourceNative>();
        winrt::check_hresult(native->AttachToWindow(g_island.host));
        HWND islandWnd = nullptr;
        winrt::check_hresult(native->get_WindowHandle(&islandWnd));
        SetWindowPos(islandWnd, nullptr, 0, 0, kTaskbarRect.right - kTaskbarRect.left,
                     kTaskbarRect.bottom - kTaskbarRect.top, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        g_island.root = wuxc::Grid();
        g_island.source.Content(g_island.root);
    } catch (winrt::hresult_error const& error) {
        printf("could not host XAML (%08X): %ls\n", static_cast<unsigned>(error.code()),
               error.message().c_str());
        printf("xaml_tests.exe.manifest has to be beside the exe; tools/build.ps1 "
               "writes it\n");
        return false;
    }
    Pump();
    return true;
}

void CloseIsland() {
    X::RemoveEverything();
    if (g_island.root) {
        g_island.root.Children().Clear();
    }
    g_island.root = nullptr;
    if (g_island.source) {
        g_island.source.Close();
    }
    if (g_island.manager) {
        g_island.manager.Close();
    }
    Pump();
    DestroyWindow(g_island.host);
}

// A row shaped like the taskbar's: something before the tray, the anchor's own
// wrapper - which is what the walk up from the anchor comes through - and the
// clock after it. The anchor sits in a Grid inside its wrapper, as a tray
// element sits inside its button's template.
struct TaskbarRow {
    wuxc::StackPanel row;
    wuxc::Border before;
    wuxc::Border wrapper;
    wuxc::Grid inner;
    wuxc::Button anchor;
    wuxc::Border clock;
};

TaskbarRow MakeRow(double anchorHeight) {
    TaskbarRow r;
    r.row.Orientation(wuxc::Orientation::Horizontal);
    r.row.Name(L"SystemTrayFrameGrid");
    r.before.Width(40);
    r.before.Height(40);
    r.anchor.Width(68);
    r.anchor.Height(anchorHeight);
    r.inner.Children().Append(r.anchor);
    r.wrapper.Child(r.inner);
    r.clock.Width(70);
    r.clock.Height(40);
    r.row.Children().Append(r.before);
    r.row.Children().Append(r.wrapper);
    r.row.Children().Append(r.clock);
    g_island.root.Children().Clear();
    g_island.root.Children().Append(r.row);
    Pump();
    return r;
}

// ---------------------------------------------------------------------------
// The icon store, driven through the real notification path
// ---------------------------------------------------------------------------

const HMONITOR kLeftDisplay = reinterpret_cast<HMONITOR>(2);
constexpr DWORD kCallback = 0x8123;

bool g_leftDisplayConnected = true;

std::vector<MonitorInfoEntry> TwoDisplays() {
    MonitorInfoEntry left;
    left.handle = kLeftDisplay;
    left.workArea = {-1920, 0, 0, 1032};
    left.dpi = 96;
    MonitorInfoEntry primary;
    primary.handle = reinterpret_cast<HMONITOR>(1);
    primary.workArea = {0, 0, 1920, 1032};
    primary.primary = true;
    primary.dpi = 96;
    if (!g_leftDisplayConnected) {
        return {primary};
    }
    return {left, primary};
}

std::wstring NoProcessPath(HWND) {
    return std::wstring();
}

Settings EmbeddingSettings() {
    Settings s;
    s.defaultTray = Destination::Secondary;
    s.embedInTaskbar = true;
    s.showTooltips = true;
    s.maxVisibleIcons = 8;
    return s;
}

void ResetEverything(const Settings& settings) {
    X::RemoveEverything();
    X::g_iconOrder.clear();
    X::g_iconOrderLoaded = false;
    X::g_drag = X::DragState{};
    X::g_refreshPending = false;
    g_leftDisplayConnected = true;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& icon : g_icons) {
        if (icon.icon) {
            DestroyIcon(icon.icon);
        }
    }
    g_icons.clear();
    g_primaryOnly.clear();
    g_settings = settings;
    g_enumerateMonitors = TwoDisplays;
    g_lookUpProcessPath = NoProcessPath;
    g_embeddedMonitors.clear();
    SplitTrayTestHarness::StoredValues().clear();
    g_displaySlots.clear();
    g_displaySlotsLoaded = false;
    RecomputeGeometryLocked();
    g_placements.clear();
    g_placementsLoaded = false;
    g_hidden.clear();
    g_hiddenLoaded = false;
}

void ApplySettings(const Settings& settings) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_settings = settings;
    RecomputeGeometryLocked();
}

DWORD WireHandle(HWND wnd) {
    return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(wnd));
}

std::vector<BYTE> MakePayload(DWORD message, HWND owner, UINT uID, DWORD flags,
                              PCWSTR tip, HICON icon) {
    std::vector<BYTE> p(kPayloadUnicodeV4, kPayloadUnicodeV4 + sizeof(kPayloadUnicodeV4));
    const GUID none = {};
    const DWORD ownerWnd = WireHandle(owner);
    const DWORD iconHandle = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(icon));
    const DWORD callback = kCallback;
    memcpy(p.data() + wire::kGuid, &none, sizeof(GUID));
    memcpy(p.data() + wire::kMessage, &message, 4);
    memcpy(p.data() + wire::kOwnerWnd, &ownerWnd, 4);
    memcpy(p.data() + wire::kUID, &uID, 4);
    memcpy(p.data() + wire::kFlags, &flags, 4);
    memcpy(p.data() + wire::kIcon, &iconHandle, 4);
    memcpy(p.data() + wire::kCallbackMsg, &callback, 4);
    memset(p.data() + wire::kTip, 0, wire::kTipChars * sizeof(wchar_t));
    if (tip) {
        memcpy(p.data() + wire::kTip, tip,
               std::min<size_t>(wcslen(tip), wire::kTipChars - 1) * sizeof(wchar_t));
    }
    memset(p.data() + wire::kExePath, 0, wire::kExePathChars * sizeof(wchar_t));
    const wchar_t exe[] = L"C:\\apps\\thing.exe";
    memcpy(p.data() + wire::kExePath, exe, sizeof(exe));
    return p;
}

void Feed(const std::vector<BYTE>& payload) {
    TrayNotification n;
    if (!ParseTrayNotification(kTrayCopyDataId, payload.data(), payload.size(), &n)) {
        printf("    a test payload did not parse\n");
        return;
    }
    bool forward = false;
    std::lock_guard<std::mutex> lock(g_mutex);
    FillMissingPathLocked(&n);
    ApplyNotificationLocked(n, payload, &forward, nullptr, nullptr);
}

uint64_t SerialOf(HWND owner, UINT uID) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& icon : g_icons) {
        if (icon.ownerWnd == owner && icon.uID == uID) {
            return icon.serial;
        }
    }
    return 0;
}

uint64_t AddIcon(HWND owner, UINT uID, PCWSTR tip, HICON icon) {
    Feed(MakePayload(NIM_ADD, owner, uID, NIF_MESSAGE | NIF_ICON | NIF_TIP, tip, icon));
    return SerialOf(owner, uID);
}

void SetVersion(HWND owner, UINT uID, DWORD version) {
    auto p = MakePayload(NIM_SETVERSION, owner, uID, 0, nullptr, nullptr);
    memcpy(p.data() + wire::kVersion, &version, 4);
    Feed(p);
}

// A 16 by 16 picture of one colour, so two pictures can be told apart.
HICON MakeIcon(BYTE r, BYTE g, BYTE b) {
    BYTE mask[16 * 16 / 8] = {};
    std::vector<DWORD> pixels(16 * 16, 0xFF000000u | (DWORD(r) << 16) | (DWORD(g) << 8) | b);
    return CreateIcon(GetModuleHandleW(nullptr), 16, 16, 1, 32, mask,
                      reinterpret_cast<BYTE*>(pixels.data()));
}

// The application on the other end: a window that keeps the callbacks it gets.
struct Callback {
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};

std::vector<Callback> g_received;

LRESULT CALLBACK OwnerProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == kCallback) {
        g_received.push_back({message, wParam, lParam});
        return 0;
    }
    return DefWindowProcW(wnd, message, wParam, lParam);
}

HWND MakeOwner() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = OwnerProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"SplitTrayXamlTestOwner";
        RegisterClassW(&wc);
        registered = true;
    }
    return CreateWindowExW(0, L"SplitTrayXamlTestOwner", nullptr, 0, 0, 0, 0, 0,
                           HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
}

// ---------------------------------------------------------------------------
// Reading what was drawn
// ---------------------------------------------------------------------------

X::EmbeddedTray* LeftTray() {
    return X::TrayOfMonitor(kLeftDisplay);
}

// The left display's tray, put into `row` from its anchor.
X::EmbeddedTray* Embed(TaskbarRow const& row) {
    X::SyncEmbeddedTrays();
    X::EmbeddedTray* tray = LeftTray();
    if (!tray) {
        return nullptr;
    }
    tray->taskbarWnd = g_island.host;
    if (!X::EnsureEmbeddedPanel(*tray, row.anchor)) {
        return nullptr;
    }
    return tray;
}

wuxc::StackPanel PanelOf(X::EmbeddedTray* tray) {
    return tray ? tray->panel.get() : nullptr;
}

std::vector<wuxc::Border> CellsOf(X::EmbeddedTray* tray) {
    std::vector<wuxc::Border> cells;
    if (auto panel = PanelOf(tray)) {
        for (auto child : panel.Children()) {
            cells.push_back(child.try_as<wuxc::Border>());
        }
    }
    return cells;
}

uint64_t SerialAt(X::EmbeddedTray* tray, size_t index) {
    auto cells = CellsOf(tray);
    return index < cells.size() && cells[index] ? X::SerialOfCell(cells[index]) : ~0ull;
}

std::wstring TooltipOf(wuxc::Border const& cell) {
    auto boxed = wuxc::ToolTipService::GetToolTip(cell);
    return boxed ? winrt::unbox_value_or<winrt::hstring>(boxed, L"").c_str() : L"";
}

std::wstring NameOf(wux::DependencyObject const& element) {
    return wux::Automation::AutomationProperties::GetName(element).c_str();
}

wuxc::Image ImageOf(wuxc::Border const& cell) {
    auto face = X::FaceOfCell(cell);
    return face ? face.Content().try_as<wuxc::Image>() : nullptr;
}

size_t RebuildsLogged() {
    size_t count = 0;
    for (const auto& line : SplitTrayTestHarness::LogSnapshot()) {
        if (line.find(L"rebuilt:") != std::wstring::npos) {
            count++;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// Where the tray goes, and how big its cells are
// ---------------------------------------------------------------------------

void Test_TheTrayGoesIntoTheRowBeforeTheAnchorsWrapper() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    // Before the wrapper the walk came through, so beside the clock and not in
    // the anchor's own template (FindTrayRow).
    CHECK_EQ(r.row.Children().Size(), 4u);
    auto inserted = r.row.Children().GetAt(1).try_as<wuxc::StackPanel>();
    CHECK(inserted && inserted == PanelOf(tray));
    CHECK(inserted && inserted.Name() == L"SplitTrayIcons");
    CHECK(r.row.Children().GetAt(0) == r.before);
    CHECK(r.row.Children().GetAt(2) == r.wrapper);
    CHECK_EQ(r.inner.Children().Size(), 1u);
    CHECK(tray->active);
    CHECK(g_embeddedMonitors.count(kLeftDisplay) == 1);

    // Asked again while it is still there, it is the same panel.
    CHECK(X::EnsureEmbeddedPanel(*tray, r.anchor));
    CHECK(PanelOf(tray) == inserted);
    CHECK_EQ(r.row.Children().Size(), 4u);
}

void Test_WithNoStackPanelAboveAnyPanelWillDo() {
    // No row that lays its children out in a line: the nearest panel above the
    // anchor is taken, which may overlap, rather than nothing.
    ResetEverything(EmbeddingSettings());
    wuxc::Grid outer;
    wuxc::Border wrapper;
    wuxc::Grid inner;
    wuxc::Button anchor;
    anchor.Width(68);
    anchor.Height(38);
    inner.Children().Append(anchor);
    wrapper.Child(inner);
    outer.Children().Append(wrapper);
    g_island.root.Children().Clear();
    g_island.root.Children().Append(outer);
    Pump();

    X::SyncEmbeddedTrays();
    X::EmbeddedTray* tray = LeftTray();
    CHECK(tray && X::EnsureEmbeddedPanel(*tray, anchor));
    CHECK_EQ(inner.Children().Size(), 2u);
    CHECK(tray && inner.Children().GetAt(0) == PanelOf(tray));
    CHECK(inner.Children().GetAt(1) == anchor);
}

void Test_CellsAreSizedFromTheAnchorsHeight() {
    struct Case {
        double anchor;
        double width, height, icon;
    };
    // The native tray's 32 by 38 cell and 16 pixel icon, scaled by the row's
    // height; a scale past 3 is taken as 3 (a taskbar resized by another mod
    // is fine, a nonsense value is not).
    const Case cases[] = {
        {38, 32, 38, 16},
        {57, 48, 57, 24},
        {19, 16, 19, 8},
        {200, 96, 200, 48},
    };
    for (const Case& c : cases) {
        ResetEverything(EmbeddingSettings());
        TaskbarRow r = MakeRow(c.anchor);
        X::EmbeddedTray* tray = Embed(r);
        CHECK(tray != nullptr);
        if (!tray) {
            continue;
        }
        CHECK_EQ(static_cast<int>(tray->cellWidthDip + 0.5), static_cast<int>(c.width));
        CHECK_EQ(static_cast<int>(tray->cellHeightDip + 0.5), static_cast<int>(c.height));
        CHECK_EQ(static_cast<int>(tray->iconSizeDip + 0.5), static_cast<int>(c.icon));
    }

    // An anchor not laid out yet has no height: the native size, unscaled.
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    r.anchor.Visibility(wux::Visibility::Collapsed);
    Pump();
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray && tray->cellWidthDip == 32 && tray->cellHeightDip == 38 &&
          tray->iconSizeDip == 16);
}

// ---------------------------------------------------------------------------
// What the tray draws
// ---------------------------------------------------------------------------

void Test_AnEmptyTrayShowsItsHandle() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    auto cells = CellsOf(tray);
    CHECK_EQ(cells.size(), 1u);
    if (cells.size() == 1 && cells[0]) {
        CHECK_EQ(X::SerialOfCell(cells[0]), 0ull);
        // Something to click when nothing else is, named for screen readers.
        CHECK_WSTR(NameOf(cells[0].Child()), L"Split Tray, no icons here yet");
    }
}

void Test_EachIconIsACellNamedAndWithItsTooltip() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    HICON picture = MakeIcon(200, 30, 30);
    const uint64_t a = AddIcon(owner, 1, L"alpha", picture);
    const uint64_t b = AddIcon(owner, 2, L"beta\nsecond line", picture);
    const uint64_t c = AddIcon(owner, 3, L"gamma", picture);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    Pump();

    auto cells = CellsOf(tray);
    CHECK_EQ(cells.size(), 3u);
    if (cells.size() != 3) {
        return;
    }
    // In the order the icons registered.
    CHECK_EQ(X::SerialOfCell(cells[0]), a);
    CHECK_EQ(X::SerialOfCell(cells[1]), b);
    CHECK_EQ(X::SerialOfCell(cells[2]), c);

    for (const auto& cell : cells) {
        CHECK(cell.Width() == tray->cellWidthDip && cell.Height() == tray->cellHeightDip);
        // Laid out at that size in the row.
        CHECK_EQ(static_cast<int>(cell.ActualWidth() + 0.5),
                 static_cast<int>(tray->cellWidthDip + 0.5));
    }
    CHECK_WSTR(TooltipOf(cells[0]), L"alpha");
    CHECK_WSTR(TooltipOf(cells[1]), L"beta\nsecond line");
    // A screen reader hears the first line (IconLabel).
    CHECK_WSTR(NameOf(X::FaceOfCell(cells[0])), L"alpha");
    CHECK_WSTR(NameOf(X::FaceOfCell(cells[1])), L"beta");

    // The picture, as a bitmap of the icon's own size, drawn at the tray's.
    auto image = ImageOf(cells[0]);
    CHECK(image != nullptr);
    if (image) {
        CHECK(image.Width() == tray->iconSizeDip);
        auto bitmap = image.Source().try_as<wuxmi::WriteableBitmap>();
        CHECK(bitmap && bitmap.PixelWidth() == 16 && bitmap.PixelHeight() == 16);
    }
    DestroyIcon(picture);
    DestroyWindow(owner);
}

void Test_TooltipsSwitchedOffLeaveTheName() {
    Settings s = EmbeddingSettings();
    s.showTooltips = false;
    ResetEverything(s);
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    AddIcon(owner, 1, L"alpha", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    auto cells = CellsOf(tray);
    CHECK_EQ(cells.size(), 1u);
    if (!cells.empty() && cells[0]) {
        CHECK(wuxc::ToolTipService::GetToolTip(cells[0]) == nullptr);
        CHECK_WSTR(NameOf(X::FaceOfCell(cells[0])), L"alpha");
        // No picture, no image.
        CHECK(ImageOf(cells[0]) == nullptr);
    }

    // Switched back on, the tooltip is put on the cell as it stands.
    s.showTooltips = true;
    ApplySettings(s);
    X::RefreshTray(*tray);
    cells = CellsOf(tray);
    CHECK(!cells.empty() && cells[0] && TooltipOf(cells[0]) == L"alpha");
    DestroyWindow(owner);
}

void Test_PastTheLimitTheRestGoBehindTheChevronWhichComesFirst() {
    Settings s = EmbeddingSettings();
    s.maxVisibleIcons = 2;
    ResetEverything(s);
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t a = AddIcon(owner, 1, L"alpha", nullptr);
    const uint64_t b = AddIcon(owner, 2, L"beta", nullptr);
    const uint64_t c = AddIcon(owner, 3, L"gamma", nullptr);
    const uint64_t d = AddIcon(owner, 4, L"delta", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    Pump();

    auto cells = CellsOf(tray);
    CHECK_EQ(cells.size(), 3u);
    CHECK_EQ(SerialAt(tray, 0), 0ull);  // the chevron
    CHECK_EQ(SerialAt(tray, 1), a);
    CHECK_EQ(SerialAt(tray, 2), b);
    if (!cells.empty() && cells[0]) {
        CHECK_WSTR(TooltipOf(cells[0]), L"Show hidden icons");
        CHECK_WSTR(NameOf(cells[0].Child()), L"Show hidden icons");
    }
    CHECK_EQ(tray->hiddenEntries.size(), 2u);
    CHECK(tray->hiddenEntries.size() == 2 && tray->hiddenEntries[0].serial == c &&
          tray->hiddenEntries[1].serial == d);

    // An icon behind the chevron is where the chevron is (IconScreenRect).
    RECT hidden = {}, chevron = {};
    CHECK(X::IconScreenRect(c, &hidden));
    CHECK(!cells.empty() && cells[0] && X::ElementScreenRect(*tray, cells[0], &chevron));
    CHECK(EqualRect(&hidden, &chevron));
    DestroyWindow(owner);
}

void Test_AnIconTheUserHidIsBehindTheChevronHoweverMuchRoom() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t a = AddIcon(owner, 1, L"alpha", nullptr);
    const uint64_t b = AddIcon(owner, 2, L"beta", nullptr);
    SetIconHidden(L"thing.exe#1", true);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    CHECK_EQ(CellsOf(tray).size(), 2u);
    CHECK_EQ(SerialAt(tray, 0), 0ull);
    CHECK_EQ(SerialAt(tray, 1), b);
    CHECK(tray->hiddenEntries.size() == 1 && tray->hiddenEntries[0].serial == a);

    // Shown again, the chevron goes.
    SetIconHidden(L"thing.exe#1", false);
    X::RefreshTray(*tray);
    CHECK_EQ(CellsOf(tray).size(), 2u);
    CHECK_EQ(SerialAt(tray, 0), a);
    CHECK(tray->hiddenEntries.empty());
    DestroyWindow(owner);
}

// ---------------------------------------------------------------------------
// Updates in place (DECISIONS 82)
// ---------------------------------------------------------------------------

void Test_ANewTooltipOrPictureIsDrawnInPlace() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    HICON red = MakeIcon(200, 30, 30);
    HICON blue = MakeIcon(30, 30, 200);
    AddIcon(owner, 1, L"alpha", red);
    AddIcon(owner, 2, L"beta", red);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    const auto before = CellsOf(tray);
    CHECK_EQ(before.size(), 2u);
    if (before.size() != 2) {
        return;
    }
    const size_t rebuilds = RebuildsLogged();
    auto firstImage = ImageOf(before[0]);
    auto firstSource = firstImage ? firstImage.Source() : nullptr;

    // A new tooltip on the second icon: the same cells, the new text, and the
    // first icon's picture not drawn again.
    Feed(MakePayload(NIM_MODIFY, owner, 2, NIF_TIP, L"beta 2", nullptr));
    X::RefreshTray(*tray);
    auto after = CellsOf(tray);
    CHECK(after.size() == 2 && after[0] == before[0] && after[1] == before[1]);
    CHECK(after.size() == 2 && TooltipOf(after[1]) == L"beta 2");
    CHECK(after.size() == 2 && NameOf(X::FaceOfCell(after[1])) == L"beta 2");
    CHECK(firstImage && firstImage.Source() == firstSource);
    CHECK_EQ(RebuildsLogged(), rebuilds);

    // A new picture on the first: drawn into the same cell's image.
    Feed(MakePayload(NIM_MODIFY, owner, 1, NIF_ICON, nullptr, blue));
    X::RefreshTray(*tray);
    after = CellsOf(tray);
    CHECK(after.size() == 2 && after[0] == before[0]);
    CHECK(firstImage && firstImage.Source() != firstSource);
    CHECK(after.size() == 2 && ImageOf(after[0]) == firstImage);
    CHECK_EQ(RebuildsLogged(), rebuilds);

    // Its picture taken away: cleared, in place (DECISIONS 76).
    Feed(MakePayload(NIM_MODIFY, owner, 1, NIF_ICON, nullptr, nullptr));
    X::RefreshTray(*tray);
    after = CellsOf(tray);
    CHECK(after.size() == 2 && after[0] == before[0] && ImageOf(after[0]) == nullptr);

    // And given one again, a fresh image in the same cell.
    Feed(MakePayload(NIM_MODIFY, owner, 1, NIF_ICON, nullptr, red));
    X::RefreshTray(*tray);
    after = CellsOf(tray);
    CHECK(after.size() == 2 && after[0] == before[0] && ImageOf(after[0]) != nullptr);
    CHECK_EQ(RebuildsLogged(), rebuilds);

    // A new icon changes the layout: that is a rebuild.
    AddIcon(owner, 3, L"gamma", red);
    X::RefreshTray(*tray);
    CHECK_EQ(CellsOf(tray).size(), 3u);
    CHECK_EQ(RebuildsLogged(), rebuilds + 1);
    DestroyIcon(red);
    DestroyIcon(blue);
    DestroyWindow(owner);
}

void Test_ARefreshWaitsWhileThePointerIsDown() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    AddIcon(owner, 1, L"alpha", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshEmbeddedTray();
    CHECK_EQ(CellsOf(tray).size(), 1u);

    // A press may become a drag; a rebuild under it would end the press.
    X::g_drag.pointerDown = true;
    AddIcon(owner, 2, L"beta", nullptr);
    X::RefreshEmbeddedTray();
    CHECK_EQ(CellsOf(tray).size(), 1u);
    CHECK(X::g_refreshPending);

    X::g_drag.pointerDown = false;
    X::RefreshEmbeddedTray();
    CHECK_EQ(CellsOf(tray).size(), 2u);
    CHECK(!X::g_refreshPending);
    DestroyWindow(owner);
}

// ---------------------------------------------------------------------------
// The order the user chose
// ---------------------------------------------------------------------------

void Test_TheSavedOrderDecidesTheRow() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t a = AddIcon(owner, 1, L"alpha", nullptr);
    const uint64_t b = AddIcon(owner, 2, L"beta", nullptr);
    const uint64_t c = AddIcon(owner, 3, L"gamma", nullptr);
    SplitTrayTestHarness::StoredValues()[kIconOrderValue] =
        L"thing.exe#3\nthing.exe#1";
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    // The saved ones first, in the saved order; the rest after, as they came.
    CHECK_EQ(SerialAt(tray, 0), c);
    CHECK_EQ(SerialAt(tray, 1), a);
    CHECK_EQ(SerialAt(tray, 2), b);
    DestroyWindow(owner);
}

void Test_AReorderedRowIsSavedAndReadBack() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t a = AddIcon(owner, 1, L"alpha", nullptr);
    const uint64_t b = AddIcon(owner, 2, L"beta", nullptr);
    const uint64_t c = AddIcon(owner, 3, L"gamma", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    auto panel = PanelOf(tray);

    // The first icon dragged to the end, as a drag does it: without the cell
    // ever leaving the panel (ShiftCellTo).
    auto dragged = CellsOf(tray)[0];
    X::ShiftCellTo(panel, 0, 2);
    CHECK_EQ(SerialAt(tray, 0), b);
    CHECK_EQ(SerialAt(tray, 1), c);
    CHECK_EQ(SerialAt(tray, 2), a);
    CHECK(CellsOf(tray)[2] == dragged);

    X::CommitVisualOrder(panel);
    CHECK_WSTR(SplitTrayTestHarness::StoredValues()[kIconOrderValue],
               L"thing.exe#2\nthing.exe#3\nthing.exe#1");

    // Read back at the next start, it puts the row the same way.
    X::g_iconOrder.clear();
    X::g_iconOrderLoaded = false;
    tray->drawnAll.clear();
    X::RefreshTray(*tray);
    CHECK_EQ(SerialAt(tray, 0), b);
    CHECK_EQ(SerialAt(tray, 1), c);
    CHECK_EQ(SerialAt(tray, 2), a);

    // An icon not on screen keeps its place in the saved order.
    Feed(MakePayload(NIM_DELETE, owner, 3, 0, nullptr, nullptr));
    X::RefreshTray(*tray);
    X::CommitVisualOrder(PanelOf(tray));
    CHECK_WSTR(SplitTrayTestHarness::StoredValues()[kIconOrderValue],
               L"thing.exe#2\nthing.exe#1\nthing.exe#3");
    DestroyWindow(owner);
}

// ---------------------------------------------------------------------------
// Where an icon is, and what invoking it does
// ---------------------------------------------------------------------------

void Test_AnIconIsWhereItsCellIsOnScreen() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t a = AddIcon(owner, 1, L"alpha", nullptr);
    const uint64_t b = AddIcon(owner, 2, L"beta", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    Pump();

    const double scale = g_island.root.XamlRoot().RasterizationScale();
    RECT first = {}, second = {};
    CHECK(X::IconScreenRect(a, &first));
    CHECK(X::IconScreenRect(b, &second));
    // Inside the taskbar window, a cell wide, and side by side, after the 40
    // pixel element that comes before the tray in the row.
    CHECK(first.left >= kTaskbarRect.left && first.right <= kTaskbarRect.right);
    CHECK(first.top >= kTaskbarRect.top && first.bottom <= kTaskbarRect.bottom);
    const int cellWidth = static_cast<int>(tray->cellWidthDip * scale + 0.5);
    CHECK(abs((first.right - first.left) - cellWidth) <= 1);
    CHECK(abs(first.left - (kTaskbarRect.left + static_cast<int>(40 * scale + 0.5))) <= 1);
    CHECK(abs(second.left - first.right) <= 1);

    // An icon that is not in this tray is nowhere the mod can say.
    RECT none = {};
    CHECK(!X::IconScreenRect(0xDEAD, &none));
    DestroyWindow(owner);
}

void Test_InvokingACellIsTheKeyboardsSelectionToItsApplication() {
    // As a screen reader invokes it, and as Enter or Space does (DECISIONS 80):
    // an icon before version 3 gets the clicks the key stands for, a later one
    // NIN_KEYSELECT.
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t older = AddIcon(owner, 7, L"older", nullptr);
    const uint64_t newer = AddIcon(owner, 8, L"newer", nullptr);
    SetVersion(owner, 8, NOTIFYICON_VERSION_4);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    Pump();
    auto cells = CellsOf(tray);
    CHECK_EQ(cells.size(), 2u);
    if (cells.size() != 2) {
        return;
    }
    CHECK_EQ(X::SerialOfCell(cells[0]), older);
    CHECK_EQ(X::SerialOfCell(cells[1]), newer);

    auto invoke = [](wuxc::Border const& cell) {
        wuxap::ButtonAutomationPeer peer(X::FaceOfCell(cell));
        peer.as<wuxapr::IInvokeProvider>().Invoke();
        Pump(100);
    };

    g_received.clear();
    invoke(cells[0]);
    CHECK_EQ(g_received.size(), 2u);
    if (g_received.size() == 2) {
        CHECK_EQ(g_received[0].wParam, 7u);
        CHECK_EQ(static_cast<UINT>(g_received[0].lParam), static_cast<UINT>(WM_LBUTTONDOWN));
        CHECK_EQ(static_cast<UINT>(g_received[1].lParam), static_cast<UINT>(WM_LBUTTONUP));
    }

    g_received.clear();
    invoke(cells[1]);
    CHECK_EQ(g_received.size(), 1u);
    if (g_received.size() == 1) {
        CHECK_EQ(LOWORD(g_received[0].lParam), static_cast<WORD>(NIN_KEYSELECT));
        CHECK_EQ(HIWORD(g_received[0].lParam), 8u);
    }
    DestroyWindow(owner);
}

// ---------------------------------------------------------------------------
// Taking it back out
// ---------------------------------------------------------------------------

void Test_ATrayDroppedByARebuildIsPutBack() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    AddIcon(owner, 1, L"alpha", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    auto old = PanelOf(tray);

    // The taskbar rebuilt its row without the mod's panel in it.
    r.row.Children().RemoveAt(1);
    CHECK_EQ(r.row.Children().Size(), 3u);
    CHECK(X::EnsureEmbeddedPanel(*tray, r.anchor));
    auto fresh = PanelOf(tray);
    CHECK(fresh && fresh != old);
    CHECK_EQ(r.row.Children().Size(), 4u);
    // Nothing is taken as drawn in the new panel, so it is drawn whole.
    CHECK(tray->drawnAll.empty());
    X::RefreshTray(*tray);
    CHECK_EQ(CellsOf(tray).size(), 1u);
    DestroyWindow(owner);
}

void Test_TakingTheTrayOutLeavesTheRowAsItWas() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    X::RemovePanel(*tray);
    CHECK_EQ(r.row.Children().Size(), 3u);
    CHECK(r.row.Children().GetAt(0) == r.before && r.row.Children().GetAt(1) == r.wrapper &&
          r.row.Children().GetAt(2) == r.clock);
    CHECK(!tray->active);
    CHECK(PanelOf(tray) == nullptr);
    CHECK(g_embeddedMonitors.empty());
}

void Test_EmbeddingOffOrTheDisplayGoneTakesTheTrayOut() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    CHECK(Embed(r) != nullptr);
    CHECK_EQ(r.row.Children().Size(), 4u);

    Settings off = EmbeddingSettings();
    off.embedInTaskbar = false;
    ApplySettings(off);
    X::SyncEmbeddedTrays();
    CHECK(X::g_embeddedTrays.empty());
    CHECK_EQ(r.row.Children().Size(), 3u);

    // On again, and then the display unplugged: its tray is kept, unavailable,
    // and nothing of it stays in the taskbar.
    ApplySettings(EmbeddingSettings());
    CHECK(Embed(r) != nullptr);
    CHECK_EQ(r.row.Children().Size(), 4u);
    g_leftDisplayConnected = false;
    ApplySettings(EmbeddingSettings());
    X::SyncEmbeddedTrays();
    CHECK(LeftTray() == nullptr);
    CHECK_EQ(r.row.Children().Size(), 3u);
}

void Test_UnloadingTakesEveryPanelOut() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    AddIcon(owner, 1, L"alpha", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (tray) {
        X::RefreshTray(*tray);
    }
    X::RemoveEverything();
    CHECK(X::g_embeddedTrays.empty());
    CHECK_EQ(r.row.Children().Size(), 3u);
    CHECK(g_embeddedMonitors.empty());
    DestroyWindow(owner);
}

void Test_TheAttachWalkLeavesATreeWithNoTrayOfExplorersAlone() {
    // The walk down from a taskbar's root anchors only on Explorer's own
    // SystemTray.* elements (AnchorPreference). A tree with none is left as it
    // is, and the walk says what it found.
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    X::SyncEmbeddedTrays();
    X::EmbeddedTray* tray = LeftTray();
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    tray->taskbarWnd = g_island.host;
    tray->root = g_island.root.XamlRoot();
    X::g_loggedAttachCandidates = false;
    X::TryAttachTray(*tray);
    CHECK(!tray->active);
    CHECK_EQ(r.row.Children().Size(), 3u);
    bool logged = false;
    for (const auto& line : SplitTrayTestHarness::LogSnapshot()) {
        if (line.find(L"0 SystemTray.* candidate(s)") != std::wstring::npos) {
            logged = true;
        }
    }
    CHECK(logged);
}


// ---------------------------------------------------------------------------
// The pointer on a cell (CellPressed, CellDragged, CellReleased and the rest)
//
// A test cannot raise a pointer event without moving the real pointer, so the
// handlers' work is called as the handlers call it: with the cell, where, and
// which button.
// ---------------------------------------------------------------------------

auto NoCapture = [] {};

// The tray with three icons, 1 to 3, from `owner`, in the left display's row.
struct ThreeIcons {
    TaskbarRow row;
    HWND owner = nullptr;
    uint64_t a = 0, b = 0, c = 0;
    X::EmbeddedTray* tray = nullptr;
};

ThreeIcons MakeThreeIcons(const Settings& settings) {
    ThreeIcons t;
    ResetEverything(settings);
    t.row = MakeRow(38);
    t.owner = MakeOwner();
    t.a = AddIcon(t.owner, 1, L"alpha", nullptr);
    t.b = AddIcon(t.owner, 2, L"beta", nullptr);
    t.c = AddIcon(t.owner, 3, L"gamma", nullptr);
    t.tray = Embed(t.row);
    if (t.tray) {
        X::RefreshTray(*t.tray);
        Pump();
    }
    g_received.clear();
    return t;
}

wuxc::Border CellOf(X::EmbeddedTray* tray, uint64_t serial) {
    for (const auto& cell : CellsOf(tray)) {
        if (cell && X::SerialOfCell(cell) == serial) {
            return cell;
        }
    }
    return nullptr;
}

std::vector<UINT> ReceivedMouseMessages() {
    std::vector<UINT> messages;
    for (const auto& callback : g_received) {
        messages.push_back(static_cast<UINT>(LOWORD(callback.lParam)));
    }
    return messages;
}

void Test_HoveringTellsTheApplicationAndOpensItsOwnPopup() {
    // DECISIONS 79. The pointer over an icon is WM_MOUSEMOVE to its
    // application; a version 4 icon without NIF_SHOWTIP draws a popup of its
    // own, opened once the pointer rests and closed when it leaves.
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    const uint64_t older = AddIcon(owner, 1, L"older", nullptr);
    const uint64_t popup = AddIcon(owner, 2, L"its own popup", nullptr);
    SetVersion(owner, 2, NOTIFYICON_VERSION_4);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    UINT hoverMs = 400;
    SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverMs, 0);

    g_received.clear();
    X::CellPointerEntered(CellOf(tray, older), POINT{150, 250});
    X::CellPointerMoved(CellOf(tray, older), POINT{151, 250});
    Pump(hoverMs + 200);
    // Hover only: an icon before version 4 has the tray's tooltip.
    CHECK(ReceivedMouseMessages() == (std::vector<UINT>{WM_MOUSEMOVE, WM_MOUSEMOVE}));
    X::CellPointerExited(CellOf(tray, older));

    g_received.clear();
    X::CellPointerEntered(CellOf(tray, popup), POINT{180, 250});
    // Waited for rather than timed: a busy machine runs the hover timer late.
    const ULONGLONG until = GetTickCount64() + 5000;
    while (GetTickCount64() < until &&
           std::none_of(g_received.begin(), g_received.end(), [](Callback const& c) {
               return LOWORD(c.lParam) == NIN_POPUPOPEN;
           })) {
        Pump(20);
    }
    X::CellPointerExited(CellOf(tray, popup));
    Pump();
    CHECK(ReceivedMouseMessages() ==
          (std::vector<UINT>{WM_MOUSEMOVE, NIN_POPUPOPEN, NIN_POPUPCLOSE}));
    for (const auto& callback : g_received) {
        CHECK_EQ(HIWORD(callback.lParam), 2u);
    }

    // Left before it rested: no popup at all.
    g_received.clear();
    X::CellPointerEntered(CellOf(tray, popup), POINT{180, 250});
    X::CellPointerExited(CellOf(tray, popup));
    Pump(hoverMs + 200);
    CHECK(ReceivedMouseMessages() == (std::vector<UINT>{WM_MOUSEMOVE}));
    DestroyWindow(owner);
}

void Test_AClickReachesTheApplicationAsTheButtonItWas() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    SetVersion(t.owner, 3, NOTIFYICON_VERSION_4);
    struct Case {
        X::PointerButton button;
        UINT down, up;
    };
    const Case cases[] = {
        {X::PointerButton::Left, WM_LBUTTONDOWN, WM_LBUTTONUP},
        {X::PointerButton::Right, WM_RBUTTONDOWN, WM_RBUTTONUP},
        {X::PointerButton::Middle, WM_MBUTTONDOWN, WM_MBUTTONUP},
    };
    for (const Case& c : cases) {
        g_received.clear();
        auto cell = CellOf(t.tray, t.a);
        CHECK(X::CellPressed(cell, true, 10));
        X::CellReleased(cell, true, kLeftDisplay, c.button, false, NoCapture);
        Pump();
        // A version 0 icon: (uID, message).
        CHECK_EQ(g_received.size(), 2u);
        if (g_received.size() == 2) {
            CHECK_EQ(g_received[0].wParam, 1u);
            CHECK_EQ(static_cast<UINT>(g_received[0].lParam), c.down);
            CHECK_EQ(static_cast<UINT>(g_received[1].lParam), c.up);
        }
    }

    // A version 4 icon, as Explorer clicks it: NIN_SELECT after a left click.
    g_received.clear();
    auto cell = CellOf(t.tray, t.c);
    X::CellPressed(cell, true, 74);
    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Left, false, NoCapture);
    Pump();
    CHECK(ReceivedMouseMessages() ==
          (std::vector<UINT>{WM_LBUTTONDOWN, WM_LBUTTONUP, NIN_SELECT}));

    // Any other button is nothing.
    g_received.clear();
    X::CellPressed(cell, true, 74);
    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Other, false, NoCapture);
    Pump();
    CHECK(g_received.empty());

    // A double click.
    X::CellDoubleTapped(CellOf(t.tray, t.a));
    Pump();
    CHECK(g_received.size() == 1 &&
          static_cast<UINT>(g_received[0].lParam) == WM_LBUTTONDBLCLK);
    DestroyWindow(t.owner);
}

void Test_ShiftAndAClickSendsTheIconToThePrimaryTray() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto cell = CellOf(t.tray, t.b);
    X::CellPressed(cell, true, 48);
    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Left, true, NoCapture);
    Pump();
    // The mod's move, not a click: the application hears nothing, and the
    // choice is remembered.
    CHECK(g_received.empty());
    CHECK_EQ(CellsOf(t.tray).size(), 2u);
    CHECK(CellOf(t.tray, t.b) == nullptr);
    CHECK(SplitTrayTestHarness::StoredValues()[kPlacementValue].find(L"thing.exe#2=") !=
          std::wstring::npos);
    DestroyWindow(t.owner);
}

void Test_DraggingAlongTheRowReordersItAndIsNotAClick() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto cell = CellOf(t.tray, t.a);
    CHECK(X::CellPressed(cell, true, 16));
    // A shaky click is still a click.
    CHECK(!X::CellDragged(cell, 19, 10));
    CHECK(!X::g_drag.dragging);
    // Past the threshold, it is a drag, and the cell walks along the row.
    CHECK(X::CellDragged(cell, 80, 10));
    CHECK(X::g_drag.dragging);
    CHECK_EQ(SerialAt(t.tray, 0), t.b);
    CHECK_EQ(SerialAt(t.tray, 1), t.c);
    CHECK_EQ(SerialAt(t.tray, 2), t.a);
    // The dragged cell never left the panel, so it kept the pointer.
    CHECK(CellsOf(t.tray)[2] == cell);

    bool released = false;
    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Left, false,
                    [&] { released = true; });
    Pump();
    CHECK(released);
    CHECK(g_received.empty());
    CHECK_WSTR(SplitTrayTestHarness::StoredValues()[kIconOrderValue],
               L"thing.exe#2\nthing.exe#3\nthing.exe#1");
    CHECK(!X::g_drag.pointerDown && !X::g_drag.dragging);

    // A press on a cell that is not in a tray's row cannot be dragged.
    wuxc::Border loose;
    CHECK(!X::CellPressed(loose, true, 0));
    CHECK(!X::CellDragged(loose, 80, 10));
    X::CellReleased(loose, true, kLeftDisplay, X::PointerButton::Left, false, NoCapture);
    DestroyWindow(t.owner);
}

void Test_DraggingOffTheRowSendsTheIconToThePrimaryTray() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto cell = CellOf(t.tray, t.b);
    X::CellPressed(cell, true, 48);
    CHECK(X::CellDragged(cell, 60, 10));
    // Clear of the row, the icon dims, and stays where it is in the row, even
    // over another icon's slot.
    CHECK(X::CellDragged(cell, 5, -40));
    CHECK(X::g_drag.outside);
    CHECK(cell.Opacity() < 0.5);
    CHECK_EQ(SerialAt(t.tray, 1), t.b);
    CHECK(X::CellDragged(cell, 90, 80));
    CHECK_EQ(SerialAt(t.tray, 1), t.b);
    // Back over the row, it is a reorder again.
    CHECK(X::CellDragged(cell, 60, 10));
    CHECK(!X::g_drag.outside && cell.Opacity() == 1.0);
    CHECK(X::CellDragged(cell, 60, 80));

    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Left, false, NoCapture);
    Pump();
    CHECK(g_received.empty());
    CHECK(CellOf(t.tray, t.b) == nullptr);
    CHECK_EQ(CellsOf(t.tray).size(), 2u);
    CHECK(SplitTrayTestHarness::StoredValues()[kPlacementValue].find(L"thing.exe#2=") !=
          std::wstring::npos);
    DestroyWindow(t.owner);
}

void Test_ADragCutShortKeepsItsOrderAndIsNotAClick() {
    // Capture can be taken away mid-drag. The order so far is kept, and the
    // release after it is not a click on whatever the icon sat over.
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto cell = CellOf(t.tray, t.a);
    X::CellPressed(cell, true, 16);
    X::CellDragged(cell, 48, 10);
    X::CellCaptureLost(cell);
    CHECK_WSTR(SplitTrayTestHarness::StoredValues()[kIconOrderValue],
               L"thing.exe#2\nthing.exe#1\nthing.exe#3");
    CHECK(X::g_drag.suppressClick);
    CHECK(!X::g_drag.pointerDown && !X::g_drag.dragging);

    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Left, false, NoCapture);
    Pump();
    CHECK(g_received.empty());
    CHECK(!X::g_drag.suppressClick);

    // Capture lost with no drag under way changes nothing.
    X::CellPressed(cell, true, 48);
    X::CellCaptureLost(cell);
    CHECK(!X::g_drag.suppressClick);
    DestroyWindow(t.owner);
}

// A window standing in for Explorer's Shell_TrayWnd, which the taskbar's thread
// is asked to refresh through.
HWND MakeShellStandIn() {
    return CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                           nullptr, nullptr);
}

bool TakePosted(HWND window, UINT message) {
    MSG msg;
    return PeekMessageW(&msg, window, message, message, PM_REMOVE) != FALSE;
}

void Test_ARefreshThatWaitedForTheReleaseIsAskedForThen() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    HWND shell = MakeShellStandIn();
    g_shellTrayWnd.store(shell);
    auto cell = CellOf(t.tray, t.a);
    X::CellPressed(cell, true, 16);
    AddIcon(t.owner, 4, L"delta", nullptr);
    X::RefreshEmbeddedTray();
    CHECK(X::g_refreshPending);
    CHECK_EQ(CellsOf(t.tray).size(), 3u);

    // Posted, not run inside the cell's own handler.
    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Left, false, NoCapture);
    CHECK(TakePosted(shell, GetXamlRefreshMessage()));
    CHECK_EQ(CellsOf(t.tray).size(), 3u);
    X::RefreshEmbeddedTray();
    CHECK_EQ(CellsOf(t.tray).size(), 4u);
    g_shellTrayWnd.store(nullptr);
    DestroyWindow(shell);
    DestroyWindow(t.owner);
}

void Test_TheKeyboardsMenuKeyIsTheApplicationsMenuOnce() {
    // DECISIONS 80. Shift+F10 or the menu key: right clicks for an older icon,
    // WM_CONTEXTMENU from version 3. The key and ContextRequested can both
    // report one press, and the second is not sent again.
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    SetVersion(t.owner, 3, NOTIFYICON_VERSION_4);
    X::SendKeyboardMenu(t.a);
    X::SendKeyboardMenu(t.a);
    Pump();
    CHECK(ReceivedMouseMessages() == (std::vector<UINT>{WM_RBUTTONDOWN, WM_RBUTTONUP}));

    g_received.clear();
    X::SendKeyboardMenu(t.c);
    Pump();
    CHECK(ReceivedMouseMessages() == (std::vector<UINT>{WM_CONTEXTMENU}));
    DestroyWindow(t.owner);
}

void Test_TheChevronOpensTheOverflowWithItsIcons() {
    Settings s = EmbeddingSettings();
    s.maxVisibleIcons = 1;
    ThreeIcons t = MakeThreeIcons(s);
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto chevron = CellsOf(t.tray)[0];
    CHECK_EQ(X::SerialOfCell(chevron), 0ull);
    X::ChevronReleased(chevron, kLeftDisplay, false);
    Pump();
    CHECK(X::g_overflowFlyout != nullptr);
    auto grid = X::g_overflowFlyout ? X::g_overflowFlyout.Content()
                                          .try_as<wuxc::VariableSizedWrapGrid>()
                                    : nullptr;
    CHECK(grid != nullptr);
    if (!grid) {
        return;
    }
    // The icons behind the chevron, in square cells.
    CHECK_EQ(grid.Children().Size(), 2u);
    std::vector<uint64_t> serials;
    for (auto child : grid.Children()) {
        auto cell = child.try_as<wuxc::Border>();
        CHECK(cell && cell.Width() == 40 && cell.Height() == 40);
        serials.push_back(cell ? X::SerialOfCell(cell) : 0);
    }
    CHECK(serials == (std::vector<uint64_t>{t.b, t.c}));

    // A click there reaches the application, as on the bar.
    if (grid.Children().Size() == 0) {
        return;
    }
    // Not a drag there: the popup has no row to drag along.
    auto inPopup = grid.Children().GetAt(0).try_as<wuxc::Border>();
    CHECK(!X::CellPressed(inPopup, false, 0));
    X::CellReleased(inPopup, false, kLeftDisplay, X::PointerButton::Left, false, NoCapture);
    Pump();
    CHECK(ReceivedMouseMessages() == (std::vector<UINT>{WM_LBUTTONDOWN, WM_LBUTTONUP}));
    CHECK(g_received.size() == 2 && g_received[0].wParam == 2);

    // With nothing behind it, the chevron does nothing.
    s.maxVisibleIcons = 8;
    ApplySettings(s);
    X::RefreshTray(*t.tray);
    CHECK(t.tray->hiddenEntries.empty());
    X::g_overflowFlyout.Content(nullptr);
    X::ChevronReleased(chevron, kLeftDisplay, false);
    CHECK(X::g_overflowFlyout.Content() == nullptr);
    DestroyWindow(t.owner);
}

// ---------------------------------------------------------------------------
// The mod's menu (BuildTrayContextMenu)
// ---------------------------------------------------------------------------

std::vector<std::wstring> TextsOf(wuxc::MenuFlyout const& menu) {
    std::vector<std::wstring> texts;
    for (auto item : menu.Items()) {
        if (auto plain = item.try_as<wuxc::MenuFlyoutItem>()) {
            texts.push_back(plain.Text().c_str());
        } else if (auto sub = item.try_as<wuxc::MenuFlyoutSubItem>()) {
            texts.push_back(std::wstring(L"> ") + sub.Text().c_str());
        } else {
            texts.push_back(L"---");
        }
    }
    return texts;
}

wuxc::MenuFlyoutItem ItemCalled(wuxc::MenuFlyout const& menu, std::wstring_view text) {
    for (auto item : menu.Items()) {
        if (auto plain = item.try_as<wuxc::MenuFlyoutItem>()) {
            if (std::wstring_view(plain.Text()) == text) {
                return plain;
            }
        }
    }
    return nullptr;
}

wuxc::MenuFlyoutSubItem SubmenuOf(wuxc::MenuFlyout const& menu) {
    for (auto item : menu.Items()) {
        if (auto sub = item.try_as<wuxc::MenuFlyoutSubItem>()) {
            return sub;
        }
    }
    return nullptr;
}

// Chosen as a screen reader chooses it.
void Choose(wuxc::MenuFlyoutItem const& item) {
    wuxap::MenuFlyoutItemAutomationPeer peer(item);
    peer.as<wuxapr::IInvokeProvider>().Invoke();
    Pump(50);
}

void Test_TheModsMenuOnAnIconOffersEveryChoice() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto menu = X::BuildTrayContextMenu(t.a, kLeftDisplay);
    const std::wstring moveTo = L"Move to " + TrayLabel(1);
    CHECK(TextsOf(menu) == (std::vector<std::wstring>{
                               moveTo, L"Hide in the overflow menu",
                               L"> Move an icon to this tray", L"---", L"Arrange icons\u2026",
                               L"Reset icon order", L"Show every hidden icon",
                               L"Reset moved icons"}));

    // Hidden, then offered back.
    Choose(ItemCalled(menu, L"Hide in the overflow menu"));
    CHECK(IsIconHidden(L"thing.exe#1"));
    CHECK_EQ(SerialAt(t.tray, 0), 0ull);
    menu = X::BuildTrayContextMenu(t.a, kLeftDisplay);
    CHECK(ItemCalled(menu, L"Show on the tray") != nullptr);
    Choose(ItemCalled(menu, L"Show every hidden icon"));
    CHECK(!IsIconHidden(L"thing.exe#1"));

    // Sent to the primary tray, and the moves forgotten again.
    Choose(ItemCalled(menu, moveTo));
    CHECK(CellOf(t.tray, t.a) == nullptr);
    CHECK(SplitTrayTestHarness::StoredValues()[kPlacementValue].find(L"thing.exe#1=") !=
          std::wstring::npos);
    Choose(ItemCalled(menu, L"Reset moved icons"));
    CHECK(SplitTrayTestHarness::StoredValues()[kPlacementValue].empty());

    // The order forgotten.
    SplitTrayTestHarness::StoredValues()[kIconOrderValue] = L"thing.exe#3";
    X::g_iconOrder = {L"thing.exe#3"};
    X::g_iconOrderLoaded = true;
    Choose(ItemCalled(menu, L"Reset icon order"));
    CHECK(X::g_iconOrder.empty());
    CHECK(SplitTrayTestHarness::StoredValues()[kIconOrderValue].empty());
    DestroyWindow(t.owner);
}

void Test_TheEmptyTraysMenuBringsIconsHere() {
    // The handle's menu: no icon to move or hide, but every icon elsewhere can
    // be brought here (AppendMoveHereItems).
    Settings s = EmbeddingSettings();
    s.defaultTray = Destination::Primary;
    ResetEverything(s);
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    AddIcon(owner, 1, L"elsewhere", nullptr);
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    X::RefreshTray(*tray);
    CHECK_EQ(SerialAt(tray, 0), 0ull);

    auto menu = X::BuildTrayContextMenu(0, kLeftDisplay);
    CHECK(ItemCalled(menu, L"Hide in the overflow menu") == nullptr);
    auto submenu = SubmenuOf(menu);
    CHECK(submenu && submenu.Items().Size() == 1);
    if (!submenu || submenu.Items().Size() != 1) {
        return;
    }
    auto bring = submenu.Items().GetAt(0).try_as<wuxc::MenuFlyoutItem>();
    CHECK(bring && bring.Text() == L"elsewhere");
    Choose(bring);
    CHECK_EQ(CellsOf(tray).size(), 1u);
    CHECK(SerialAt(tray, 0) != 0);

    // With every icon here, there is nothing to bring.
    menu = X::BuildTrayContextMenu(0, kLeftDisplay);
    submenu = SubmenuOf(menu);
    auto none = submenu ? submenu.Items().GetAt(0).try_as<wuxc::MenuFlyoutItem>() : nullptr;
    CHECK(none && none.Text() == L"Every icon is already here" && !none.IsEnabled());
    DestroyWindow(owner);
}

// ---------------------------------------------------------------------------
// The taskbar's thread, asked from elsewhere
// ---------------------------------------------------------------------------

void Test_AChangeToTheStoreRedrawsTheTray() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    HWND owner = MakeOwner();
    // Waiting to embed, until it is in the taskbar.
    CHECK(X::AnyDisplayTrayWaitingToEmbed());
    X::EmbeddedTray* tray = Embed(r);
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    CHECK(!X::AnyDisplayTrayWaitingToEmbed());

    AddIcon(owner, 1, L"alpha", nullptr);
    X::OnIconStoreChanged();
    CHECK_EQ(CellsOf(tray).size(), 1u);
    CHECK(SerialAt(tray, 0) != 0);

    // From another thread it is posted to the taskbar's window.
    HWND shell = MakeShellStandIn();
    g_shellTrayWnd.store(shell);
    X::RequestEmbeddedRefresh();
    CHECK(TakePosted(shell, GetXamlRefreshMessage()));
    g_shellTrayWnd.store(nullptr);
    DestroyWindow(shell);

    // With embedding off, nothing waits to embed.
    Settings off = EmbeddingSettings();
    off.embedInTaskbar = false;
    ApplySettings(off);
    CHECK(!X::AnyDisplayTrayWaitingToEmbed());
    DestroyWindow(owner);
}


bool Logged(std::wstring_view text) {
    for (const auto& line : SplitTrayTestHarness::LogSnapshot()) {
        if (line.find(text) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

void Test_ShiftAndARightClickIsTheModsMenuNotTheApplications() {
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    auto cell = CellOf(t.tray, t.a);
    X::CellPressed(cell, true, 16);
    X::CellReleased(cell, true, kLeftDisplay, X::PointerButton::Right, true, NoCapture);
    Pump();
    CHECK(g_received.empty());
    DestroyWindow(t.owner);
}

void Test_AnElementLoadedOnATraysTaskbarAnchorsTheTrayThere() {
    // The IconView constructor hook hands over each tray element as it loads
    // (OnTrayIconViewLoaded). One on a display's taskbar is where that
    // display's tray goes; the slot it is in is named in the log, and with
    // dumpXamlTree the tray frame's tree is printed once.
    Settings s = EmbeddingSettings();
    s.dumpXamlTree = true;
    ResetEverything(s);
    TaskbarRow r = MakeRow(38);
    r.wrapper.Name(L"NotificationCenterButton");
    Pump();
    X::SyncEmbeddedTrays();
    X::EmbeddedTray* tray = LeftTray();
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    tray->taskbarWnd = g_island.host;
    tray->root = g_island.root.XamlRoot();

    X::OnTrayIconViewLoaded(r.anchor);
    CHECK(tray->active);
    CHECK_EQ(r.row.Children().Size(), 4u);
    CHECK(r.row.Children().GetAt(1) == PanelOf(tray));
    CHECK(Logged(L"in NotificationCenterButton"));
    CHECK(Logged(L"target taskbar tray subtree"));
}

void Test_AnElementElsewhereIsLeftAlone() {
    ResetEverything(EmbeddingSettings());
    TaskbarRow r = MakeRow(38);
    X::SyncEmbeddedTrays();
    X::EmbeddedTray* tray = LeftTray();
    CHECK(tray != nullptr);
    if (!tray) {
        return;
    }
    // Not in any tree yet: no root to tell which taskbar it is on.
    wuxc::Button unloaded;
    X::OnTrayIconViewLoaded(unloaded);
    CHECK(!tray->active);

    // On a taskbar that is not the tray's: this display has none the mod can
    // find (FindTaskbarWindowOn), so the element is the primary taskbar's.
    X::OnTrayIconViewLoaded(r.anchor);
    CHECK(!tray->active);
    CHECK_EQ(r.row.Children().Size(), 3u);

    // And the timer's attempt finds no taskbar either.
    X::TryAttachEmbeddedTray();
    CHECK(!tray->active);
}

// A second taskbar is the mod's only in its own process: its objects are read
// from this process's memory (DECISIONS 92). The suite runs on the desktop it
// was started on, where Explorer's second taskbars are another process's -
// looked at, not sent anything. A machine with one display has none.
void Test_AnotherProcesssSecondTaskbarIsNotFound() {
    HWND explorers = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
    if (!explorers) {
        printf("      no second taskbar on this desktop: nothing to look past\n");
        return;
    }
    DWORD owner = 0;
    GetWindowThreadProcessId(explorers, &owner);
    CHECK(owner != GetCurrentProcessId());
    HMONITOR monitor = MonitorFromWindow(explorers, MONITOR_DEFAULTTONULL);
    CHECK(monitor != nullptr);
    CHECK(X::FindTaskbarWindowOn(monitor) == nullptr);
}

void Test_WithoutExplorersSymbolsNothingIsHooked() {
    // A test process has no taskbar.dll or SystemTray.dll: nothing resolves,
    // nothing is hooked, and the trays float rather than embed.
    ResetEverything(EmbeddingSettings());
    X::EnsureTaskbarXamlHooked();
    CHECK(!X::g_taskbarSymbolsHooked.load());
    CHECK(!X::g_systemTraySymbolsHooked.load());
}

void Test_FocusGoesToTheCellOfTheIconAsked() {
    // NIM_SETFOCUS gives the keyboard back to an icon (DECISIONS 80).
    ThreeIcons t = MakeThreeIcons(EmbeddingSettings());
    CHECK(t.tray != nullptr);
    if (!t.tray) {
        return;
    }
    X::FocusIconCell(t.b);
    Pump();
    auto focused = wux::Input::FocusManager::GetFocusedElement(g_island.root.XamlRoot());
    CHECK(focused && focused == X::FaceOfCell(CellOf(t.tray, t.b)));
    // An icon the mod does not draw, and no icon at all, are no-ops.
    X::FocusIconCell(0xDEAD);
    X::FocusIconCell(0);
    DestroyWindow(t.owner);
}

struct TestRunner {
    void Run(const char* name, void (*fn)()) {
        g_currentTest = name;
        const int before = g_failures;
        try {
            fn();
        } catch (winrt::hresult_error const& error) {
            g_failures++;
            printf("  FAIL  %s: threw %08X %ls\n", name,
                   static_cast<unsigned>(error.code()), error.message().c_str());
        }
        printf("  %s  %s\n", (g_failures == before) ? "ok  " : "FAIL", name);
    }
};

}  // namespace

int main() {
    printf("split-tray XAML tests (a hosted island, the mod's section 10)\n\n");
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (!OpenIsland()) {
        printf("\n0 checks, 1 failure\n");
        return 1;
    }

    TestRunner runner;
    printf("where the tray goes\n");
    runner.Run("the tray goes into the row, before the anchor's wrapper",
               Test_TheTrayGoesIntoTheRowBeforeTheAnchorsWrapper);
    runner.Run("with no StackPanel above, any panel will do",
               Test_WithNoStackPanelAboveAnyPanelWillDo);
    runner.Run("cells are sized from the anchor's height",
               Test_CellsAreSizedFromTheAnchorsHeight);

    printf("\nwhat it draws\n");
    runner.Run("an empty tray shows its handle", Test_AnEmptyTrayShowsItsHandle);
    runner.Run("each icon is a cell, named and with its tooltip",
               Test_EachIconIsACellNamedAndWithItsTooltip);
    runner.Run("tooltips switched off leave the name", Test_TooltipsSwitchedOffLeaveTheName);
    runner.Run("past the limit the rest go behind the chevron, which comes first",
               Test_PastTheLimitTheRestGoBehindTheChevronWhichComesFirst);
    runner.Run("an icon the user hid is behind the chevron however much room",
               Test_AnIconTheUserHidIsBehindTheChevronHoweverMuchRoom);

    printf("\nupdates in place\n");
    runner.Run("a new tooltip or picture is drawn in place",
               Test_ANewTooltipOrPictureIsDrawnInPlace);
    runner.Run("a refresh waits while the pointer is down",
               Test_ARefreshWaitsWhileThePointerIsDown);

    printf("\nthe order the user chose\n");
    runner.Run("the saved order decides the row", Test_TheSavedOrderDecidesTheRow);
    runner.Run("a reordered row is saved and read back",
               Test_AReorderedRowIsSavedAndReadBack);

    printf("\nwhere an icon is, and invoking it\n");
    runner.Run("an icon is where its cell is on screen", Test_AnIconIsWhereItsCellIsOnScreen);
    runner.Run("invoking a cell is the keyboard's selection to its application",
               Test_InvokingACellIsTheKeyboardsSelectionToItsApplication);

    printf("\ntaking it back out\n");
    runner.Run("a tray dropped by a rebuild is put back", Test_ATrayDroppedByARebuildIsPutBack);
    runner.Run("taking the tray out leaves the row as it was",
               Test_TakingTheTrayOutLeavesTheRowAsItWas);
    runner.Run("embedding off, or the display gone, takes the tray out",
               Test_EmbeddingOffOrTheDisplayGoneTakesTheTrayOut);
    runner.Run("unloading takes every panel out", Test_UnloadingTakesEveryPanelOut);
    runner.Run("the attach walk leaves a tree with no tray of Explorer's alone",
               Test_TheAttachWalkLeavesATreeWithNoTrayOfExplorersAlone);

    printf("\nthe pointer on a cell\n");
    runner.Run("hovering tells the application, and opens its own popup",
               Test_HoveringTellsTheApplicationAndOpensItsOwnPopup);
    runner.Run("a click reaches the application as the button it was",
               Test_AClickReachesTheApplicationAsTheButtonItWas);
    runner.Run("Shift and a click sends the icon to the primary tray",
               Test_ShiftAndAClickSendsTheIconToThePrimaryTray);
    runner.Run("dragging along the row reorders it, and is not a click",
               Test_DraggingAlongTheRowReordersItAndIsNotAClick);
    runner.Run("dragging off the row sends the icon to the primary tray",
               Test_DraggingOffTheRowSendsTheIconToThePrimaryTray);
    runner.Run("a drag cut short keeps its order, and is not a click",
               Test_ADragCutShortKeepsItsOrderAndIsNotAClick);
    runner.Run("a refresh that waited for the release is asked for then",
               Test_ARefreshThatWaitedForTheReleaseIsAskedForThen);
    runner.Run("the keyboard's menu key is the application's menu, once",
               Test_TheKeyboardsMenuKeyIsTheApplicationsMenuOnce);
    runner.Run("the chevron opens the overflow, with its icons",
               Test_TheChevronOpensTheOverflowWithItsIcons);

    printf("\nthe mod's menu\n");
    runner.Run("the mod's menu on an icon offers every choice",
               Test_TheModsMenuOnAnIconOffersEveryChoice);
    runner.Run("the empty tray's menu brings icons here",
               Test_TheEmptyTraysMenuBringsIconsHere);

    printf("\nthe taskbar's thread, asked from elsewhere\n");
    runner.Run("a change to the store redraws the tray", Test_AChangeToTheStoreRedrawsTheTray);
    runner.Run("Shift and a right click is the mod's menu, not the application's",
               Test_ShiftAndARightClickIsTheModsMenuNotTheApplications);
    runner.Run("focus goes to the cell of the icon asked",
               Test_FocusGoesToTheCellOfTheIconAsked);

    printf("\nthe way in\n");
    runner.Run("an element loaded on a tray's taskbar anchors the tray there",
               Test_AnElementLoadedOnATraysTaskbarAnchorsTheTrayThere);
    runner.Run("an element elsewhere is left alone", Test_AnElementElsewhereIsLeftAlone);
    runner.Run("another process's second taskbar is not found",
               Test_AnotherProcesssSecondTaskbarIsNotFound);
    runner.Run("without Explorer's symbols nothing is hooked",
               Test_WithoutExplorersSymbolsNothingIsHooked);

    ResetEverything(EmbeddingSettings());
    CloseIsland();
    printf("\n%d checks, %d failure%s\n", g_checks, g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
