// ============================================================================
// Module:  tests/regression/split_tray_tests.cpp
// Purpose: Regression tests for the pure logic of src/split-tray.wh.cpp. The mod
//          source is compiled directly into this executable against the stub
//          Windhawk headers in tests/harness, so these tests exercise the
//          shipped code rather than a copy of it.
//
// Coverage anchors (one per defect this project actually shipped):
//   * The mod must decode the Shell_TrayWnd WM_COPYDATA record, because hooking
//     Shell_NotifyIconW inside explorer.exe only ever sees Explorer's own icons.
//   * The secondary tray window must land inside the chosen monitor, including
//     monitors at negative coordinates (the earlier build hardcoded "to the
//     right" and drew itself off-screen).
//
// Build/run: tools/run-tests.ps1
// ============================================================================

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <type_traits>

// The mod, compiled as-is. tests/harness shadows the real Windhawk headers.
#include "../../src/split-tray.wh.cpp"

#include "golden_payloads.h"

using namespace SplitTray;

// ---------------------------------------------------------------------------
// Minimal test scaffolding
// ---------------------------------------------------------------------------

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

#define CHECK_WSTR(actual, expected)                                     \
    do {                                                                 \
        std::wstring a_ = (actual);                                      \
        std::wstring e_ = (expected);                                    \
        g_checks++;                                                      \
        if (a_ != e_) {                                                  \
            g_failures++;                                                \
            printf("  FAIL  %s:%d  %s == L\"%ls\" (got L\"%ls\")\n",     \
                   g_currentTest, __LINE__, #actual, e_.c_str(),         \
                   a_.c_str());                                          \
        }                                                                \
    } while (0)

struct TestRunner {
    void Run(const char* name, void (*fn)()) {
        g_currentTest = name;
        const int before = g_failures;
        fn();
        printf("  %s  %s\n", (g_failures == before) ? "ok  " : "FAIL", name);
    }
};

Settings DefaultSettings() {
    Settings s;
    s.defaultTray = Destination::Primary;
    s.corner = Corner::BottomRight;
    s.offsetX = 8;
    s.offsetY = 8;
    s.iconSize = 16;
    s.cellSize = 28;
    s.maxColumns = 12;
    s.opacity = 235;
    return s;
}

// The real monitor layout on the development machine at the time the earlier
// build placed its window at x=3620: the secondary display is to the *left*, at
// a negative x. Kept as a literal so the test does not depend on the hardware it
// happens to run on.
constexpr RECT kSecondaryWorkArea = {-1920, 0, -384, 912};  // 1536 x 912
constexpr RECT kPrimaryWorkArea = {0, 0, 1920, 1032};

// ---------------------------------------------------------------------------
// Section 1 - wire protocol
// ---------------------------------------------------------------------------

void Test_ParsesRealUnicodeV4Payload() {
    TrayNotification n;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4,
                                sizeof(kPayloadUnicodeV4), &n));
    CHECK_EQ(n.message, static_cast<DWORD>(NIM_ADD));
    CHECK_EQ(reinterpret_cast<ULONG_PTR>(n.ownerWnd),
             static_cast<ULONG_PTR>(kProbeOwnerWnd));
    CHECK_EQ(n.uID, kProbeUID);
    CHECK_EQ(n.callbackMessage, kProbeCallbackMsg);
    CHECK_EQ(reinterpret_cast<ULONG_PTR>(n.icon),
             static_cast<ULONG_PTR>(kProbeIconHandle));
    CHECK_EQ(n.flags, static_cast<UINT>(NIF_MESSAGE | NIF_ICON | NIF_TIP));
    CHECK_WSTR(n.tip, L"PROBE-TIP-W");
    CHECK(!n.hasGuid);
    // The exe path rides along in the record, which is how the mod knows which
    // process an icon belongs to without opening a process handle.
    CHECK(n.exePath.find(L"wire_probe.exe") != std::wstring::npos);
}

void Test_ParsesGuidIdentifiedIcon() {
    TrayNotification n;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4Guid,
                                sizeof(kPayloadUnicodeV4Guid), &n));
    CHECK(n.hasGuid);
    CHECK_EQ(n.flags & NIF_GUID, static_cast<UINT>(NIF_GUID));
    const GUID expected = {0x11223344,
                           0x5566,
                           0x7788,
                           {0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00}};
    CHECK(memcmp(&n.guid, &expected, sizeof(GUID)) == 0);
}

void Test_ShellNormalisesLegacyAndAnsiCallers() {
    // A V1 caller (cbSize 168) and an ANSI caller both arrive in the one 1484
    // byte UTF-16 layout, so the mod needs exactly one parser.
    TrayNotification legacy;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV1,
                                sizeof(kPayloadUnicodeV1), &legacy));
    CHECK_WSTR(legacy.tip, L"PROBE-TIP-W");
    CHECK_EQ(legacy.uID, kProbeUID);

    TrayNotification ansi;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadAnsiV4,
                                sizeof(kPayloadAnsiV4), &ansi));
    CHECK_WSTR(ansi.tip, L"PROBE-ANSI-TIP");
    CHECK_EQ(ansi.uID, kProbeUID);
}

void Test_ParsesSetVersion() {
    TrayNotification n;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadSetVersion4,
                                sizeof(kPayloadSetVersion4), &n));
    CHECK_EQ(n.message, static_cast<DWORD>(NIM_SETVERSION));
    CHECK_EQ(n.version, static_cast<DWORD>(NOTIFYICON_VERSION_4));
}

void Test_RejectsNonTrayCopyData() {
    TrayNotification n;
    // Appbar traffic and in-proc load requests use other dwData values and must
    // pass straight through to Explorer.
    CHECK(!ParseTrayNotification(0, kPayloadUnicodeV4, sizeof(kPayloadUnicodeV4), &n));
    CHECK(!ParseTrayNotification(3, kPayloadUnicodeV4, sizeof(kPayloadUnicodeV4), &n));

    // Wrong signature.
    unsigned char corrupted[sizeof(kPayloadUnicodeV4)];
    memcpy(corrupted, kPayloadUnicodeV4, sizeof(corrupted));
    corrupted[0] ^= 0xFF;
    CHECK(!ParseTrayNotification(kTrayCopyDataId, corrupted, sizeof(corrupted), &n));

    CHECK(!ParseTrayNotification(kTrayCopyDataId, nullptr, 1484, &n));
    CHECK(!ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4, 8, &n));
}

void Test_ARecordOfAnotherShapeIsLeftToExplorer() {
    // DECISIONS 57, refining 3. A future Windows could change the record. The
    // offsets were captured from this one, and reading a different record at
    // them decodes fields that are not there - an identity that is not the
    // icon's, or a folded record that replays garbage into Explorer. So only
    // the captured shape is taken; anything else passes through untouched.
    TrayNotification n;
    CHECK(!ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4,
                                 wire::kMinUsableSize, &n));
    CHECK(!ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4,
                                 sizeof(kPayloadUnicodeV4) - 4, &n));

    // The right length with a different NOTIFYICONDATA inside it.
    std::vector<BYTE> resized(kPayloadUnicodeV4,
                              kPayloadUnicodeV4 + sizeof(kPayloadUnicodeV4));
    const DWORD otherCbSize = wire::kExpectedNidCbSize - 4;
    memcpy(resized.data() + wire::kNidCbSize, &otherCbSize, sizeof(otherCbSize));
    CHECK(!ParseTrayNotification(kTrayCopyDataId, resized.data(), resized.size(), &n));

    // A longer record, as an extended structure would make it.
    std::vector<BYTE> longer(kPayloadUnicodeV4,
                             kPayloadUnicodeV4 + sizeof(kPayloadUnicodeV4));
    longer.resize(longer.size() + 16);
    CHECK(!ParseTrayNotification(kTrayCopyDataId, longer.data(), longer.size(), &n));

    // The captured record still parses.
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4,
                                sizeof(kPayloadUnicodeV4), &n));
}

void Test_PayloadWithMessageRewritesOnlyTheMessage() {
    std::vector<BYTE> source(kPayloadUnicodeV4,
                             kPayloadUnicodeV4 + sizeof(kPayloadUnicodeV4));
    std::vector<BYTE> deleted = PayloadWithMessage(source, NIM_DELETE);
    CHECK_EQ(deleted.size(), source.size());
    CHECK_EQ(ReadDword(deleted.data(), wire::kMessage), static_cast<DWORD>(NIM_DELETE));
    // Every other byte is untouched, so a replay carries the original identity.
    CHECK(memcmp(deleted.data(), source.data(), wire::kMessage) == 0);
    CHECK(memcmp(deleted.data() + wire::kMessage + 4, source.data() + wire::kMessage + 4,
                 source.size() - wire::kMessage - 4) == 0);
}

// Shell_NotifyIconGetRect's two messages, as the real shell32 sent them to the
// wire probe (tests/probe/probe-rect-output-26100.txt): the size is asked for
// first, then the position. Owner 0x018A0546, uID 4242.
const unsigned char kRectQuerySize[] = {
    0x23, 0x34, 0x75, 0x34, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x46, 0x05, 0x8A, 0x01, 0x92, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const unsigned char kRectQueryPositionByGuid[] = {
    0x23, 0x34, 0x75, 0x34, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x46, 0x05, 0x8A, 0x01, 0x92, 0x10, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11,
    0x66, 0x55, 0x88, 0x77, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
};

void Test_ParsesAnIconRectQuery() {
    IconRectQuery q;
    CHECK(ParseIconRectQuery(kIconRectCopyDataId, kRectQuerySize, sizeof(kRectQuerySize),
                             &q));
    CHECK_EQ(q.part, kIconRectSize);
    CHECK_EQ(reinterpret_cast<ULONG_PTR>(q.ownerWnd), static_cast<ULONG_PTR>(0x018A0546));
    CHECK_EQ(q.uID, 4242u);
    CHECK(IsEmptyGuid(q.guid));

    IconRectQuery byGuid;
    CHECK(ParseIconRectQuery(kIconRectCopyDataId, kRectQueryPositionByGuid,
                             sizeof(kRectQueryPositionByGuid), &byGuid));
    CHECK_EQ(byGuid.part, kIconRectPosition);
    CHECK_EQ(byGuid.guid.Data1, 0x11223344ul);
    CHECK_EQ(byGuid.guid.Data2, 0x5566);
    CHECK_EQ(byGuid.guid.Data3, 0x7788);
    CHECK_EQ(byGuid.guid.Data4[0], 0x99);
    CHECK_EQ(byGuid.guid.Data4[6], 0xFF);
}

void Test_RejectsWhatIsNotAnIconRectQuery() {
    IconRectQuery q;
    // A tray notification, and a rect query sent under any other dwData.
    CHECK(!ParseIconRectQuery(kIconRectCopyDataId, kPayloadUnicodeV4,
                              sizeof(kPayloadUnicodeV4), &q));
    CHECK(!ParseIconRectQuery(kTrayCopyDataId, kRectQuerySize, sizeof(kRectQuerySize), &q));

    unsigned char copy[sizeof(kRectQuerySize)];
    memcpy(copy, kRectQuerySize, sizeof(copy));
    copy[0] ^= 0xFF;  // signature
    CHECK(!ParseIconRectQuery(kIconRectCopyDataId, copy, sizeof(copy), &q));

    memcpy(copy, kRectQuerySize, sizeof(copy));
    copy[4] = 3;  // a part this mod does not know how to answer
    CHECK(!ParseIconRectQuery(kIconRectCopyDataId, copy, sizeof(copy), &q));

    // Truncated before the uID, or no data at all.
    CHECK(!ParseIconRectQuery(kIconRectCopyDataId, kRectQuerySize, 0x14, &q));
    CHECK(!ParseIconRectQuery(kIconRectCopyDataId, nullptr, sizeof(kRectQuerySize), &q));
}

void Test_IconRectReplyIsWhatShell32Reads() {
    // The probe answered the size with (111, 222) and the position with
    // (333, 444), and shell32 handed its caller {333, 444, 444, 666}.
    const RECT probed = {333, 444, 444, 666};
    CHECK_EQ(IconRectReply(probed, kIconRectSize), MAKELONG(111, 222));
    CHECK_EQ(IconRectReply(probed, kIconRectPosition), MAKELONG(333, 444));

    // Signed halves: the secondary monitor here is left of the primary one.
    const RECT left = {-1768, 1080, -1736, 1120};
    const LRESULT position = IconRectReply(left, kIconRectPosition);
    CHECK_EQ(GET_X_LPARAM(position), -1768);
    CHECK_EQ(GET_Y_LPARAM(position), 1080);
    const LRESULT size = IconRectReply(left, kIconRectSize);
    CHECK_EQ(GET_X_LPARAM(size), 32);
    CHECK_EQ(GET_Y_LPARAM(size), 40);

    // A size of zero is how the tray says "no such icon", so an empty rect
    // must never be reported as found.
    CHECK_EQ(IconRectReply(RECT{10, 10, 10, 30}, kIconRectSize), 0);
    CHECK_EQ(IconRectReply(RECT{10, 30, 20, 10}, kIconRectSize), 0);
}

// ---------------------------------------------------------------------------
// Section 2 - settings parsing
// ---------------------------------------------------------------------------

void Test_ParsesDestinationAndCorner() {
    CHECK(ParseDestination(L"primary", Destination::Both) == Destination::Primary);
    CHECK(ParseDestination(L"secondary", Destination::Both) == Destination::Secondary);
    CHECK(ParseDestination(L"both", Destination::Primary) == Destination::Both);
    CHECK(ParseDestination(L"nonsense", Destination::Both) == Destination::Both);
    CHECK(ParseDestination(nullptr, Destination::Secondary) == Destination::Secondary);

    CHECK(ParseCorner(L"bottomRight") == Corner::BottomRight);
    CHECK(ParseCorner(L"bottomLeft") == Corner::BottomLeft);
    CHECK(ParseCorner(L"topRight") == Corner::TopRight);
    CHECK(ParseCorner(L"topLeft") == Corner::TopLeft);
    CHECK(ParseCorner(L"garbage") == Corner::BottomRight);
    CHECK(ParseCorner(nullptr) == Corner::BottomRight);
}

void Test_ParsesHexColourAndRejectsGarbage() {
    CHECK_EQ(ParseHexColor(L"FF8000", 0), RGB(0xFF, 0x80, 0x00));
    CHECK_EQ(ParseHexColor(L"#ff8000", 0), RGB(0xFF, 0x80, 0x00));
    CHECK_EQ(ParseHexColor(L"20", RGB(1, 2, 3)), RGB(1, 2, 3));
    CHECK_EQ(ParseHexColor(L"zzzzzz", RGB(1, 2, 3)), RGB(1, 2, 3));
    CHECK_EQ(ParseHexColor(nullptr, RGB(1, 2, 3)), RGB(1, 2, 3));
}

void Test_LoadSettingsReadsTheRoutingArray() {
    using namespace SplitTrayTestHarness;
    ResetSettings();
    StringSettings()[L"defaultTray"] = L"secondary";
    StringSettings()[L"perProcessRouting[0].exe"] = L"stremio.exe";
    StringSettings()[L"perProcessRouting[0].destination"] = L"secondary";
    StringSettings()[L"perProcessRouting[1].exe"] = L"everything.exe";
    StringSettings()[L"perProcessRouting[1].destination"] = L"both";
    StringSettings()[L"perProcessRouting[2].exe"] = L"telemachus.exe";
    StringSettings()[L"perProcessRouting[2].destination"] = L"tray4";
    // Index 3 is absent, which is how the loop learns where the array ends.
    StringSettings()[L"extraTrays[0].display"] = L"primary";
    StringSettings()[L"extraTrays[0].corner"] = L"topRight";
    StringSettings()[L"extraTrays[1].display"] = L"2";
    StringSettings()[L"extraTrays[1].corner"] = L"bottomLeft";
    IntSettings()[L"extraTrays[1].disabled"] = 1;
    // extraTrays[0].disabled is absent, as it is for an entry saved before the
    // switch existed, and a missing value reads as 0: that has to mean on.
    // A display that is not a display still takes its place in the numbering.
    StringSettings()[L"extraTrays[2].display"] = L"left";
    StringSettings()[L"trayPosition"] = L"topLeft";
    StringSettings()[L"backgroundColor"] = L"123456";
    IntSettings()[L"iconSize"] = 20;
    IntSettings()[L"cellSize"] = 32;
    IntSettings()[L"maxColumns"] = 6;
    IntSettings()[L"opacity"] = 200;
    IntSettings()[L"alwaysOnTop"] = 1;
    IntSettings()[L"mirrorHiddenIcons"] = 1;

    const Settings s = LoadSettings();
    CHECK(s.defaultTray == Destination::Secondary);
    CHECK_EQ(static_cast<int>(s.rules.size()), 3);
    CHECK_WSTR(s.rules[0].pattern, L"stremio.exe");
    CHECK(s.rules[0].destination == Destination::Secondary);
    CHECK_WSTR(s.rules[1].pattern, L"everything.exe");
    CHECK(s.rules[1].destination == Destination::Both);
    CHECK(s.rules[2].destination == Destination::Tray(4));
    CHECK_EQ(static_cast<int>(s.extraTrays.size()), 3);
    CHECK_EQ(s.extraTrays[0].display, 0);
    CHECK(s.extraTrays[0].corner == Corner::TopRight);
    CHECK(!s.extraTrays[0].disabled);
    CHECK_EQ(s.extraTrays[1].display, 2);
    CHECK(s.extraTrays[1].corner == Corner::BottomLeft);
    CHECK(s.extraTrays[1].disabled);
    CHECK_EQ(s.extraTrays[2].display, -1);
    CHECK(!s.extraTrays[2].disabled);
    CHECK(s.corner == Corner::TopLeft);
    CHECK_EQ(s.background, RGB(0x12, 0x34, 0x56));
    CHECK_EQ(s.iconSize, 20);
    CHECK_EQ(s.cellSize, 32);
    CHECK_EQ(s.maxColumns, 6);
    CHECK_EQ(s.opacity, 200);
    CHECK(s.alwaysOnTop);
    CHECK(s.mirrorHiddenIcons);
    ResetSettings();
}

void Test_LoadSettingsClampsHostileValues() {
    using namespace SplitTrayTestHarness;
    ResetSettings();
    IntSettings()[L"iconSize"] = 100000;
    IntSettings()[L"cellSize"] = 1;  // smaller than the icon: must be raised
    IntSettings()[L"maxColumns"] = 0;
    IntSettings()[L"opacity"] = 0;
    const Settings s = LoadSettings();
    CHECK_EQ(s.iconSize, 128);
    CHECK(s.cellSize >= s.iconSize + 2);
    CHECK_EQ(s.maxColumns, 1);
    CHECK_EQ(s.opacity, 16);
    ResetSettings();
}

// ---------------------------------------------------------------------------
// Section 3 - routing
// ---------------------------------------------------------------------------

void Test_RoutesByExecutableNameCaseInsensitively() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"Discord.exe", Destination::Secondary}};
    CHECK(ResolveDestination(s, L"C:\\Users\\b\\AppData\\Local\\Discord\\discord.exe") ==
          Destination::Secondary);
    CHECK(ResolveDestination(s, L"C:\\x\\DISCORD.EXE") == Destination::Secondary);
    CHECK(ResolveDestination(s, L"C:\\x\\notepad.exe") == Destination::Primary);
    // A bare file name with no directory still matches.
    CHECK(ResolveDestination(s, L"discord.exe") == Destination::Secondary);
    // Not a substring match: "discord.exe" must not be matched by "cord.exe".
    s.rules = {{L"cord.exe", Destination::Secondary}};
    CHECK(ResolveDestination(s, L"C:\\x\\discord.exe") == Destination::Primary);
}

void Test_RulesWithASeparatorMatchThePath() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"\\Steam\\", Destination::Both}};
    CHECK(ResolveDestination(s, L"C:\\Program Files\\Steam\\steam.exe") ==
          Destination::Both);
    CHECK(ResolveDestination(s, L"C:\\Games\\steam.exe") == Destination::Primary);
}

void Test_FirstMatchingRuleWins() {
    Settings s = DefaultSettings();
    s.rules = {{L"app.exe", Destination::Secondary},
               {L"app.exe", Destination::Primary}};
    CHECK(ResolveDestination(s, L"C:\\app.exe") == Destination::Secondary);
}

void Test_EmptyRulePatternIsIgnored() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Both;
    s.rules = {{L"", Destination::Primary}};
    CHECK(ResolveDestination(s, L"C:\\anything.exe") == Destination::Both);
    // An unknown process with an empty path falls back to the default too.
    CHECK(ResolveDestination(s, L"") == Destination::Both);
}

void Test_PlanForwardsAndMirrorsPerDestination() {
    CHECK(PlanFor(Destination::Primary, true).forwardToShell);
    CHECK(!PlanFor(Destination::Primary, true).mirror);

    CHECK(!PlanFor(Destination::Secondary, true).forwardToShell);
    CHECK(PlanFor(Destination::Secondary, true).mirror);
    CHECK_EQ(PlanFor(Destination::Secondary, true).tray, 2);

    CHECK(PlanFor(Destination::Both, true).forwardToShell);
    CHECK(PlanFor(Destination::Both, true).mirror);
    CHECK_EQ(PlanFor(Destination::Both, true).tray, 2);

    // Any tray by number, and never into the shell as well unless asked.
    const RoutingPlan three = PlanFor(Destination::Tray(3), true);
    CHECK(!three.forwardToShell);
    CHECK(three.mirror);
    CHECK_EQ(three.tray, 3);
    CHECK(PlanFor(Destination::Tray(1), true).forwardToShell);
    CHECK(!PlanFor(Destination::Tray(1), true).mirror);
}

void Test_DestinationsNameTrays() {
    CHECK(ParseDestination(L"primary", Destination::Both) == Destination::Primary);
    CHECK(ParseDestination(L"secondary", Destination::Primary) == Destination::Tray(2));
    CHECK(ParseDestination(L"both", Destination::Primary) == Destination::Both);
    CHECK(ParseDestination(L"tray3", Destination::Primary) == Destination::Tray(3));
    CHECK(ParseDestination(L"tray12", Destination::Primary) == Destination::Tray(12));
    CHECK(ParseDestination(L"tray1", Destination::Both) == Destination::Primary);
    // Anything else keeps the fallback rather than guessing.
    CHECK(ParseDestination(L"tray", Destination::Both) == Destination::Both);
    CHECK(ParseDestination(L"tray0", Destination::Both) == Destination::Both);
    CHECK(ParseDestination(L"tray3x", Destination::Both) == Destination::Both);
    CHECK(ParseDestination(L"3", Destination::Both) == Destination::Both);
    CHECK(ParseDestination(nullptr, Destination::Both) == Destination::Both);

    CHECK_WSTR(DestinationName(Destination::Primary), L"primary");
    CHECK_WSTR(DestinationName(Destination::Tray(3)), L"tray 3");
    CHECK_WSTR(DestinationName(Destination::Both), L"tray 2 and primary");
}

void Test_PlacementCodesReadWhatEveryVersionWrote() {
    // The two-tray versions wrote p and s; those must keep their meaning.
    CHECK(ParsePlacementCode(L"p") == Destination::Primary);
    CHECK(ParsePlacementCode(L"s") == Destination::Secondary);
    CHECK(ParsePlacementCode(L"3") == Destination::Tray(3));
    CHECK(ParsePlacementCode(L"17") == Destination::Tray(17));
    CHECK(ParsePlacementCode(L"1") == Destination::Primary);
    CHECK(ParsePlacementCode(L"") == Destination::Primary);
    CHECK(ParsePlacementCode(L"x") == Destination::Primary);

    CHECK_WSTR(PlacementCode(Destination::Primary), L"p");
    CHECK_WSTR(PlacementCode(Destination::Secondary), L"s");
    CHECK_WSTR(PlacementCode(Destination::Tray(4)), L"4");
    for (int tray = 1; tray <= 9; tray++) {
        CHECK(ParsePlacementCode(PlacementCode(Destination::Tray(tray))) ==
              Destination::Tray(tray));
    }
}

void Test_MissingSecondaryMonitorFallsBackToPrimary() {
    // Requirement 7: with the configured display gone, every icon must show in
    // the primary tray - never be swallowed into a tray that does not exist.
    for (auto destination :
         {Destination::Primary, Destination::Secondary, Destination::Both}) {
        const RoutingPlan plan = PlanFor(destination, false);
        CHECK(plan.forwardToShell);
        CHECK(!plan.mirror);
    }
}

// ---------------------------------------------------------------------------
// Section 4 - monitors and layout
// ---------------------------------------------------------------------------

std::vector<MonitorInfoEntry> TwoMonitorSetup() {
    MonitorInfoEntry secondary;
    secondary.handle = reinterpret_cast<HMONITOR>(2);
    secondary.workArea = kSecondaryWorkArea;
    secondary.primary = false;
    secondary.dpi = 96;

    MonitorInfoEntry primary;
    primary.handle = reinterpret_cast<HMONITOR>(1);
    primary.workArea = kPrimaryWorkArea;
    primary.primary = true;
    primary.dpi = 96;

    // EnumerateMonitors sorts left to right, so the display at x=-1920 is index 1.
    return {secondary, primary};
}

// Four displays, sorted as EnumerateMonitors sorts them: the primary is third
// from the left, and there is one above it.
std::vector<MonitorInfoEntry> FourMonitorSetup() {
    auto make = [](int handle, RECT work, bool primary) {
        MonitorInfoEntry m;
        m.handle = reinterpret_cast<HMONITOR>(static_cast<INT_PTR>(handle));
        m.workArea = work;
        m.primary = primary;
        m.dpi = 96;
        return m;
    };
    return {make(1, {-3840, 0, -1920, 1040}, false),
            make(2, {-1920, 0, 0, 1040}, false),
            make(3, {0, -1080, 1920, -40}, false),
            make(4, {0, 0, 1920, 1040}, true)};
}

void Test_EveryDisplayButThePrimaryGetsATray() {
    Settings s = DefaultSettings();
    const auto trays = PlanTrays(FourMonitorSetup(), s);
    CHECK_EQ(static_cast<int>(trays.size()), 3);
    // Numbered from 2, left to right, skipping the primary display.
    for (size_t i = 0; i < trays.size(); i++) {
        CHECK_EQ(trays[i].number, static_cast<int>(i) + 2);
        CHECK(trays[i].forDisplay);
        CHECK(trays[i].available);
        CHECK(!trays[i].monitor.primary);
    }
    CHECK_EQ(trays[0].monitor.workArea.left, -3840);
    CHECK_EQ(trays[1].monitor.workArea.left, -1920);
    CHECK_EQ(trays[2].monitor.workArea.top, -1080);

    // The development machine: one tray, on the display to the left.
    const auto two = PlanTrays(TwoMonitorSetup(), s);
    CHECK_EQ(static_cast<int>(two.size()), 1);
    CHECK_EQ(two[0].number, 2);
    CHECK_EQ(two[0].monitor.workArea.left, kSecondaryWorkArea.left);
}

void Test_OneDisplayMeansNoTraysUnlessAsked() {
    // Requirement: with a single display, everything stays in one tray.
    MonitorInfoEntry only;
    only.workArea = kPrimaryWorkArea;
    only.primary = true;
    Settings s = DefaultSettings();
    CHECK(PlanTrays({only}, s).empty());
    CHECK(PlanTrays({}, s).empty());

    // An extra tray on it is what makes the mod usable - and testable - there.
    s.extraTrays = {{0, Corner::BottomLeft}};
    const auto trays = PlanTrays({only}, s);
    CHECK_EQ(static_cast<int>(trays.size()), 1);
    CHECK_EQ(trays[0].number, 2);
    CHECK(!trays[0].forDisplay);
    CHECK(trays[0].available);
    CHECK(trays[0].monitor.primary);
    CHECK(trays[0].corner == Corner::BottomLeft);
}

void Test_ExtraTraysComeAfterTheDisplaysAndKeepTheirNumbers() {
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft},   // the primary display
                    {9, Corner::TopLeft},      // not connected
                    {1, Corner::TopRight}};    // the leftmost display
    const auto trays = PlanTrays(TwoMonitorSetup(), s);
    CHECK_EQ(static_cast<int>(trays.size()), 4);
    CHECK(trays[0].forDisplay);
    CHECK_EQ(trays[1].number, 3);
    CHECK(trays[1].available);
    CHECK(trays[1].monitor.primary);
    // A missing display keeps its tray's number, so the next extra tray is
    // still tray 5 and icons placed there stay put.
    CHECK_EQ(trays[2].number, 4);
    CHECK(!trays[2].available);
    CHECK_EQ(trays[3].number, 5);
    CHECK(trays[3].available);
    CHECK_EQ(trays[3].monitor.workArea.left, kSecondaryWorkArea.left);
    CHECK(trays[3].corner == Corner::TopRight);
}

void Test_ADisabledExtraTrayKeepsItsNumberButIsNotThere() {
    // Regression test: an extra tray can be switched off and its entry kept -
    // the user's test tray on the primary display. It is not shown, and like an
    // extra tray whose display is missing it keeps its number, so the trays
    // after it do not shift and icons placed in them stay put.
    const auto monitors = TwoMonitorSetup();
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft, true}, {0, Corner::TopLeft}};
    const auto trays = PlanTrays(monitors, s);
    CHECK_EQ(static_cast<int>(trays.size()), 3);
    CHECK_EQ(trays[1].number, 3);
    CHECK(!trays[1].available);
    CHECK(trays[1].disabled);
    CHECK_EQ(trays[2].number, 4);
    CHECK(trays[2].available);
    CHECK(!trays[2].disabled);
    CHECK_WSTR(DescribeTrayPlace(trays[1], monitors[1]), L"disabled");
    CHECK_WSTR(DescribeTrayPlace(trays[2], monitors[1]), L"floating, primary display");
}

void Test_DisplaysAreFoundByNumberOrAsPrimary() {
    const auto monitors = TwoMonitorSetup();
    MonitorInfoEntry found;
    CHECK(ResolveDisplay(monitors, 0, &found));
    CHECK(found.primary);
    CHECK(ResolveDisplay(monitors, 1, &found));
    CHECK_EQ(found.workArea.left, kSecondaryWorkArea.left);
    CHECK(ResolveDisplay(monitors, 2, &found));
    CHECK_EQ(found.workArea.left, kPrimaryWorkArea.left);
    CHECK(!ResolveDisplay(monitors, 3, &found));
    CHECK(!ResolveDisplay(monitors, -1, &found));
    CHECK(!ResolveDisplay({}, 0, &found));
}

void Test_TraysAreDescribedByWhereTheyAre() {
    const auto monitors = FourMonitorSetup();
    const MonitorInfoEntry& primary = monitors[3];
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft}, {9, Corner::BottomLeft}};
    const auto trays = PlanTrays(monitors, s);
    CHECK_WSTR(DescribeTrayPlace(trays[0], primary), L"display to the left");
    CHECK_WSTR(DescribeTrayPlace(trays[2], primary), L"display above");
    CHECK_WSTR(DescribeTrayPlace(trays[3], primary), L"floating, primary display");
    CHECK_WSTR(DescribeTrayPlace(trays[4], primary),
               L"floating, display not connected");
}

// A display as EnumerateMonitors gives it, with the identity Windows has for
// its monitor, which does not follow where it is arranged.
MonitorInfoEntry NamedDisplay(const wchar_t* id, RECT workArea, bool primary = false) {
    MonitorInfoEntry display;
    display.handle = reinterpret_cast<HMONITOR>(static_cast<INT_PTR>(workArea.left + 100000));
    display.workArea = workArea;
    display.primary = primary;
    display.id = id;
    return display;
}

const MonitorInfoEntry kLeft = NamedDisplay(L"left", {-3840, 0, -1920, 1040});
const MonitorInfoEntry kMiddle = NamedDisplay(L"middle", {-1920, 0, 0, 1040});
const MonitorInfoEntry kMain = NamedDisplay(L"main", {0, 0, 1920, 1040}, true);

void Test_ADisplayKeepsItsTrayNumberWhileItIsUnplugged() {
    // DECISIONS 86. Unplugging a display renumbered every tray after it: icons
    // meant for tray 3 showed on another display as tray 2, and an extra
    // tray's moved into the next one's place. A display's tray now keeps its
    // number while the display is away, and so does every tray after it.
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft}};
    std::vector<std::wstring> slots;
    CHECK(AssignDisplaySlots({kLeft, kMiddle, kMain}, &slots));
    auto trays = PlanTrays({kLeft, kMiddle, kMain}, s, slots);
    // Seen for the first time, numbered as before: from the left, then the
    // extra trays.
    CHECK_EQ(static_cast<int>(trays.size()), 3);
    if (trays.size() == 3) {
        CHECK(trays[0].number == 2 && trays[0].monitor.id == L"left");
        CHECK(trays[1].number == 3 && trays[1].monitor.id == L"middle");
        CHECK(trays[2].number == 4 && !trays[2].forDisplay);
    }

    // The display on the left unplugged: its tray is still tray 2, not
    // there, and the others keep their numbers.
    CHECK(!AssignDisplaySlots({kMiddle, kMain}, &slots));
    trays = PlanTrays({kMiddle, kMain}, s, slots);
    CHECK_EQ(static_cast<int>(trays.size()), 3);
    if (trays.size() == 3) {
        CHECK(trays[0].number == 2 && trays[0].forDisplay && !trays[0].available);
        CHECK_WSTR(DescribeTrayPlace(trays[0], kMain), L"display not connected");
        CHECK(trays[1].number == 3 && trays[1].available && trays[1].monitor.id == L"middle");
        CHECK(trays[2].number == 4 && trays[2].available);
    }
    // An icon meant for tray 2 waits in the primary tray meanwhile.
    CHECK(!PlanFor(Destination::Tray(2), trays[0].available).mirror);

    // Back, wherever it is put now: tray 2 again.
    const MonitorInfoEntry movedLeft = NamedDisplay(L"left", {1920, 0, 3840, 1040});
    CHECK(!AssignDisplaySlots({kMiddle, kMain, movedLeft}, &slots));
    trays = PlanTrays({kMiddle, kMain, movedLeft}, s, slots);
    CHECK(trays.size() == 3 && trays[0].available && trays[0].monitor.id == L"left" &&
          trays[1].monitor.id == L"middle");
}

void Test_ADisplaySeenForTheFirstTimeTakesTheNumberOfOneThatIsAway() {
    // DECISIONS 86. A laptop meets a different display in each place it is
    // used. Each takes the number of the one that is not there, so the rules
    // written for "tray 2" follow the laptop's external display, as they did
    // when trays were numbered by position.
    const MonitorInfoEntry home = NamedDisplay(L"home", {1920, 0, 3840, 1040});
    const MonitorInfoEntry office = NamedDisplay(L"office", {1920, 0, 4480, 1400});
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft}};
    std::vector<std::wstring> slots;
    AssignDisplaySlots({kMain, home}, &slots);
    CHECK(AssignDisplaySlots({kMain, office}, &slots));
    auto trays = PlanTrays({kMain, office}, s, slots);
    CHECK(trays.size() == 2 && trays[0].number == 2 && trays[0].available &&
          trays[0].monitor.id == L"office" && trays[1].number == 3);
    CHECK(AssignDisplaySlots({kMain, home}, &slots));
    trays = PlanTrays({kMain, home}, s, slots);
    CHECK(trays.size() == 2 && trays[0].monitor.id == L"home");
    CHECK_EQ(static_cast<int>(slots.size()), 1);

    // Only more displays at once than ever before make a new number - and the
    // extra trays move up one, the one renumbering left.
    CHECK(AssignDisplaySlots({kMain, home, office}, &slots));
    trays = PlanTrays({kMain, home, office}, s, slots);
    CHECK_EQ(static_cast<int>(trays.size()), 3);
    if (trays.size() == 3) {
        CHECK(trays[0].monitor.id == L"home" && trays[1].monitor.id == L"office");
        CHECK(trays[2].number == 4 && !trays[2].forDisplay);
    }
}

void Test_ADisplayThatBecomesThePrimaryGivesUpItsTray() {
    // DECISIONS 86. The primary display has Explorer's own tray. One made
    // primary leaves its number free for a display that is not; the old
    // primary, seen for the first time as a display of its own, takes it.
    const MonitorInfoEntry mainNowSecondary = NamedDisplay(L"main", {0, 0, 1920, 1040});
    const MonitorInfoEntry middleNowPrimary =
        NamedDisplay(L"middle", {-1920, 0, 0, 1040}, true);
    Settings s = DefaultSettings();
    std::vector<std::wstring> slots;
    AssignDisplaySlots({kMiddle, kMain}, &slots);
    CHECK(AssignDisplaySlots({middleNowPrimary, mainNowSecondary}, &slots));
    const auto trays = PlanTrays({middleNowPrimary, mainNowSecondary}, s, slots);
    CHECK(trays.size() == 1 && trays[0].number == 2 && trays[0].available &&
          trays[0].monitor.id == L"main");

    // The one display left is the primary one, and had a slot of its own - a
    // laptop's external display unplugged. Its tray stays empty rather than
    // appearing on the display that has Explorer's.
    std::vector<std::wstring> alone = {L"middle"};
    CHECK(!AssignDisplaySlots({middleNowPrimary}, &alone));
    const auto onlyPrimary = PlanTrays({middleNowPrimary}, s, alone);
    CHECK(onlyPrimary.size() == 1 && !onlyPrimary[0].available);
}

void Test_TrayLandsInsideAMonitorAtNegativeCoordinates() {
    // Regression test: the previous build assumed the secondary display was to
    // the right of the primary and placed its window at x=3620, off every screen.
    Settings s = DefaultSettings();
    const TrayLayout layout = ComputeLayout(kSecondaryWorkArea, 5, s, 96);

    CHECK_EQ(layout.columns, 5);
    CHECK_EQ(layout.rows, 1);
    CHECK_EQ(layout.width, 5 * 28);
    CHECK_EQ(layout.height, 28);
    CHECK_EQ(layout.x, kSecondaryWorkArea.right - 8 - layout.width);
    CHECK_EQ(layout.y, kSecondaryWorkArea.bottom - 8 - layout.height);

    // Fully inside the work area, which is the property that actually matters.
    CHECK(layout.x >= kSecondaryWorkArea.left);
    CHECK(layout.y >= kSecondaryWorkArea.top);
    CHECK(layout.x + layout.width <= kSecondaryWorkArea.right);
    CHECK(layout.y + layout.height <= kSecondaryWorkArea.bottom);
    CHECK(layout.x < 0);  // and it really is the negative-coordinate display
}

void Test_LayoutHonoursEveryCorner() {
    Settings s = DefaultSettings();
    s.offsetX = 10;
    s.offsetY = 12;

    s.corner = Corner::BottomLeft;
    TrayLayout l = ComputeLayout(kSecondaryWorkArea, 3, s, 96);
    CHECK_EQ(l.x, kSecondaryWorkArea.left + 10);
    CHECK_EQ(l.y, kSecondaryWorkArea.bottom - 12 - l.height);

    s.corner = Corner::TopLeft;
    l = ComputeLayout(kSecondaryWorkArea, 3, s, 96);
    CHECK_EQ(l.x, kSecondaryWorkArea.left + 10);
    CHECK_EQ(l.y, kSecondaryWorkArea.top + 12);

    s.corner = Corner::TopRight;
    l = ComputeLayout(kSecondaryWorkArea, 3, s, 96);
    CHECK_EQ(l.x, kSecondaryWorkArea.right - 10 - l.width);
    CHECK_EQ(l.y, kSecondaryWorkArea.top + 12);
}

void Test_LayoutWrapsOntoMoreRows() {
    Settings s = DefaultSettings();
    s.maxColumns = 4;
    const TrayLayout l = ComputeLayout(kSecondaryWorkArea, 9, s, 96);
    CHECK_EQ(l.columns, 4);
    CHECK_EQ(l.rows, 3);
    CHECK_EQ(l.width, 4 * 28);
    CHECK_EQ(l.height, 3 * 28);
}

void Test_LayoutIsEmptyWithNoIcons() {
    Settings s = DefaultSettings();
    const TrayLayout l = ComputeLayout(kSecondaryWorkArea, 0, s, 96);
    CHECK_EQ(l.width, 0);
    CHECK_EQ(l.height, 0);
    CHECK_EQ(l.columns, 0);
}

void Test_LayoutScalesWithMonitorDpi() {
    Settings s = DefaultSettings();
    const TrayLayout l = ComputeLayout(kSecondaryWorkArea, 2, s, 144);  // 150%
    CHECK_EQ(l.cell, 42);
    CHECK_EQ(l.icon, 24);
    CHECK_EQ(l.width, 84);
    CHECK(l.x + l.width <= kSecondaryWorkArea.right);
}

void Test_HitTestMatchesThePaintedGrid() {
    Settings s = DefaultSettings();
    s.maxColumns = 3;
    const TrayLayout l = ComputeLayout(kSecondaryWorkArea, 5, s, 96);
    // Painting walks index -> (index % columns, index / columns); hit testing
    // must invert exactly that.
    for (int index = 0; index < 5; index++) {
        const int column = index % l.columns;
        const int row = index / l.columns;
        const int centreX = column * l.cell + l.cell / 2;
        const int centreY = row * l.cell + l.cell / 2;
        CHECK_EQ(HitTestCell(l, centreX, centreY, 5), index);
    }
    // The sixth cell exists in the grid but has no icon behind it.
    CHECK_EQ(HitTestCell(l, 2 * l.cell + 1, 1 * l.cell + 1, 5), -1);
    CHECK_EQ(HitTestCell(l, -1, 0, 5), -1);
    CHECK_EQ(HitTestCell(l, 0, -1, 5), -1);
    CHECK_EQ(HitTestCell(l, l.width, 0, 5), -1);
    CHECK_EQ(HitTestCell(l, 0, l.height, 5), -1);
    CHECK_EQ(HitTestCell(l, 0, 0, 0), -1);
}

void Test_CellScreenRectIsWhereTheCellIsPainted() {
    Settings s = DefaultSettings();
    s.maxColumns = 3;
    const TrayLayout l = ComputeLayout(kSecondaryWorkArea, 5, s, 96);
    for (int index = 0; index < 5; index++) {
        RECT rect = {};
        CHECK(CellScreenRect(l, index, 5, &rect));
        CHECK_EQ(rect.right - rect.left, l.cell);
        CHECK_EQ(rect.bottom - rect.top, l.cell);
        // Its middle hit-tests back to the same icon.
        const int middleX = (rect.left + rect.right) / 2 - l.x;
        const int middleY = (rect.top + rect.bottom) / 2 - l.y;
        CHECK_EQ(HitTestCell(l, middleX, middleY, 5), index);
    }
    RECT rect = {};
    CHECK(CellScreenRect(l, 4, 5, &rect));
    CHECK_EQ(rect.left, l.x + 1 * l.cell);  // second column
    CHECK_EQ(rect.top, l.y + 1 * l.cell);   // second row

    CHECK(!CellScreenRect(l, 5, 5, &rect));
    CHECK(!CellScreenRect(l, -1, 5, &rect));
    CHECK(!CellScreenRect(ComputeLayout(kSecondaryWorkArea, 0, s, 96), 0, 0, &rect));
}

void Test_IslandBoundsScaleToTheScreen() {
    // The secondary taskbar on this machine: its window at (-1920, 912), the
    // XAML island filling it, at 125%.
    const RECT r = IslandBoundsToScreen(POINT{-1920, 912}, 1200, 5, 32, 38, 1.25);
    CHECK_EQ(r.left, -420);
    CHECK_EQ(r.top, 918);     // 912 + 6.25
    CHECK_EQ(r.right, -380);  // -1920 + 1540
    CHECK_EQ(r.bottom, 966);  // 912 + 53.75

    const RECT unscaled = IslandBoundsToScreen(POINT{0, 1032}, 10, 0, 32, 48, 1.0);
    CHECK_EQ(unscaled.left, 10);
    CHECK_EQ(unscaled.top, 1032);
    CHECK_EQ(unscaled.right, 42);
    CHECK_EQ(unscaled.bottom, 1080);
}

// ---------------------------------------------------------------------------
// Section 5 - icon identity
// ---------------------------------------------------------------------------

void Test_IconIdentityUsesGuidWhenPresent() {
    TrayNotification withGuid;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4Guid,
                                sizeof(kPayloadUnicodeV4Guid), &withGuid));

    MirroredIcon icon;
    icon.hasGuid = true;
    icon.guid = withGuid.guid;
    icon.ownerWnd = reinterpret_cast<HWND>(0xDEAD);  // deliberately different
    icon.uID = 999;
    CHECK(SameIcon(icon, withGuid));

    // A GUID-identified icon can legitimately report uID 0 on a later message.
    TrayNotification sameGuidNoId = withGuid;
    sameGuidNoId.uID = 0;
    CHECK(SameIcon(icon, sameGuidNoId));

    TrayNotification otherGuid = withGuid;
    otherGuid.guid.Data1 ^= 1;
    CHECK(!SameIcon(icon, otherGuid));
}

void Test_IconIdentityFallsBackToWindowAndId() {
    TrayNotification n;
    CHECK(ParseTrayNotification(kTrayCopyDataId, kPayloadUnicodeV4,
                                sizeof(kPayloadUnicodeV4), &n));
    MirroredIcon icon;
    icon.hasGuid = false;
    icon.ownerWnd = n.ownerWnd;
    icon.uID = n.uID;
    CHECK(SameIcon(icon, n));

    TrayNotification otherId = n;
    otherId.uID = n.uID + 1;
    CHECK(!SameIcon(icon, otherId));

    TrayNotification otherWnd = n;
    otherWnd.ownerWnd = reinterpret_cast<HWND>(0x1234);
    CHECK(!SameIcon(icon, otherWnd));
}

// ---------------------------------------------------------------------------
// Section 6 - store behaviour, driven through the real notification path
// ---------------------------------------------------------------------------

// Builds a payload from a golden one with selected fields overridden, so tests
// can drive the store with realistic records.
std::vector<BYTE> MakePayload(DWORD message,
                              DWORD ownerWnd,
                              DWORD uID,
                              DWORD flags,
                              PCWSTR tip,
                              PCWSTR exePath,
                              const GUID* guid = nullptr) {
    std::vector<BYTE> p(kPayloadUnicodeV4,
                        kPayloadUnicodeV4 + sizeof(kPayloadUnicodeV4));
    // Written unconditionally: the golden capture carries whatever the real
    // shell put there, and a test asking for NIF_GUID must control the field.
    const GUID value = guid ? *guid : GUID{};
    memcpy(p.data() + wire::kGuid, &value, sizeof(GUID));
    memcpy(p.data() + wire::kMessage, &message, 4);
    memcpy(p.data() + wire::kOwnerWnd, &ownerWnd, 4);
    memcpy(p.data() + wire::kUID, &uID, 4);
    memcpy(p.data() + wire::kFlags, &flags, 4);
    memset(p.data() + wire::kTip, 0, wire::kTipChars * sizeof(wchar_t));
    if (tip) {
        const size_t chars = std::min<size_t>(wcslen(tip), wire::kTipChars - 1);
        memcpy(p.data() + wire::kTip, tip, chars * sizeof(wchar_t));
    }
    memset(p.data() + wire::kExePath, 0, wire::kExePathChars * sizeof(wchar_t));
    if (exePath) {
        const size_t chars = std::min<size_t>(wcslen(exePath), wire::kExePathChars - 1);
        memcpy(p.data() + wire::kExePath, exePath, chars * sizeof(wchar_t));
    }
    return p;
}

// Pushes a payload through the same code path the subclass uses.
bool Feed(const std::vector<BYTE>& payload,
          bool* forwardToShell,
          bool* retractFromShell = nullptr,
          bool* recordShellAnswer = nullptr) {
    TrayNotification n;
    if (!ParseTrayNotification(kTrayCopyDataId, payload.data(), payload.size(), &n)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    FillMissingPathLocked(&n);
    ApplyNotificationLocked(n, payload, forwardToShell, retractFromShell,
                            recordShellAnswer);
    return true;
}

// The tests' windows are made-up handles, and one could happen to be a real
// window on the machine running them, whose program would then be looked up.
// Nothing is found unless a test says otherwise.
std::wstring NoProcessPath(HWND) {
    return std::wstring();
}

// The displays the store tests see: the development machine's two, or only its
// primary one when a test takes the other away.
bool g_testSecondaryConnected = true;

std::vector<MonitorInfoEntry> TestMonitors() {
    auto monitors = TwoMonitorSetup();
    if (!g_testSecondaryConnected) {
        monitors.erase(monitors.begin());  // the display on the left
    }
    return monitors;
}

// The taskbar thread's half of a move (SettleShellIcons), with a stand-in for
// Explorer that answers each record it is handed with `answer`. Returns the
// records, in order.
template <typename Answer>
std::vector<TrayNotification> SettleAnswering(Answer answer) {
    std::vector<TrayNotification> handed;
    SettleShellIcons([&](HWND, const std::vector<BYTE>& record) -> LRESULT {
        TrayNotification n;
        ParseTrayNotification(kTrayCopyDataId, record.data(), record.size(), &n);
        handed.push_back(n);
        return answer(n);
    });
    return handed;
}

// The same, with an Explorer that takes everything. Returns the messages.
std::vector<DWORD> SettleWithExplorer() {
    std::vector<DWORD> messages;
    for (const TrayNotification& n :
         SettleAnswering([](const TrayNotification&) -> LRESULT { return TRUE; })) {
        messages.push_back(n.message);
    }
    return messages;
}

void ResetStore(const Settings& settings, bool secondaryAvailable) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& icon : g_icons) {
        if (icon.icon) {
            DestroyIcon(icon.icon);
        }
    }
    g_icons.clear();
    g_primaryOnly.clear();
    g_settings = settings;
    g_testSecondaryConnected = secondaryAvailable;
    g_enumerateMonitors = TestMonitors;
    g_lookUpProcessPath = NoProcessPath;
    g_embeddedMonitors.clear();
    // Remembered placements outrank the rules, so a leftover from an earlier
    // test would quietly override the routing this one is about to assert.
    // Clearing the stored string as well, or the next load would read it back.
    // Which display has which tray number is remembered too (DECISIONS 86),
    // and is read as the trays are planned, so it goes first.
    SplitTrayTestHarness::StoredValues().clear();
    g_displaySlots.clear();
    g_displaySlotsLoaded = false;
    RecomputeGeometryLocked(g_enumerateMonitors());
    g_placements.clear();
    g_placementsLoaded = false;
    g_hidden.clear();
    g_hiddenLoaded = false;
    g_lastSeen.clear();
    g_lastSeenLoaded = false;
    g_lastSeenSweepDay = -1;
}

void Test_SecondaryOnlyIconIsSwallowedAndMirrored() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"stremio.exe", Destination::Secondary}};
    ResetStore(s, true);

    bool forward = true;
    CHECK(Feed(MakePayload(NIM_ADD, 0x1111, 7, NIF_MESSAGE | NIF_TIP, L"Stremio",
                           L"C:\\apps\\stremio.exe"),
               &forward));
    CHECK(!forward);  // removed from the primary tray
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
        CHECK_WSTR(g_icons[0].tip, L"Stremio");
        CHECK(g_icons[0].destination == Destination::Secondary);
        CHECK(!g_icons[0].forwardedToShell);
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 0);
    }
}

void Test_PrimaryOnlyIconIsForwardedAndNotMirrored() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);

    bool forward = false;
    CHECK(Feed(MakePayload(NIM_ADD, 0x2222, 3, NIF_MESSAGE | NIF_TIP, L"Notepad",
                           L"C:\\windows\\notepad.exe"),
               &forward));
    CHECK(forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 0);
        // Still tracked, so a later settings change can move it without waiting
        // for the application to touch its icon again.
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
    }
}

void Test_BothShowsInTheMirrorAndStaysInThePrimaryTray() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Both;
    ResetStore(s, true);

    bool forward = false;
    CHECK(Feed(MakePayload(NIM_ADD, 0x3333, 1, NIF_MESSAGE | NIF_TIP, L"Everything",
                           L"C:\\tools\\everything.exe"),
               &forward));
    CHECK(forward);
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK(g_icons[0].forwardedToShell);
}

IconRectQuery QueryFor(DWORD ownerWnd, UINT uID, const GUID* guid = nullptr) {
    IconRectQuery q;
    q.part = kIconRectSize;
    q.ownerWnd = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(ownerWnd));
    q.uID = uID;
    if (guid) {
        q.guid = *guid;
    }
    return q;
}

int AnsweredByTheMod(const IconRectQuery& q) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return SecondaryOnlyIconIndexLocked(q);
}

void Test_TheModAnswersWhereOnlyForIconsTheShellDoesNotHave() {
    // Tauri's tray library asks where its icon is before handling any click,
    // and drops the click if the answer is "not found". Explorer can only say
    // for icons it holds, so the mod answers for the rest - and only those.
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"telemachus.exe", Destination::Secondary}};
    ResetStore(s, true);

    bool forward = true;
    CHECK(Feed(MakePayload(NIM_ADD, 0x1111, 2, NIF_MESSAGE | NIF_TIP, L"Telemachus",
                           L"C:\\apps\\telemachus.exe"),
               &forward));
    CHECK(!forward);
    CHECK(Feed(MakePayload(NIM_ADD, 0x2222, 3, NIF_MESSAGE | NIF_TIP, L"Notepad",
                           L"C:\\windows\\notepad.exe"),
               &forward));
    CHECK(forward);

    CHECK_EQ(AnsweredByTheMod(QueryFor(0x1111, 2)), 0);
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x2222, 3)), -1);  // Explorer has it
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x1111, 3)), -1);  // nobody has it
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x3333, 2)), -1);

    // An icon shown in both trays is in Explorer's too, so Explorer answers.
    s.defaultTray = Destination::Both;
    s.rules.clear();
    ResetStore(s, true);
    CHECK(Feed(MakePayload(NIM_ADD, 0x3333, 1, NIF_MESSAGE | NIF_TIP, L"Everything",
                           L"C:\\tools\\everything.exe"),
               &forward));
    CHECK(forward);
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x3333, 1)), -1);
}

void Test_AnIconAskedAboutByGuidIsFoundByGuid() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    const GUID guid = {0x11223344, 0x5566, 0x7788, {1, 2, 3, 4, 5, 6, 7, 8}};
    bool forward = true;
    CHECK(Feed(MakePayload(NIM_ADD, 0x1111, 0, NIF_MESSAGE | NIF_GUID, L"guid icon",
                           L"C:\\apps\\guid.exe", &guid),
               &forward));
    CHECK(!forward);

    // The identifier's GUID decides, whatever window it names.
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x9999, 0, &guid)), 0);
    GUID other = guid;
    other.Data1 ^= 1;
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x1111, 0, &other)), -1);
    // Without one it is window and uID, as for any icon.
    CHECK_EQ(AnsweredByTheMod(QueryFor(0x1111, 0)), 0);
}

void Test_RoutingIsStickyForTheLifeOfAnIcon() {
    // If the add was swallowed, every later message about that icon must be
    // swallowed too - otherwise the primary tray gets a modify for an icon it
    // never received, and the two trays disagree about what exists.
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x4444, 2, NIF_MESSAGE | NIF_TIP, L"first",
                     L"C:\\a\\app.exe"),
         &forward);
    CHECK(!forward);

    // The rules change under us, but this icon keeps its original decision.
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_settings.defaultTray = Destination::Primary;
    }
    forward = true;
    Feed(MakePayload(NIM_MODIFY, 0x4444, 2, NIF_TIP, L"second", L"C:\\a\\app.exe"),
         &forward);
    CHECK(!forward);

    forward = true;
    Feed(MakePayload(NIM_DELETE, 0x4444, 2, 0, nullptr, L"C:\\a\\app.exe"), &forward);
    CHECK(!forward);
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 0);
}

void Test_PartialModifyKeepsUntouchedFields() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5555, 9, NIF_MESSAGE | NIF_TIP, L"original tip",
                     L"C:\\a\\app.exe"),
         &forward);

    // A modify that only carries NIF_ICON must not blank the tooltip, and must
    // not clear the callback message that clicks are forwarded with.
    Feed(MakePayload(NIM_MODIFY, 0x5555, 9, NIF_ICON, L"ignored", L"C:\\a\\app.exe"),
         &forward);

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK_WSTR(g_icons[0].tip, L"original tip");
    CHECK_EQ(g_icons[0].callbackMessage, kProbeCallbackMsg);
}

void Test_HiddenIconsAreSkippedWhenAsked() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.mirrorHiddenIcons = false;
    ResetStore(s, true);

    std::vector<BYTE> payload = MakePayload(
        NIM_ADD, 0x6666, 4, NIF_MESSAGE | NIF_TIP | NIF_STATE, L"hidden",
        L"C:\\a\\app.exe");
    const DWORD state = NIS_HIDDEN;
    const DWORD mask = NIS_HIDDEN;
    memcpy(payload.data() + wire::kState, &state, 4);
    memcpy(payload.data() + wire::kStateMask, &mask, 4);

    bool forward = true;
    CHECK(Feed(payload, &forward));
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 0);
    CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
}

void Test_MirrorsHiddenIconsByDefault() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.mirrorHiddenIcons = true;
    ResetStore(s, true);

    std::vector<BYTE> payload = MakePayload(
        NIM_ADD, 0x7777, 4, NIF_MESSAGE | NIF_TIP | NIF_STATE, L"hidden",
        L"C:\\a\\app.exe");
    const DWORD state = NIS_HIDDEN;
    memcpy(payload.data() + wire::kState, &state, 4);
    memcpy(payload.data() + wire::kStateMask, &state, 4);

    bool forward = true;
    CHECK(Feed(payload, &forward));
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
}

void Test_NoMirroringWhenTheSecondaryMonitorIsGone() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, /*secondaryAvailable=*/false);

    bool forward = false;
    CHECK(Feed(MakePayload(NIM_ADD, 0x8888, 1, NIF_MESSAGE | NIF_TIP, L"app",
                           L"C:\\a\\app.exe"),
               &forward));
    CHECK(forward);  // the icon must still be visible somewhere
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 0);
}

void Test_DeleteOfAnUnknownIconIsForwarded() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    bool forward = false;
    CHECK(Feed(MakePayload(NIM_DELETE, 0x9999, 5, 0, nullptr, L"C:\\a\\app.exe"),
               &forward));
    CHECK(forward);
}

std::vector<BYTE> MakeSetVersion(DWORD ownerWnd, DWORD uID, DWORD version) {
    std::vector<BYTE> p = MakePayload(NIM_SETVERSION, ownerWnd, uID, 0, nullptr,
                                      L"C:\\a\\app.exe");
    memcpy(p.data() + wire::kVersion, &version, 4);
    return p;
}

// Was "recorded and forwarded", unconditionally. Forwarding the version of an
// icon the shell was never given fails the call in the application, and the
// version is now replayed after the icon is next added instead.
void Test_SetVersionIsRecordedAndOnlyForwardedToAShellThatHasTheIcon() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.rules = {{L"primary.exe", Destination::Primary}};
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0xAAAA, 6, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    CHECK(!forward);
    forward = true;
    CHECK(Feed(MakeSetVersion(0xAAAA, 6, NOTIFYICON_VERSION_4), &forward));
    CHECK(!forward);  // the shell does not have this icon

    Feed(MakePayload(NIM_ADD, 0xABAB, 1, NIF_MESSAGE | NIF_TIP, L"other",
                     L"C:\\a\\primary.exe"),
         &forward);
    CHECK(forward);
    forward = false;
    CHECK(Feed(MakeSetVersion(0xABAB, 1, NOTIFYICON_VERSION_4), &forward));
    CHECK(forward);  // this one it does

    // A version for an icon the mod has never seen is the shell's business.
    forward = false;
    CHECK(Feed(MakeSetVersion(0xACAC, 9, NOTIFYICON_VERSION_4), &forward));
    CHECK(forward);

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK_EQ(g_icons[0].version, static_cast<UINT>(NOTIFYICON_VERSION_4));
    CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
    CHECK_EQ(g_primaryOnly[0].version, static_cast<UINT>(NOTIFYICON_VERSION_4));
}

// ---------------------------------------------------------------------------
// The record that recreates an icon
//
// From a live report: the Claude usage monitor vanished after a move to the
// secondary tray and back. The record replayed as the add was the last message,
// a partial modify - flags 0x2, no callback, no tooltip, no path.
// ---------------------------------------------------------------------------

std::vector<BYTE> WithCallbackAndIcon(std::vector<BYTE> p, DWORD callback, DWORD icon) {
    memcpy(p.data() + wire::kCallbackMsg, &callback, 4);
    memcpy(p.data() + wire::kIcon, &icon, 4);
    return p;
}

TrayNotification Parsed(const std::vector<BYTE>& record) {
    TrayNotification n;
    ParseTrayNotification(kTrayCopyDataId, record.data(), record.size(), &n);
    return n;
}

void Test_FoldKeepsWhatAPartialModifyLeavesOut() {
    std::vector<BYTE> state;
    FoldTrayRecord(&state,
                   WithCallbackAndIcon(MakePayload(NIM_ADD, 0x10, 0,
                                                   NIF_MESSAGE | NIF_ICON | NIF_TIP,
                                                   L"usage 9%",
                                                   L"C:\\m\\UsageMonitor.exe"),
                                       0x8001, 0x1111));
    // The picture alone, then the tooltip alone. Neither carries a path.
    FoldTrayRecord(&state, WithCallbackAndIcon(
                               MakePayload(NIM_MODIFY, 0x10, 0, NIF_ICON, nullptr,
                                           nullptr),
                               0, 0x2222));
    FoldTrayRecord(&state, MakePayload(NIM_MODIFY, 0x10, 0, NIF_TIP, L"usage 12%",
                                       nullptr));

    const TrayNotification n = Parsed(state);
    CHECK_EQ(n.flags & (NIF_MESSAGE | NIF_ICON | NIF_TIP),
             static_cast<UINT>(NIF_MESSAGE | NIF_ICON | NIF_TIP));
    CHECK_EQ(n.callbackMessage, 0x8001u);
    CHECK_EQ(reinterpret_cast<ULONG_PTR>(n.icon), static_cast<ULONG_PTR>(0x2222));
    CHECK_WSTR(n.tip, L"usage 12%");
    CHECK_WSTR(n.exePath, L"C:\\m\\UsageMonitor.exe");
}

void Test_FoldDoesNotKeepABalloon() {
    std::vector<BYTE> state;
    FoldTrayRecord(&state, MakePayload(NIM_ADD, 0x10, 1, NIF_MESSAGE | NIF_INFO,
                                       nullptr, L"C:\\a\\app.exe"));
    CHECK_EQ(Parsed(state).flags & NIF_INFO, 0u);
    FoldTrayRecord(&state, MakePayload(NIM_MODIFY, 0x10, 1, NIF_INFO | NIF_REALTIME,
                                       nullptr, nullptr));
    CHECK_EQ(Parsed(state).flags & (NIF_INFO | NIF_REALTIME), 0u);
    CHECK((Parsed(state).flags & NIF_MESSAGE) != 0);
}

void Test_FoldAppliesStateThroughItsMask() {
    std::vector<BYTE> state;
    std::vector<BYTE> add =
        MakePayload(NIM_ADD, 0x10, 1, NIF_STATE, nullptr, L"C:\\a\\app.exe");
    const DWORD hiddenAndShared[2] = {NIS_HIDDEN | NIS_SHAREDICON,
                                      NIS_HIDDEN | NIS_SHAREDICON};
    memcpy(add.data() + wire::kState, hiddenAndShared, 8);
    FoldTrayRecord(&state, add);

    // Un-hide only: the shared bit is outside the mask and must survive.
    std::vector<BYTE> modify = MakePayload(NIM_MODIFY, 0x10, 1, NIF_STATE, nullptr,
                                           nullptr);
    const DWORD unhide[2] = {0, NIS_HIDDEN};
    memcpy(modify.data() + wire::kState, unhide, 8);
    FoldTrayRecord(&state, modify);

    const TrayNotification n = Parsed(state);
    CHECK_EQ(n.state, static_cast<DWORD>(NIS_SHAREDICON));
    CHECK_EQ(n.stateMask, static_cast<DWORD>(NIS_HIDDEN | NIS_SHAREDICON));
}

void Test_AnAddStartsTheRecordAfresh() {
    std::vector<BYTE> state;
    FoldTrayRecord(&state, MakePayload(NIM_ADD, 0x10, 1, NIF_MESSAGE | NIF_TIP,
                                       L"old", L"C:\\a\\app.exe"));
    FoldTrayRecord(&state, MakePayload(NIM_ADD, 0x10, 1, NIF_ICON, nullptr,
                                       L"C:\\a\\app.exe"));
    CHECK_EQ(Parsed(state).flags, static_cast<UINT>(NIF_ICON));
}

void Test_AddRecordDrawsWithTheModsOwnPicture() {
    std::vector<BYTE> state;
    FoldTrayRecord(&state,
                   WithCallbackAndIcon(MakePayload(NIM_MODIFY, 0x10, 1, NIF_TIP,
                                                   L"tip", L"C:\\a\\app.exe"),
                                       0, 0x1111));
    const TrayNotification n =
        Parsed(AddRecordFor(state, reinterpret_cast<HICON>(0xBEEF)));
    CHECK_EQ(n.message, static_cast<DWORD>(NIM_ADD));
    CHECK_EQ(reinterpret_cast<ULONG_PTR>(n.icon), static_cast<ULONG_PTR>(0xBEEF));
    CHECK((n.flags & NIF_ICON) != 0);
    CHECK((n.flags & NIF_TIP) != 0);

    // Without a picture of its own the record has none. It used to keep the
    // handle the application last sent, which it has usually destroyed by
    // then, and which may by then be another icon's (DECISIONS 72).
    const TrayNotification bare = Parsed(AddRecordFor(state, nullptr));
    CHECK(bare.icon == nullptr);
    CHECK_EQ(bare.flags & NIF_ICON, 0u);
    CHECK((bare.flags & NIF_TIP) != 0);
}

void Test_SetVersionRecordCarriesTheVersion() {
    std::vector<BYTE> state;
    FoldTrayRecord(&state, MakePayload(NIM_ADD, 0x10, 7, NIF_MESSAGE, nullptr,
                                       L"C:\\a\\app.exe"));
    const TrayNotification n =
        Parsed(SetVersionRecordFor(state, NOTIFYICON_VERSION_4));
    CHECK_EQ(n.message, static_cast<DWORD>(NIM_SETVERSION));
    CHECK_EQ(n.version, static_cast<DWORD>(NOTIFYICON_VERSION_4));
    CHECK_EQ(n.uID, 7u);
}

// The same, through the store, for an icon in either tray.
void Test_TheStoreKeepsTheWholeIconNotTheLastMessage() {
    for (const Destination tray : {Destination::Primary, Destination::Secondary}) {
        Settings s = DefaultSettings();
        s.defaultTray = tray;
        ResetStore(s, true);

        bool forward = false;
        Feed(WithCallbackAndIcon(MakePayload(NIM_ADD, 0x10, 0,
                                             NIF_MESSAGE | NIF_ICON | NIF_TIP,
                                             L"usage 9%", L"C:\\m\\UsageMonitor.exe"),
                                 0x8001, 0),
             &forward);
        Feed(MakePayload(NIM_MODIFY, 0x10, 0, NIF_TIP, L"usage 12%", nullptr),
             &forward);

        std::lock_guard<std::mutex> lock(g_mutex);
        const auto& list = (tray == Destination::Primary) ? g_primaryOnly : g_icons;
        CHECK_EQ(static_cast<int>(list.size()), 1);
        if (list.empty()) {
            continue;
        }
        const TrayNotification n = Parsed(list[0].payload);
        CHECK((n.flags & NIF_MESSAGE) != 0);
        CHECK_EQ(n.callbackMessage, 0x8001u);
        CHECK_WSTR(n.tip, L"usage 12%");
        CHECK_WSTR(n.exePath, L"C:\\m\\UsageMonitor.exe");
    }
}

void Test_PuttingAnIconBackReplaysItsVersionToo() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x10, 3, NIF_MESSAGE, nullptr, L"C:\\a\\app.exe"),
         &forward);
    Feed(MakeSetVersion(0x10, 3, NOTIFYICON_VERSION_4), &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
        for (auto& icon : g_icons) {
            icon.shellTarget = true;  // as a move to the primary tray does
        }
    }
    auto accept = [](const TrayNotification&) -> LRESULT { return TRUE; };
    std::vector<TrayNotification> handed = SettleAnswering(accept);
    CHECK_EQ(static_cast<int>(handed.size()), 2);
    if (handed.size() == 2) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_SETVERSION));
        CHECK_EQ(handed[1].version, static_cast<DWORD>(NOTIFYICON_VERSION_4));
    }

    // Taking it out is a delete and nothing else.
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto& icon : g_icons) {
            icon.shellTarget = false;
        }
    }
    handed = SettleAnswering(accept);
    CHECK_EQ(static_cast<int>(handed.size()), 1);
    if (handed.size() == 1) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_DELETE));
    }
}

void Test_MultipleIconsFromTheSameWindowAreDistinct() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0xBBBB, 1, NIF_MESSAGE | NIF_TIP, L"one",
                     L"C:\\a\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, 0xBBBB, 2, NIF_MESSAGE | NIF_TIP, L"two",
                     L"C:\\a\\app.exe"),
         &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 2);
    }
    Feed(MakePayload(NIM_DELETE, 0xBBBB, 1, 0, nullptr, L"C:\\a\\app.exe"), &forward);
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK_WSTR(g_icons[0].tip, L"two");
}

void Test_SettingsChangeMovesIconsBetweenTrays() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);

    bool forward = false;
    Feed(MakePayload(NIM_ADD, 0xCCCC, 1, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\apps\\stremio.exe"),
         &forward);
    CHECK(forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
        CHECK_EQ(static_cast<int>(g_icons.size()), 0);
        // The user adds a rule sending this app to the secondary tray.
        g_settings.rules = {{L"stremio.exe", Destination::Secondary}};
    }

    ApplySettingsToTrackedIcons();
    // It must also be retracted from the primary tray, which happens when the
    // taskbar's thread delivers the move.
    CHECK(SettleWithExplorer() == std::vector<DWORD>{NIM_DELETE});

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 0);
    CHECK(g_icons[0].destination == Destination::Secondary);
    CHECK(!g_icons[0].forwardedToShell);
}

// ---------------------------------------------------------------------------
// Per-icon placement (DECISIONS 29)
//
// Placement shipped without any of this, which is how it reached a live run
// unmeasured. The storage these go through is the harness's real in-memory map,
// so a save that silently dropped its value would fail them.
// ---------------------------------------------------------------------------

void ForgetPlacements() {
    g_placements.clear();
    g_placementsLoaded = false;
    g_hidden.clear();
    g_hiddenLoaded = false;
    g_lastSeen.clear();
    g_lastSeenLoaded = false;
    g_lastSeenSweepDay = -1;
    SplitTrayTestHarness::StoredValues().clear();
}

// Simulate a restart: everything in memory goes, the stored string stays.
void ReloadPlacementsFromStorage() {
    g_placements.clear();
    g_placementsLoaded = false;
}

void Test_PlacementKeyPrefersTheGuid() {
    GUID guid = {0x12345678, 0x9ABC, 0xDEF0,
                 {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};
    const std::wstring keyed =
        MakeStableKey(true, guid, L"C:\\apps\\thing.exe", 7);
    CHECK_WSTR(keyed, L"{12345678-9ABC-DEF0-1122334455667788}");

    // The same icon seen from a different path and id is still the same icon.
    CHECK(MakeStableKey(true, guid, L"D:\\other\\renamed.exe", 99) == keyed);
}

void Test_PlacementKeyFallsBackToFileNameAndId() {
    GUID none = {};
    CHECK(MakeStableKey(false, none, L"C:\\apps\\thing.exe", 7) ==
          L"thing.exe#7");
    // Two icons from one application stay distinct.
    CHECK(MakeStableKey(false, none, L"C:\\apps\\thing.exe", 8) ==
          L"thing.exe#8");
    // The full path is not part of the key: the same application installed
    // somewhere else keeps its placement.
    CHECK(MakeStableKey(false, none, L"D:\\elsewhere\\thing.exe", 7) ==
          L"thing.exe#7");
}

void Test_RememberedPlacementBeatsTheRules() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"stremio.exe", Destination::Secondary}};

    // With nothing remembered, the rule decides.
    CHECK(ResolvePlacement(s, L"stremio.exe#1", L"C:\\apps\\stremio.exe") ==
          Destination::Secondary);

    // The user drags it back; that choice now outranks the rule.
    RememberPlacement(L"stremio.exe#1", Destination::Primary);
    CHECK(ResolvePlacement(s, L"stremio.exe#1", L"C:\\apps\\stremio.exe") ==
          Destination::Primary);

    // A different icon of the same application is untouched by that choice.
    CHECK(ResolvePlacement(s, L"stremio.exe#2", L"C:\\apps\\stremio.exe") ==
          Destination::Secondary);
}

void Test_PlacementSurvivesARestart() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;

    RememberPlacement(L"{11111111-2222-3333-4444-555555555555}",
                      Destination::Secondary);
    RememberPlacement(L"thing.exe#3", Destination::Secondary);
    RememberPlacement(L"thing.exe#4", Destination::Primary);

    ReloadPlacementsFromStorage();

    CHECK(ResolvePlacement(s, L"{11111111-2222-3333-4444-555555555555}",
                           L"C:\\apps\\any.exe") == Destination::Secondary);
    CHECK(ResolvePlacement(s, L"thing.exe#3", L"C:\\apps\\thing.exe") ==
          Destination::Secondary);
    CHECK(ResolvePlacement(s, L"thing.exe#4", L"C:\\apps\\thing.exe") ==
          Destination::Primary);
    // Never moved, so the default still decides.
    CHECK(ResolvePlacement(s, L"thing.exe#5", L"C:\\apps\\thing.exe") ==
          Destination::Primary);
}

// From an external review. Placements were saved up to about 7,900 characters
// and read back through an 8K buffer, which Windhawk empties rather than cuts
// when the value does not fit: the placements past the limit were dropped at
// every save, and a last key that took the value past 8K lost every one of them
// at the next start.
void Test_EveryPlacementSurvivesARestartHoweverMany() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;

    const int count = 800;  // about 26,000 characters stored
    auto keyOf = [](int i) {
        return L"application-number-" + std::to_wstring(i) + L".exe#1";
    };
    auto placeOf = [](int i) {
        return (i % 2) ? Destination::Secondary : Destination::Tray(3);
    };
    for (int i = 0; i < count; i++) {
        RememberPlacement(keyOf(i), placeOf(i));
    }

    ReloadPlacementsFromStorage();

    int kept = 0;
    for (int i = 0; i < count; i++) {
        if (ResolvePlacement(s, keyOf(i), L"C:\\apps\\any.exe") == placeOf(i)) {
            kept++;
        }
    }
    CHECK_EQ(kept, count);
}

void Test_AStoredStringIsReadWhateverItsLength() {
    ForgetPlacements();
    CHECK(ReadStoredString(L"nothing stored here").empty());

    const std::wstring small = L"a=1";
    WriteStoredString(L"small", small);
    CHECK(ReadStoredString(L"small") == small);

    // Larger than every buffer the reader starts with but the last.
    const std::wstring large(300000, L'x');
    WriteStoredString(L"large", large);
    CHECK(ReadStoredString(L"large") == large);
}

// ---------------------------------------------------------------------------
// Forgetting icons not seen for a year (DECISIONS 90)
//
// What the mod remembers about an icon - where it was moved, that it was
// hidden, its place in the taskbar's order - was kept for ever, for icons whose
// applications had long gone. The day is the tests' own (g_currentDay).
// ---------------------------------------------------------------------------

int g_testDay = 20000;

int TestDay() {
    return g_testDay;
}

// A restart: memory goes, storage stays.
void ForgetMemoryOfIcons() {
    g_placements.clear();
    g_placementsLoaded = false;
    g_hidden.clear();
    g_hiddenLoaded = false;
    g_lastSeen.clear();
    g_lastSeenLoaded = false;
    g_lastSeenSweepDay = -1;
}

void ForgetLongUnseen() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ForgetIconsLongUnseenLocked();
}

void Test_WhatIsRememberedOfAnIconNotSeenForAYearIsForgotten() {
    ForgetPlacements();
    g_currentDay = TestDay;
    g_testDay = 20000;
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;

    RememberPlacement(L"old.exe#1", Destination::Secondary);
    RememberPlacement(L"recent.exe#1", Destination::Secondary);
    SetIconHidden(L"old.exe#1", true);
    SetIconHidden(L"recent.exe#1", true);
    auto& stored = SplitTrayTestHarness::StoredValues();
    stored[kIconOrderValue] = L"old.exe#1\nrecent.exe#1";
    // Seen 366 days ago, and 365 days ago: a year is the limit.
    stored[kLastSeenValue] = L"old.exe#1=19634\nrecent.exe#1=19635";
    ForgetMemoryOfIcons();

    ForgetLongUnseen();

    CHECK(ResolvePlacement(s, L"old.exe#1", L"C:\\a\\old.exe") == Destination::Primary);
    CHECK(ResolvePlacement(s, L"recent.exe#1", L"C:\\a\\recent.exe") ==
          Destination::Secondary);
    CHECK(!IsIconHidden(L"old.exe#1"));
    CHECK(IsIconHidden(L"recent.exe#1"));
    CHECK_WSTR(stored[kIconOrderValue], L"recent.exe#1");
    CHECK_WSTR(stored[kLastSeenValue], L"recent.exe#1=19635");

    // Forgotten for good: the next start has nothing of it to read back.
    ForgetMemoryOfIcons();
    ForgetLongUnseen();
    CHECK(ResolvePlacement(s, L"old.exe#1", L"C:\\a\\old.exe") == Destination::Primary);
    CHECK(ResolvePlacement(s, L"recent.exe#1", L"C:\\a\\recent.exe") ==
          Destination::Secondary);
    g_currentDay = CurrentDay;
}

void Test_AnUpgradeForgetsNothingAndStartsTheYearThen() {
    // Everything the mod remembered before this version has no sighting. It is
    // given one, today, rather than taken for long gone.
    ForgetPlacements();
    g_currentDay = TestDay;
    g_testDay = 20000;
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    RememberPlacement(L"kept.exe#1", Destination::Secondary);
    SetIconHidden(L"hidden.exe#1", true);
    auto& stored = SplitTrayTestHarness::StoredValues();
    stored[kIconOrderValue] = L"ordered.exe#1";
    ForgetMemoryOfIcons();

    ForgetLongUnseen();
    CHECK(ResolvePlacement(s, L"kept.exe#1", L"C:\\a\\kept.exe") == Destination::Secondary);
    CHECK(IsIconHidden(L"hidden.exe#1"));
    CHECK_WSTR(stored[kIconOrderValue], L"ordered.exe#1");
    CHECK_WSTR(stored[kLastSeenValue],
               L"hidden.exe#1=20000\nkept.exe#1=20000\nordered.exe#1=20000");

    // A year and a day later, with none of them seen since, they go.
    g_testDay = 20000 + 366;
    ForgetMemoryOfIcons();
    ForgetLongUnseen();
    CHECK(ResolvePlacement(s, L"kept.exe#1", L"C:\\a\\kept.exe") == Destination::Primary);
    CHECK(!IsIconHidden(L"hidden.exe#1"));
    CHECK_WSTR(stored[kIconOrderValue], L"");
    CHECK_WSTR(stored[kLastSeenValue], L"");
    g_currentDay = CurrentDay;
}

void Test_AnIconIsSeenWhenItArrivesAndEveryDayItStays() {
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"mirrored.exe", Destination::Secondary}};
    ResetStore(s, true);
    g_currentDay = TestDay;
    g_testDay = 20000;
    auto& stored = SplitTrayTestHarness::StoredValues();
    auto& writes = SplitTrayTestHarness::StoredValueWrites();

    bool forward = false;
    // One in Split Tray's trays, one left to Explorer: both are seen.
    Feed(MakePayload(NIM_ADD, 0x1001, 1, NIF_MESSAGE | NIF_TIP, L"m", L"C:\\a\\mirrored.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, 0x1002, 1, NIF_MESSAGE | NIF_TIP, L"p", L"C:\\a\\primary.exe"),
         &forward);
    CHECK_WSTR(stored[kLastSeenValue], L"mirrored.exe#1=20000\nprimary.exe#1=20000");

    // Every message of the day after that writes nothing: the taskbar's thread
    // sees an application's updates several times a second.
    const int before = writes[kLastSeenValue];
    for (int i = 0; i < 50; i++) {
        Feed(MakePayload(NIM_MODIFY, 0x1001, 1, NIF_TIP, L"m2", L"C:\\a\\mirrored.exe"),
             &forward);
    }
    CHECK_EQ(writes[kLastSeenValue], before);

    // Nor does an icon added again the same day: an application restarting,
    // or every application at once when Explorer does.
    Feed(MakePayload(NIM_DELETE, 0x1002, 1, 0, nullptr, L"C:\\a\\primary.exe"), &forward);
    Feed(MakePayload(NIM_ADD, 0x1002, 1, NIF_MESSAGE | NIF_TIP, L"p", L"C:\\a\\primary.exe"),
         &forward);
    CHECK_EQ(writes[kLastSeenValue], before);

    // The next day, the first message stamps every icon there is, whichever
    // it is about, and once.
    g_testDay = 20001;
    Feed(MakePayload(NIM_MODIFY, 0x1001, 1, NIF_TIP, L"m3", L"C:\\a\\mirrored.exe"), &forward);
    Feed(MakePayload(NIM_MODIFY, 0x1002, 1, NIF_TIP, L"p3", L"C:\\a\\primary.exe"), &forward);
    CHECK_WSTR(stored[kLastSeenValue], L"mirrored.exe#1=20001\nprimary.exe#1=20001");
    CHECK_EQ(writes[kLastSeenValue], before + 1);
    g_currentDay = CurrentDay;
}

// ---------------------------------------------------------------------------
// Choosing which icons live in the overflow
// ---------------------------------------------------------------------------

void ForgetHidden() {
    g_hidden.clear();
    g_hiddenLoaded = false;
    SplitTrayTestHarness::StoredValues().erase(kHiddenValue);
}

void Test_HiddenIconsSurviveARestart() {
    ForgetHidden();
    SetIconHidden(L"a.exe#1", true);
    SetIconHidden(L"{11111111-2222-3333-4444-555555555555}", true);
    SetIconHidden(L"b.exe#2", true);
    SetIconHidden(L"b.exe#2", false);  // hidden, then shown again

    // Simulate a restart: memory goes, storage stays.
    g_hidden.clear();
    g_hiddenLoaded = false;

    CHECK(IsIconHidden(L"a.exe#1"));
    CHECK(IsIconHidden(L"{11111111-2222-3333-4444-555555555555}"));
    CHECK(!IsIconHidden(L"b.exe#2"));
    CHECK(!IsIconHidden(L"never.exe#1"));
}

// The same limit as placements had, on the icons kept in the overflow.
void Test_EveryHiddenIconSurvivesARestartHoweverMany() {
    ForgetHidden();
    const int count = 800;
    auto keyOf = [](int i) {
        return L"application-number-" + std::to_wstring(i) + L".exe#1";
    };
    for (int i = 0; i < count; i++) {
        SetIconHidden(keyOf(i), true);
    }

    g_hidden.clear();
    g_hiddenLoaded = false;

    int kept = 0;
    for (int i = 0; i < count; i++) {
        if (IsIconHidden(keyOf(i))) {
            kept++;
        }
    }
    CHECK_EQ(kept, count);
}

void Test_ShowingEveryHiddenIconClearsStorageToo() {
    ForgetHidden();
    SetIconHidden(L"a.exe#1", true);
    ShowAllHiddenIcons();
    g_hidden.clear();
    g_hiddenLoaded = false;
    CHECK(!IsIconHidden(L"a.exe#1"));
}

void Test_TheUsersChoiceDecidesTheOverflowBeforeTheCount() {
    const std::vector<std::wstring> keys = {L"a", L"b", L"c", L"d", L"e"};
    auto hideB = [](std::wstring const& key) { return key == L"b"; };

    // Plenty of room: only the icon the user hid goes into the overflow.
    BarSplit roomy = SplitBarAndOverflow(keys, hideB, 8);
    CHECK_EQ(static_cast<int>(roomy.shown.size()), 4);
    CHECK_EQ(static_cast<int>(roomy.overflow.size()), 1);
    CHECK_EQ(static_cast<int>(roomy.overflow[0]), 1);

    // Room for two: the hidden one is still hidden, and the limit is applied
    // to the icons the user wanted shown - not to the row as a whole, which
    // would have spent a slot on the icon they asked to hide.
    BarSplit tight = SplitBarAndOverflow(keys, hideB, 2);
    CHECK_EQ(static_cast<int>(tight.shown.size()), 2);
    CHECK_EQ(static_cast<int>(tight.shown[0]), 0);  // a
    CHECK_EQ(static_cast<int>(tight.shown[1]), 2);  // c, not b
    CHECK_EQ(static_cast<int>(tight.overflow.size()), 3);

    // No limit: nothing overflows unless it was hidden.
    auto hideNone = [](std::wstring const&) { return false; };
    BarSplit unlimited = SplitBarAndOverflow(keys, hideNone, 0);
    CHECK_EQ(static_cast<int>(unlimited.shown.size()), 5);
    CHECK_EQ(static_cast<int>(unlimited.overflow.size()), 0);
}

void Test_ArrangeWindowDefaultMoveIsBetweenTheTrays() {
    // A list per tray: the primary tray, tray 2, tray 3. The lists are what
    // the window made, so a stand-in for each is enough here.
    g_arrangeTrays = {1, 2, 3};
    g_arrangeLists.assign(3, reinterpret_cast<HWND>(1));
    CHECK_EQ(ArrangeDefaultTarget(0), 1);   // primary -> tray 2
    CHECK_EQ(ArrangeDefaultTarget(1), 0);   // tray 2 -> primary
    CHECK_EQ(ArrangeDefaultTarget(2), 0);   // tray 3 -> primary
    CHECK_EQ(ArrangeDefaultTarget(7), -1);
    CHECK_EQ(ArrangeListOfTray(3), 2);      // the key "3" finds tray 3's list
    CHECK_EQ(ArrangeListOfTray(4), -1);

    // With only the primary tray there is nowhere to move to.
    g_arrangeTrays = {1};
    g_arrangeLists.assign(1, reinterpret_cast<HWND>(1));
    CHECK_EQ(ArrangeDefaultTarget(0), -1);

    g_arrangeTrays.clear();
    g_arrangeLists.clear();
}

void Test_ForgettingPlacementsGivesTheRulesBack() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"app.exe", Destination::Secondary}};

    RememberPlacement(L"app.exe#1", Destination::Primary);
    CHECK(ResolvePlacement(s, L"app.exe#1", L"C:\\apps\\app.exe") ==
          Destination::Primary);

    ForgetAllPlacements();
    CHECK(ResolvePlacement(s, L"app.exe#1", L"C:\\apps\\app.exe") ==
          Destination::Secondary);

    // And it is forgotten in storage, not just in memory.
    ReloadPlacementsFromStorage();
    CHECK(ResolvePlacement(s, L"app.exe#1", L"C:\\apps\\app.exe") ==
          Destination::Secondary);
}

void Test_MoveIconToTrayRetractsItFromTheSecondaryTray() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0xD1D1, 4, NIF_MESSAGE | NIF_TIP, L"thing",
                     L"C:\\apps\\thing.exe"),
         &forward);
    CHECK(!forward);  // swallowed: it belongs to the secondary tray

    std::wstring key;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
        key = StableKeyOf(g_icons[0]);
    }
    CHECK_WSTR(key, L"thing.exe#4");

    MoveIconToTray(key, Destination::Primary);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 0);
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
        CHECK(g_primaryOnly[0].destination == Destination::Primary);
    }

    // And the move is remembered, so the icon does not spring back when its
    // application re-registers.
    ReloadPlacementsFromStorage();
    CHECK(ResolvePlacement(s, key, L"C:\\apps\\thing.exe") ==
          Destination::Primary);
}

// ---------------------------------------------------------------------------
// More than one of Split Tray's trays
//
// Tray 2 is the display on the left; tray 3 an extra tray on the primary
// display, which is how the live test gets a third tray out of two displays.
// ---------------------------------------------------------------------------

Settings ThreeTraySettings() {
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft}};
    return s;
}

// Which of Split Tray's trays shows the icon with this key: 0 for none, -1 if
// the mod does not know it.
int ShownTrayOf(std::wstring_view key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (const auto& icon : *list) {
            if (StableKeyOf(icon) == key) {
                return icon.shownTray;
            }
        }
    }
    return -1;
}

// Whether Explorer has the icon once the moves asked for so far have reached it.
bool ForwardedToShell(std::wstring_view key) {
    SettleWithExplorer();
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (const auto& icon : *list) {
            if (StableKeyOf(icon) == key) {
                return icon.forwardedToShell;
            }
        }
    }
    return false;
}

void Test_ARuleCanSendAnIconToAnyTray() {
    ForgetPlacements();
    Settings s = ThreeTraySettings();
    s.rules = {{L"telemachus.exe", Destination::Tray(3)},
               {L"stremio.exe", Destination::Secondary}};
    ResetStore(s, true);

    bool forward = true;
    CHECK(Feed(MakePayload(NIM_ADD, 0x3131, 2, NIF_MESSAGE | NIF_TIP, L"Telemachus",
                           L"C:\\apps\\telemachus.exe"),
               &forward));
    CHECK(!forward);
    CHECK(Feed(MakePayload(NIM_ADD, 0x3232, 7, NIF_MESSAGE | NIF_TIP, L"Stremio",
                           L"C:\\apps\\stremio.exe"),
               &forward));
    CHECK(!forward);

    CHECK_EQ(ShownTrayOf(L"telemachus.exe#2"), 3);
    CHECK_EQ(ShownTrayOf(L"stremio.exe#7"), 2);

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(IconsInTrayLocked(2).size()), 1);
    CHECK_EQ(static_cast<int>(IconsInTrayLocked(3).size()), 1);
    RecomputeGeometryLocked(g_enumerateMonitors());
    // Each floating tray is laid out on its own display, at its own corner.
    CHECK(g_floatingLayouts.count(2) == 1);
    CHECK(g_floatingLayouts.count(3) == 1);
    const TrayLayout& two = g_floatingLayouts[2];
    const TrayLayout& three = g_floatingLayouts[3];
    CHECK_EQ(two.x, kSecondaryWorkArea.right - 8 - two.width);
    CHECK_EQ(three.x, kPrimaryWorkArea.left + 8);
    CHECK_EQ(three.y, kPrimaryWorkArea.bottom - 8 - three.height);
}

void Test_AnEmptyFloatingTrayStillHasAHandle() {
    ResetStore(ThreeTraySettings(), true);
    std::lock_guard<std::mutex> lock(g_mutex);
    RecomputeGeometryLocked(g_enumerateMonitors());
    // Nothing routed there yet, and still somewhere to click for the menu.
    CHECK(g_floatingLayouts.count(3) == 1);
    CHECK_EQ(g_floatingLayouts[3].columns, 1);
    CHECK(g_floatingLayouts[3].width > 0);
}

void Test_MovingBetweenSplitTraysTraysLeavesTheShellAlone() {
    ForgetPlacements();
    Settings s = ThreeTraySettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x4141, 5, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\apps\\app.exe"),
         &forward);
    const std::wstring key = L"app.exe#5";
    CHECK_EQ(ShownTrayOf(key), 2);

    MoveIconToTray(key, Destination::Tray(3));
    CHECK_EQ(ShownTrayOf(key), 3);
    CHECK(!ForwardedToShell(key));  // never went through Explorer's tray

    // Remembered by number.
    ReloadPlacementsFromStorage();
    CHECK(ResolvePlacement(s, key, L"C:\\apps\\app.exe") == Destination::Tray(3));

    MoveIconToTray(key, Destination::Primary);
    CHECK_EQ(ShownTrayOf(key), 0);
    CHECK(ForwardedToShell(key));
}

void Test_AnIconWhoseTrayGoesAwayFallsBackAndReturns() {
    // Requirement 7, with trays by number: unplug the display and its icons go
    // to the primary tray; plug it back and they return.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5151, 1, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\apps\\app.exe"),
         &forward);
    const std::wstring key = L"app.exe#1";
    CHECK_EQ(ShownTrayOf(key), 2);
    CHECK(!ForwardedToShell(key));

    g_testSecondaryConnected = false;
    ApplySettingsToTrackedIcons();
    CHECK_EQ(ShownTrayOf(key), 0);
    CHECK(ForwardedToShell(key));
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        // The display's tray keeps its number, not there (DECISIONS 86).
        CHECK(g_trays.size() == 1 && g_trays[0].number == 2 && !g_trays[0].available);
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
        // Where it belongs is unchanged; only where it can be is.
        CHECK(g_primaryOnly[0].destination == Destination::Secondary);
    }

    g_testSecondaryConnected = true;
    ApplySettingsToTrackedIcons();
    CHECK_EQ(ShownTrayOf(key), 2);
    CHECK(!ForwardedToShell(key));
}

void Test_TrayNumbersAreRememberedAcrossRestarts() {
    // DECISIONS 86. Which display has which number is kept with the mod's
    // other remembered state, so a display unplugged while Explorer was not
    // running still has its number when the mod next loads.
    Settings s = DefaultSettings();
    ResetStore(s, true);
    g_enumerateMonitors = [] { return std::vector<MonitorInfoEntry>{kLeft, kMiddle, kMain}; };
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecomputeGeometryLocked(g_enumerateMonitors());
    }
    CHECK(!SplitTrayTestHarness::StoredValues()[L"displaySlots"].empty());

    // A restart, with the display on the left gone.
    g_enumerateMonitors = [] { return std::vector<MonitorInfoEntry>{kMiddle, kMain}; };
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_displaySlots.clear();
        g_displaySlotsLoaded = false;
        RecomputeGeometryLocked(g_enumerateMonitors());
        const TrayTarget* two = FindTrayLocked(2);
        const TrayTarget* three = FindTrayLocked(3);
        CHECK(two && !two->available);
        CHECK(three && three->available && three->monitor.id == L"middle");
    }
    g_enumerateMonitors = TestMonitors;
}

void Test_AnIconItsApplicationHidStaysHiddenAfterASettingsChange() {
    // mirrorHiddenIcons off keeps an NIS_HIDDEN icon out of Split Tray's trays
    // when it arrives; re-sorting the icons after a settings change or a
    // display change must not bring it back.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.mirrorHiddenIcons = false;
    ResetStore(s, true);

    std::vector<BYTE> hidden = MakePayload(NIM_ADD, 0x8181, 1,
                                           NIF_MESSAGE | NIF_TIP | NIF_STATE, L"hidden",
                                           L"C:\\apps\\quiet.exe");
    const DWORD state = NIS_HIDDEN;
    memcpy(hidden.data() + wire::kState, &state, sizeof(state));
    memcpy(hidden.data() + wire::kStateMask, &state, sizeof(state));
    bool forward = true;
    Feed(hidden, &forward);
    CHECK_EQ(ShownTrayOf(L"quiet.exe#1"), 0);

    ApplySettingsToTrackedIcons();
    CHECK_EQ(ShownTrayOf(L"quiet.exe#1"), 0);
}

void Test_AnIconForATrayThatIsNotThereWaitsInThePrimaryTray() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.extraTrays = {{9, Corner::BottomLeft}};  // tray 3, on a display not there
    s.rules = {{L"app.exe", Destination::Tray(3)}};
    ResetStore(s, true);

    bool forward = false;
    Feed(MakePayload(NIM_ADD, 0x6161, 1, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\apps\\app.exe"),
         &forward);
    CHECK(forward);
    CHECK_EQ(ShownTrayOf(L"app.exe#1"), 0);
}

void Test_ADisabledTrayIsNotDrawnAndItsIconsWaitInThePrimaryTray() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.extraTrays = {{0, Corner::BottomLeft, true}};  // tray 3, switched off
    s.rules = {{L"app.exe", Destination::Tray(3)}};
    ResetStore(s, true);

    bool forward = false;
    Feed(MakePayload(NIM_ADD, 0x6262, 1, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\apps\\app.exe"),
         &forward);
    CHECK(forward);
    CHECK_EQ(ShownTrayOf(L"app.exe#1"), 0);
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK(g_floatingLayouts.count(3) == 0);  // not even the empty-tray handle
    CHECK(!TrayAvailableLocked(3));
}

void Test_EveryIconHasItsOwnSerial() {
    // Two running copies of one application share a placement key - and used
    // to share a way of being found, which is how a click could reach the
    // wrong one. The serial is what a tray cell carries now.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x7171, 1, NIF_MESSAGE | NIF_TIP, L"first",
                     L"C:\\apps\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, 0x7272, 1, NIF_MESSAGE | NIF_TIP, L"second",
                     L"C:\\apps\\app.exe"),
         &forward);

    uint64_t first = 0;
    uint64_t second = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 2);
        CHECK(StableKeyOf(g_icons[0]) == StableKeyOf(g_icons[1]));
        first = g_icons[0].serial;
        second = g_icons[1].serial;
        CHECK(first != 0);
        CHECK(second != 0);
        CHECK(first != second);
        CHECK_EQ(IndexOfSerialLocked(first), 0);
        CHECK_EQ(IndexOfSerialLocked(second), 1);
        CHECK_EQ(IndexOfSerialLocked(0), -1);
    }

    // The store's order changes when icons move out and back; the serial
    // still finds the same icon.
    MoveIconToTray(L"app.exe#1", Destination::Primary);
    MoveIconToTray(L"app.exe#1", Destination::Secondary);
    std::lock_guard<std::mutex> lock(g_mutex);
    const int index = IndexOfSerialLocked(second);
    CHECK(index >= 0);
    if (index >= 0) {
        CHECK_WSTR(g_icons[static_cast<size_t>(index)].tip, L"second");
    }
}

// ---------------------------------------------------------------------------
// An empty GUID is not an identity
//
// From a live run: all four of SystemInformer's icons arrive with NIF_GUID set
// over an all-zero GUID, from one window, with uIDs 2, 3, 5 and 14. Matching on
// the flag alone made them one icon, so the rule placed uID 2 and the other
// three inherited that entry as a sticky decision instead of being routed.
// ---------------------------------------------------------------------------

void Test_EmptyGuidDoesNotMakeEveryIconTheSameIcon() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"SystemInformer.exe", Destination::Secondary}};
    ResetStore(s, true);

    // One window, four ids, NIF_GUID set with nothing in the field - exactly
    // what the live capture showed.
    const HWND owner = reinterpret_cast<HWND>(0x204E4);
    const GUID empty = {};
    for (UINT uID : {2u, 3u, 5u, 14u}) {
        bool forward = true;
        auto payload = MakePayload(NIM_ADD, reinterpret_cast<UINT_PTR>(owner),
                                   uID, NIF_MESSAGE | NIF_TIP | NIF_GUID,
                                   L"SystemInformer",
                                   L"C:\\Program Files\\SystemInformer\\SystemInformer.exe",
                                   &empty);
        Feed(payload, &forward);
        CHECK(!forward);  // every one of them belongs to the secondary tray
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 4);
    CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 0);
    // And they are four distinct icons, not one seen four times.
    CHECK(StableKeyOf(g_icons[0]) != StableKeyOf(g_icons[1]));
    CHECK_WSTR(StableKeyOf(g_icons[0]), L"SystemInformer.exe#2");
    CHECK_WSTR(StableKeyOf(g_icons[3]), L"SystemInformer.exe#14");
}

void Test_ARealGuidStillIdentifiesAnIconAcrossWindows() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    const GUID guid = {0xAABBCCDD, 0x1122, 0x3344,
                       {0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC}};
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x1000, 7, NIF_MESSAGE | NIF_TIP | NIF_GUID,
                     L"first", L"C:\\apps\\app.exe", &guid),
         &forward);
    // Same GUID, different window and id: still the same icon, which is what a
    // GUID is for - it survives the owning window being recreated.
    Feed(MakePayload(NIM_MODIFY, 0x2000, 99, NIF_TIP | NIF_GUID, L"second",
                     L"C:\\apps\\app.exe", &guid),
         &forward);

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK_WSTR(g_icons[0].tip, L"second");
}

// ---------------------------------------------------------------------------
// A modify arriving before the add must not freeze the routing
//
// The reason the secondary tray came up empty on a live run. SystemInformer
// redraws four live graphs every second, so the first message the mod saw about
// uIDs 3, 5 and 14 was a NIM_MODIFY - which carries no executable path, so the
// rule had nothing to match and they were filed under the default tray. Sticky
// routing then made that permanent, and the later NIM_ADD carrying the path was
// taken as "already decided". uID 2 only worked because its add happened to come
// first.
// ---------------------------------------------------------------------------

void Test_AModifyBeforeTheAddDoesNotFreezeTheWrongTray() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"SystemInformer.exe", Destination::Secondary}};
    ResetStore(s, true);

    // The modify comes first and carries no path, as the real ones do not.
    bool forward = true;
    Feed(MakePayload(NIM_MODIFY, 0x204E4, 3, NIF_MESSAGE | NIF_TIP, L"I/O",
                     L""),
         &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
        // Provisional, not decided: there was nothing to decide from.
        CHECK(!g_primaryOnly[0].destinationDecided);
    }

    // Now the add, with the path. The rule must take effect.
    Feed(MakePayload(NIM_ADD, 0x204E4, 3, NIF_MESSAGE | NIF_TIP, L"I/O",
                     L"C:\\Program Files\\SystemInformer\\SystemInformer.exe"),
         &forward);

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK(g_primaryOnly.empty() || g_primaryOnly[0].destination ==
                                       Destination::Secondary);
    CHECK_EQ(static_cast<int>(g_icons.size()) +
                 static_cast<int>(g_primaryOnly.size()),
             1);
    const MirroredIcon& icon =
        g_icons.empty() ? g_primaryOnly[0] : g_icons[0];
    CHECK(icon.destination == Destination::Secondary);
    CHECK(icon.destinationDecided);
}

void Test_ASettingsChangeKeepsIconsTheUserMoved() {
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"app.exe", Destination::Secondary}};
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x3000, 1, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\apps\\app.exe"),
         &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    }

    // The user drags it back to the primary tray, then something else changes
    // the settings. The hand-made choice has to survive that: re-resolving
    // straight from the rules is what used to undo it.
    MoveIconToTray(L"app.exe#1", Destination::Primary);
    ApplySettingsToTrackedIcons();

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 0);
    CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
    CHECK(g_primaryOnly[0].destination == Destination::Primary);
}

// ---------------------------------------------------------------------------
// Dragging a cell along the row
//
// The anchor for the defect that made drag-to-reorder do nothing: the row was
// rearranged by removing the dragged cell and re-inserting it, which takes it
// out of the visual tree, loses its pointer capture, and ends the drag on its
// first step - after which the release reads as a click on whatever the icon
// was dropped on.
// ---------------------------------------------------------------------------

std::wstring ApplyShift(std::wstring row, size_t from, size_t to,
                        bool* draggedWasRemoved) {
    const wchar_t dragged = row[from];
    *draggedWasRemoved = false;
    for (const auto& step : PlanCellShift(from, to)) {
        if (row[step.removeAt] == dragged) {
            *draggedWasRemoved = true;
        }
        const wchar_t moved = row[step.removeAt];
        row.erase(step.removeAt, 1);
        row.insert(step.insertAt, 1, moved);
    }
    return row;
}

void Test_CellShiftMovesTheCellWithoutEverRemovingIt() {
    const std::wstring row = L"ABCDEF";
    for (size_t from = 0; from < row.size(); from++) {
        for (size_t to = 0; to < row.size(); to++) {
            bool removed = false;
            const std::wstring result = ApplyShift(row, from, to, &removed);

            // The dragged cell lands where it was dropped.
            CHECK_EQ(static_cast<int>(result.find(row[from])),
                     static_cast<int>(to));
            // Every other cell is still present, in its original relative order.
            std::wstring others = result;
            others.erase(others.find(row[from]), 1);
            std::wstring expected = row;
            expected.erase(from, 1);
            CHECK_WSTR(others, expected);
            // The invariant that makes the drag survive: the cell under the
            // pointer is never the one taken out of the panel.
            CHECK(!removed);
        }
    }
}

void Test_CellShiftToItsOwnSlotDoesNothing() {
    CHECK_EQ(static_cast<int>(PlanCellShift(3, 3).size()), 0);
    CHECK_EQ(static_cast<int>(PlanCellShift(0, 1).size()), 1);
    CHECK_EQ(static_cast<int>(PlanCellShift(4, 1).size()), 3);
}

// Explorer announces each taskbar it creates, once its tray is ready. Asking on
// top of that made applications register twice, the first time into a tray
// that was not ready - and an icon that answered only the first was lost.
void Test_OnlyAsksAppsToReRegisterWhenExplorerWillNot() {
    // Loaded into a running Explorer: its announcement is long past.
    CHECK(ShouldAskAppsToReRegister(/*shellExistedAtLoad=*/true, /*firstAttach=*/true,
                                    /*missed=*/false));
    // Explorer starting up: it will announce the taskbar itself.
    CHECK(!ShouldAskAppsToReRegister(false, true, false));
    // A taskbar recreated later is announced by Explorer too.
    CHECK(!ShouldAskAppsToReRegister(true, false, false));
    CHECK(!ShouldAskAppsToReRegister(false, false, false));
    // Unless the announcement went out before the mod was watching.
    CHECK(ShouldAskAppsToReRegister(false, true, true));
    CHECK(ShouldAskAppsToReRegister(true, false, true));
}

// Explorer runs folder windows in processes of their own, and Windhawk loads
// the mod into every one. Only the process that shows the taskbar is the
// mod's: one that found the taskbar in another drew tray 2 again, empty,
// floating beside the real one.
void Test_TheTaskbarIsThisProcessesOnlyIfItOwnsOne() {
    constexpr DWORD self = 40;
    CHECK(WhoShowsTheTaskbar({}, self) == TaskbarShownBy::Nobody);
    CHECK(WhoShowsTheTaskbar({self}, self) == TaskbarShownBy::ThisProcess);
    CHECK(WhoShowsTheTaskbar({12}, self) == TaskbarShownBy::AnotherProcess);
    // Its own, wherever it comes among the windows of that class.
    CHECK(WhoShowsTheTaskbar({12, self}, self) == TaskbarShownBy::ThisProcess);
    CHECK(WhoShowsTheTaskbar({self, 12}, self) == TaskbarShownBy::ThisProcess);
    CHECK(WhoShowsTheTaskbar({12, 13}, self) == TaskbarShownBy::AnotherProcess);
}

void Test_FileNameOfHandlesEveryShape() {
    CHECK(FileNameOf(L"C:\\a\\b\\c.exe") == L"c.exe");
    CHECK(FileNameOf(L"c.exe") == L"c.exe");
    CHECK(FileNameOf(L"C:/a/b/c.exe") == L"c.exe");
    CHECK(FileNameOf(L"") == L"");
    CHECK(FileNameOf(L"C:\\a\\") == L"");
}

}  // namespace

// ---------------------------------------------------------------------------
// Findings from the external review of 2026-09-23
//
// Each of these was reproduced against the shipped source before it was fixed.
// ---------------------------------------------------------------------------

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

// The handle a view holds, whether it keeps a bare HICON or owns its copy.
template <typename T>
HICON HandleOf(const T& held) {
    if constexpr (std::is_same_v<T, HICON>) {
        return held;
    } else {
        return held.get();
    }
}

std::vector<BYTE> WithState(std::vector<BYTE> payload, DWORD state, DWORD mask) {
    memcpy(payload.data() + wire::kState, &state, sizeof(state));
    memcpy(payload.data() + wire::kStateMask, &mask, sizeof(mask));
    return payload;
}

std::vector<BYTE> WithIcon(std::vector<BYTE> payload, HICON icon) {
    const DWORD handle = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(icon));
    memcpy(payload.data() + wire::kIcon, &handle, sizeof(handle));
    return payload;
}

void Test_ATooltipUpdateDoesNotRevealAnIconItsApplicationHid() {
    // Whether an icon is hidden is a property of the icon, not of the last
    // message. It was read from the message alone, so a modify that did not
    // mention the state - a new tooltip - read as "not hidden" and put the
    // icon back in the tray while its stored state still said NIS_HIDDEN.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.mirrorHiddenIcons = false;
    ResetStore(s, true);

    bool forward = true;
    Feed(WithState(MakePayload(NIM_ADD, 0x6161, 4, NIF_MESSAGE | NIF_TIP | NIF_STATE,
                               L"hidden", L"C:\\a\\app.exe"),
                   NIS_HIDDEN, NIS_HIDDEN),
         &forward);
    Feed(MakePayload(NIM_MODIFY, 0x6161, 4, NIF_TIP, L"still hidden", nullptr),
         &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 0);
        CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 1);
        if (!g_primaryOnly.empty()) {
            CHECK_WSTR(g_primaryOnly[0].tip, L"still hidden");
        }
    }

    // A state change that leaves NIS_HIDDEN out of its mask does not touch it.
    Feed(WithState(MakePayload(NIM_MODIFY, 0x6161, 4, NIF_STATE, nullptr, nullptr),
                   0, NIS_SHAREDICON),
         &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 0);
    }

    // Unhiding it is what brings it back.
    Feed(WithState(MakePayload(NIM_MODIFY, 0x6161, 4, NIF_STATE, nullptr, nullptr), 0,
                   NIS_HIDDEN),
         &forward);
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    CHECK_EQ(static_cast<int>(g_primaryOnly.size()), 0);
}

void Test_AnIconReRegisteredByGuidIsClickedInItsNewWindow() {
    // An application that restarts registers the same GUID from a new window.
    // The stored record took the new owner, but the icon's own owner and uID -
    // where clicks are sent, and what the watchdog checks is still alive - kept
    // the old ones.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    const GUID guid = {0x0BADF00D, 0x1234, 0x5678,
                       {0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78}};
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x1000, 7, NIF_MESSAGE | NIF_TIP | NIF_GUID, L"before",
                     L"C:\\apps\\app.exe", &guid),
         &forward);
    uint64_t serial = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
        if (!g_icons.empty()) {
            serial = g_icons[0].serial;
        }
    }

    Feed(MakePayload(NIM_ADD, 0x2000, 9, NIF_MESSAGE | NIF_TIP | NIF_GUID, L"after",
                     L"C:\\apps\\app.exe", &guid),
         &forward);

    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK_EQ(static_cast<int>(g_icons.size()), 1);
    if (!g_icons.empty()) {
        CHECK_EQ(reinterpret_cast<ULONG_PTR>(g_icons[0].ownerWnd),
                 static_cast<ULONG_PTR>(0x2000));
        CHECK_EQ(g_icons[0].uID, 9u);
        // Still the same icon: the same cell, the same place.
        CHECK_EQ(g_icons[0].serial, serial);
        CHECK_EQ(g_icons[0].shownTray, 2);
    }
}

void Test_AFloatingTrayDrawsFromItsOwnCopyOfEachIcon() {
    // The floating trays are painted on the mod's thread from a snapshot taken
    // under the lock, while icons are updated on the taskbar's thread - which
    // destroys the picture it replaces. A snapshot of bare handles could be
    // drawing an icon that no longer exists.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    HICON first = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    HICON second = CopyIcon(LoadIconW(nullptr, IDI_WARNING));
    bool forward = true;
    Feed(WithIcon(MakePayload(NIM_ADD, 0x7171, 1, NIF_MESSAGE | NIF_TIP | NIF_ICON,
                              L"app", L"C:\\a\\app.exe"),
                  first),
         &forward);

    const FloatingView view = FloatingViewOf(2);
    CHECK_EQ(static_cast<int>(view.icons.size()), 1);

    // The application changes its picture while the tray is being painted.
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x7171, 1, NIF_ICON, nullptr, nullptr),
                  second),
         &forward);

    if (!view.icons.empty()) {
        CHECK(IconIsAlive(HandleOf(view.icons[0])));
    }
    DestroyIcon(first);
    DestroyIcon(second);
}

void Test_AnEmbeddedTrayDrawsFromItsOwnCopyOfEachIcon() {
    // The same, for a tray in a taskbar: drawn on the taskbar's thread, where
    // the store replaces pictures too - but the tray thread's watchdog destroys
    // an icon's picture when its application goes, and the overflow popup
    // draws what the last refresh kept, whenever it is opened.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    HICON first = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    HICON second = CopyIcon(LoadIconW(nullptr, IDI_WARNING));
    bool forward = true;
    Feed(WithIcon(MakePayload(NIM_ADD, 0x7272, 1, NIF_MESSAGE | NIF_TIP | NIF_ICON,
                              L"app", L"C:\\a\\app.exe"),
                  first),
         &forward);

    const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
    CHECK_EQ(static_cast<int>(cells.size()), 1);
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x7272, 1, NIF_ICON, nullptr, nullptr),
                  second),
         &forward);
    if (!cells.empty()) {
        CHECK(IconIsAlive(HandleOf(cells[0].icon)));
        CHECK_WSTR(cells[0].key, L"app.exe#1");
    }
    DestroyIcon(first);
    DestroyIcon(second);
}

void Test_AMoveTakesEffectWhenExplorerIsHandedIt() {
    // DECISIONS 58. Asking for a move says where the icon should be; only
    // Explorer taking it says where it is. An application's own messages in between
    // are handled by where the icon really is.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9191, 3, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    const std::wstring key = L"app.exe#3";
    MoveIconToTray(key, Destination::Primary);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK(!g_primaryOnly.empty() && !g_primaryOnly[0].forwardedToShell);
        CHECK(!g_primaryOnly.empty() && g_primaryOnly[0].shellTarget);
    }
    // A modify now is not Explorer's to hear: it does not have the icon yet,
    // and the add it is about to get is built from everything folded so far.
    Feed(MakePayload(NIM_MODIFY, 0x9191, 3, NIF_TIP, L"newer", nullptr), &forward);
    CHECK(!forward);

    CHECK(SettleWithExplorer() == std::vector<DWORD>{NIM_ADD});
    Feed(MakePayload(NIM_MODIFY, 0x9191, 3, NIF_TIP, L"newest", nullptr), &forward);
    CHECK(forward);
}

void Test_AnIconRemovedBeforeItsMoveIsNotAddedBack() {
    // Applications send; moves wait for the taskbar's thread. So an
    // application's NIM_DELETE is handled before a move into the primary tray
    // that was asked for first, and delivering the move afterwards recreated
    // an icon its application had already removed.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9292, 3, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#3", Destination::Primary);

    forward = true;
    Feed(MakePayload(NIM_DELETE, 0x9292, 3, 0, nullptr, nullptr), &forward);
    CHECK(!forward);  // Explorer never had it, so it is not told
    CHECK(SettleWithExplorer().empty());
}

void Test_AMoveUndoneBeforeItIsDeliveredHandsExplorerNothing() {
    // In and straight back out before the taskbar's thread got to either: the
    // icon is where it should be, so Explorer, which never had it, is not
    // asked for anything.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9393, 3, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#3", Destination::Primary);
    MoveIconToTray(L"app.exe#3", Destination::Secondary);
    CHECK(SettleWithExplorer().empty());
    std::lock_guard<std::mutex> lock(g_mutex);
    CHECK(!g_icons.empty() && !g_icons[0].forwardedToShell);
}

// ---------------------------------------------------------------------------
// Findings from the second external review of 2026-09-23 (DECISIONS 66)
// ---------------------------------------------------------------------------

// The icon with this key, under the caller's lock.
MirroredIcon* IconByKeyLocked(std::wstring_view key) {
    for (auto* list : {&g_icons, &g_primaryOnly}) {
        for (auto& icon : *list) {
            if (StableKeyOf(icon) == key) {
                return &icon;
            }
        }
    }
    return nullptr;
}

std::vector<BYTE> WithCallback(std::vector<BYTE> payload, DWORD callback) {
    memcpy(payload.data() + wire::kCallbackMsg, &callback, sizeof(callback));
    return payload;
}

void Test_AnAddCarriesWhatArrivedWhileItWaited() {
    // A move into the primary tray used to be queued as a finished record.
    // What the application sent before the taskbar's thread got to it was
    // swallowed - Explorer did not have the icon yet - and then missing from
    // the add: an old callback, an old version.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(WithCallback(MakePayload(NIM_ADD, 0x9494, 3, NIF_MESSAGE | NIF_TIP, L"app",
                                  L"C:\\a\\app.exe"),
                      0x500),
         &forward);
    Feed(MakeSetVersion(0x9494, 3, NOTIFYICON_VERSION_4), &forward);
    MoveIconToTray(L"app.exe#3", Destination::Primary);

    Feed(WithCallback(MakePayload(NIM_MODIFY, 0x9494, 3, NIF_MESSAGE | NIF_TIP, L"newer",
                                  nullptr),
                      0x501),
         &forward);
    CHECK(!forward);
    Feed(MakeSetVersion(0x9494, 3, NOTIFYICON_VERSION), &forward);
    CHECK(!forward);

    const std::vector<TrayNotification> handed =
        SettleAnswering([](const TrayNotification&) -> LRESULT { return TRUE; });
    CHECK_EQ(static_cast<int>(handed.size()), 2);
    if (handed.size() == 2) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[0].callbackMessage, 0x501u);
        CHECK_WSTR(handed[0].tip, L"newer");
        CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_SETVERSION));
        CHECK_EQ(handed[1].version, static_cast<DWORD>(NOTIFYICON_VERSION));
    }
}

void Test_WhatExplorerIsHandedFollowsTheLastMove() {
    // Two moves of one icon were queued by whoever released the lock first,
    // so an older removal could be delivered after a newer add and take the
    // icon out of Explorer for good, while the mod believed it was there.
    // Whatever order the moves came in, what is settled is the last one.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);

    bool forward = false;
    Feed(MakePayload(NIM_ADD, 0x9595, 3, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    CHECK(forward);
    const std::wstring key = L"app.exe#3";

    MoveIconToTray(key, Destination::Secondary);
    MoveIconToTray(key, Destination::Primary);
    CHECK(SettleWithExplorer().empty());  // where it started: nothing to hand over
    CHECK(ForwardedToShell(key));

    MoveIconToTray(key, Destination::Primary);
    MoveIconToTray(key, Destination::Secondary);
    CHECK(SettleWithExplorer() == std::vector<DWORD>{NIM_DELETE});
    CHECK(!ForwardedToShell(key));
}

void Test_AnAddExplorerRefusesIsAskedAgainThenLeftToItsApplication() {
    // A refused add was recorded as taken: the mod believed Explorer had the
    // icon, nothing asked again, and it was in neither tray.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    const std::vector<BYTE> add = MakePayload(NIM_ADD, 0x9696, 3, NIF_MESSAGE | NIF_TIP,
                                              L"app", L"C:\\a\\app.exe");
    Feed(add, &forward);
    const std::wstring key = L"app.exe#3";
    MoveIconToTray(key, Destination::Primary);

    auto refuse = [](const TrayNotification&) -> LRESULT { return FALSE; };
    for (int attempt = 1; attempt <= kShellAttempts; attempt++) {
        const std::vector<TrayNotification> handed = SettleAnswering(refuse);
        // The add, then whether Explorer has it already.
        CHECK_EQ(static_cast<int>(handed.size()), 2);
        if (handed.size() == 2) {
            CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
            CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_MODIFY));
        }
        std::lock_guard<std::mutex> lock(g_mutex);
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && !icon->forwardedToShell);
        CHECK(icon && NeedsSettlingLocked(*icon) == (attempt < kShellAttempts));
    }
    CHECK(SettleAnswering(refuse).empty());

    // Left to its application: its own messages go to Explorer, and what
    // Explorer answers is recorded. A modify for an icon it does not have is
    // refused...
    bool record = false;
    const std::vector<BYTE> modify =
        MakePayload(NIM_MODIFY, 0x9696, 3, NIF_TIP, L"still here", nullptr);
    Feed(modify, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(modify), false);
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && !icon->forwardedToShell);
    }
    // ...and the add the application answers that with is taken.
    Feed(add, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(add), true);
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && icon->forwardedToShell);
    }
    // An ordinary icon of Explorer's again, whose answers are recorded like
    // every other's (DECISIONS 70).
    Feed(modify, &forward, nullptr, &record);
    CHECK(forward && record);
    std::lock_guard<std::mutex> lock(g_mutex);
    const MirroredIcon* icon = IconByKeyLocked(key);
    CHECK(icon && !OwedToShell(*icon));
}

void Test_AnAddRefusedForAnIconExplorerHasIsRecordedAsThere() {
    // Explorer refuses to add an icon it already has, and updates it when
    // asked to instead. That is not a refusal to keep asking about.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9797, 3, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#3", Destination::Primary);
    const std::vector<TrayNotification> handed =
        SettleAnswering([](const TrayNotification& n) -> LRESULT {
            return n.message == NIM_MODIFY ? TRUE : FALSE;
        });
    CHECK_EQ(static_cast<int>(handed.size()), 2);
    CHECK(SettleWithExplorer().empty());
    std::lock_guard<std::mutex> lock(g_mutex);
    const MirroredIcon* icon = IconByKeyLocked(L"app.exe#3");
    CHECK(icon && icon->forwardedToShell);
}

void Test_AnIconRemovedWhileExplorerTookItBackIsTakenOutAgain() {
    // Explorer may send messages of its own while it handles a record, and
    // they come back through the subclass. An application's removal handled
    // then found an icon Explorer did not have yet, and was not passed on.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9898, 3, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#3", Destination::Primary);

    bool removedMeanwhile = false;
    const std::vector<TrayNotification> handed =
        SettleAnswering([&](const TrayNotification& n) -> LRESULT {
            if (n.message == NIM_ADD) {
                bool passedOn = true;
                Feed(MakePayload(NIM_DELETE, 0x9898, 3, 0, nullptr, nullptr), &passedOn);
                removedMeanwhile = !passedOn;
            }
            return TRUE;
        });
    CHECK(removedMeanwhile);
    CHECK_EQ(static_cast<int>(handed.size()), 2);
    if (handed.size() == 2) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_DELETE));
    }
}

// ---------------------------------------------------------------------------
// Findings from the third external review of 2026-09-24 (DECISIONS 68-72)
// ---------------------------------------------------------------------------

LRESULT Accept(const TrayNotification&) {
    return TRUE;
}

// A window whose application is still there, as the hand-back asks.
HWND MakeOwnerWindow() {
    return CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                           nullptr, nullptr);
}

DWORD WireHandle(HWND wnd) {
    return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(wnd));
}

void Test_EveryIconIsHandedBackAsItIsWhenTheModUnloads() {
    // DECISIONS 68. The hand-back reads the store as it is when it runs, and
    // what arrives while Explorer takes an icon back - Explorer may send
    // messages of its own meanwhile, and they come back through the subclass -
    // is handed back by another round: a change to the icon on its way, and a
    // new icon, swallowed into a tray that is going.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    HWND owner = MakeOwnerWindow();
    CHECK(owner != nullptr);
    const DWORD wnd = WireHandle(owner);

    bool forward = true;
    Feed(WithCallback(MakePayload(NIM_ADD, wnd, 1, NIF_MESSAGE | NIF_TIP, L"first",
                                  L"C:\\a\\app.exe"),
                      0x500),
         &forward);
    // One whose application has gone is left as it is.
    Feed(MakePayload(NIM_ADD, 0x9A9A, 4, NIF_MESSAGE | NIF_TIP, L"gone",
                     L"C:\\a\\app.exe"),
         &forward);

    g_unloading.store(true);
    std::vector<TrayNotification> handed;
    HandIconsBackToShell([&](HWND, const std::vector<BYTE>& record) -> LRESULT {
        handed.push_back(Parsed(record));
        if (handed.size() == 1) {
            bool passedOn = true;
            Feed(WithCallback(MakePayload(NIM_MODIFY, wnd, 1, NIF_MESSAGE, nullptr, nullptr),
                              0x501),
                 &passedOn);
            CHECK(!passedOn);
            Feed(MakePayload(NIM_ADD, wnd, 2, NIF_MESSAGE | NIF_TIP, L"second",
                             L"C:\\a\\app.exe"),
                 &passedOn);
            CHECK(!passedOn);
        }
        return TRUE;
    });
    CHECK(g_handedBack.load());
    // The first icon's add; then its newer callback, and the second icon.
    CHECK_EQ(static_cast<int>(handed.size()), 3);
    if (handed.size() == 3) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[0].uID, 1u);
        CHECK_EQ(handed[0].callbackMessage, 0x500u);
        CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_MODIFY));
        CHECK_EQ(handed[1].uID, 1u);
        CHECK_EQ(handed[1].callbackMessage, 0x501u);
        CHECK_EQ(handed[2].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[2].uID, 2u);
    }
    g_unloading.store(false);
    g_handedBack.store(false);
    DestroyWindow(owner);
}

void Test_TheHandBackLeavesAloneAnIconExplorerRefusedItsApplication() {
    // DECISIONS 68, 70. Found live: when the mod is loaded into a running
    // Explorer, Explorer's own icons - its volume icon among them - register
    // again, and Explorer refuses both their add and the modify that asks
    // about it. Recorded as not Explorer's and left to their application,
    // they were then handed back as the mod unloaded, three times each, and
    // refused each time. The mod never took them away; it has nothing to give
    // back.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);
    HWND owner = MakeOwnerWindow();
    const DWORD wnd = WireHandle(owner);

    const std::vector<BYTE> add = MakePayload(NIM_ADD, wnd, 100, NIF_MESSAGE | NIF_TIP,
                                              L"Speakers: 100%", L"C:\\Windows\\explorer.exe");
    bool forward = false;
    bool record = false;
    Feed(add, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(add), false);
    }
    // Refused the modify that asks, too.
    CHECK_EQ(static_cast<int>(
                 SettleAnswering([](const TrayNotification&) -> LRESULT { return FALSE; })
                     .size()),
             1);
    // One the mod did take away is still handed back.
    Feed(MakePayload(NIM_ADD, wnd, 7, NIF_MESSAGE | NIF_TIP, L"moved",
                     L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#7", Destination::Secondary);
    CHECK(SettleWithExplorer() == std::vector<DWORD>{NIM_DELETE});

    g_unloading.store(true);
    std::vector<TrayNotification> handed;
    HandIconsBackToShell([&](HWND, const std::vector<BYTE>& record) -> LRESULT {
        handed.push_back(Parsed(record));
        return TRUE;
    });
    CHECK_EQ(static_cast<int>(handed.size()), 1);
    if (!handed.empty()) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[0].uID, 7u);
    }
    g_unloading.store(false);
    g_handedBack.store(false);
    DestroyWindow(owner);
}

void Test_AHandBackThatArrivesDuringARoundIsDoneAfterIt() {
    // DECISIONS 68, 71. A wake-up dispatched while a round of settling is
    // under way does not start another inside it. The hand-back as the mod
    // unloads is one, and it is done once that round is over, not dropped.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    HWND owner = MakeOwnerWindow();
    const DWORD wnd = WireHandle(owner);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, wnd, 1, NIF_MESSAGE | NIF_TIP, L"moved", L"C:\\a\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, wnd, 2, NIF_MESSAGE | NIF_TIP, L"stays", L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#1", Destination::Primary);

    std::vector<TrayNotification> handed;
    int nested = 0;
    OnShellTrayWake([&](HWND, const std::vector<BYTE>& record) -> LRESULT {
        handed.push_back(Parsed(record));
        if (handed.size() == 1) {
            // The mod begins to unload, and its hand-back is dispatched from
            // inside this round.
            g_unloading.store(true);
            OnShellTrayWake([&](HWND, const std::vector<BYTE>&) -> LRESULT {
                nested++;
                return TRUE;
            });
            CHECK(!g_handedBack.load());
        }
        return TRUE;
    });
    CHECK_EQ(nested, 0);
    CHECK(g_handedBack.load());
    // The move from the round, then the other icon from the hand-back.
    CHECK_EQ(static_cast<int>(handed.size()), 2);
    if (handed.size() == 2) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[0].uID, 1u);
        CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[1].uID, 2u);
    }
    g_unloading.store(false);
    g_handedBack.store(false);
    DestroyWindow(owner);
}

void Test_ASettlingRoundIsNotStartedInsideAnother() {
    // DECISIONS 71. Explorer may run a message loop while it handles a
    // record, and a wake-up posted meanwhile is dispatched from inside it. A
    // second round started there handed Explorer the same add again.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9F9F, 3, NIF_MESSAGE | NIF_TIP, L"app", L"C:\\a\\app.exe"),
         &forward);
    MoveIconToTray(L"app.exe#3", Destination::Primary);

    int nested = -1;
    const std::vector<TrayNotification> handed =
        SettleAnswering([&](const TrayNotification& n) -> LRESULT {
            if (n.message == NIM_ADD && nested < 0) {
                nested = static_cast<int>(SettleAnswering(Accept).size());
            }
            return TRUE;
        });
    CHECK_EQ(nested, 0);
    CHECK_EQ(static_cast<int>(handed.size()), 1);
}

void Test_WhatArrivesWhileExplorerTakesAnIconBackFollowsIt() {
    // DECISIONS 71. An update handled while Explorer was taking an icon back
    // was swallowed - Explorer did not have the icon yet - and the add it went
    // on to take was the older one: the icon stayed in Explorer without its
    // newer callback.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(WithCallback(MakePayload(NIM_ADD, 0x9E9E, 3, NIF_MESSAGE | NIF_TIP, L"app",
                                  L"C:\\a\\app.exe"),
                      0x500),
         &forward);
    Feed(MakeSetVersion(0x9E9E, 3, NOTIFYICON_VERSION_4), &forward);
    const std::wstring key = L"app.exe#3";
    MoveIconToTray(key, Destination::Primary);

    bool swallowed = false;
    SettleAnswering([&](const TrayNotification& n) -> LRESULT {
        if (n.message == NIM_ADD) {
            bool passedOn = true;
            Feed(WithCallback(MakePayload(NIM_MODIFY, 0x9E9E, 3, NIF_MESSAGE, nullptr,
                                          nullptr),
                              0x501),
                 &passedOn);
            swallowed = !passedOn;
        }
        return TRUE;
    });
    CHECK(swallowed);

    // The whole record again, as an update, and its version after it.
    const std::vector<TrayNotification> handed = SettleAnswering(Accept);
    CHECK_EQ(static_cast<int>(handed.size()), 2);
    if (handed.size() == 2) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_MODIFY));
        CHECK_EQ(handed[0].callbackMessage, 0x501u);
        CHECK_WSTR(handed[0].tip, L"app");
        CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_SETVERSION));
        CHECK_EQ(handed[1].version, static_cast<DWORD>(NOTIFYICON_VERSION_4));
    }
    CHECK(SettleWithExplorer().empty());
    CHECK(ForwardedToShell(key));
}

void Test_AVersionExplorerDoesNotTakeIsAskedForAgain() {
    // DECISIONS 71. What Explorer answered the version that follows an add
    // was not looked at: an icon whose version it refused was recorded as
    // settled, and its callbacks came in the shape of version 0.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x9D9D, 3, NIF_MESSAGE | NIF_TIP, L"app", L"C:\\a\\app.exe"),
         &forward);
    Feed(MakeSetVersion(0x9D9D, 3, NOTIFYICON_VERSION_4), &forward);
    const std::wstring key = L"app.exe#3";
    MoveIconToTray(key, Destination::Primary);

    auto refuseVersion = [](const TrayNotification& n) -> LRESULT {
        return n.message == NIM_SETVERSION ? FALSE : TRUE;
    };
    for (int attempt = 1; attempt <= kShellAttempts; attempt++) {
        const std::vector<TrayNotification> handed = SettleAnswering(refuseVersion);
        // The add - then, with Explorer holding the icon, the whole record
        // again as an update - and the version after it.
        CHECK_EQ(static_cast<int>(handed.size()), 2);
        if (handed.size() == 2) {
            CHECK_EQ(handed[0].message,
                     static_cast<DWORD>(attempt == 1 ? NIM_ADD : NIM_MODIFY));
            CHECK_EQ(handed[1].message, static_cast<DWORD>(NIM_SETVERSION));
        }
        std::lock_guard<std::mutex> lock(g_mutex);
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && icon->forwardedToShell);
        CHECK(icon && NeedsSettlingLocked(*icon) == (attempt < kShellAttempts));
    }
    // Asked kShellAttempts times, then left at the version Explorer gives it.
    CHECK(SettleAnswering(refuseVersion).empty());
}

void Test_ExplorersAnswerToAnApplicationsOwnMessageIsRecorded() {
    // DECISIONS 70. Only an icon Explorer had refused to take back from the
    // mod had Explorer's answers recorded. An application's own add that
    // Explorer refused was recorded as there, and nothing put that right.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);

    const std::vector<BYTE> add = MakePayload(NIM_ADD, 0x9B9B, 3, NIF_MESSAGE | NIF_TIP,
                                              L"app", L"C:\\a\\app.exe");
    const std::vector<BYTE> modify =
        MakePayload(NIM_MODIFY, 0x9B9B, 3, NIF_TIP, L"newer", nullptr);
    const std::wstring key = L"app.exe#3";
    bool forward = false;
    bool record = false;
    Feed(add, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        // Refused: Explorer has it already, or will not take it. Which is
        // asked on the taskbar's next round, not in the middle of the
        // application's message (DECISIONS 51).
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK(RecordShellAnswerLocked(Parsed(add), false));
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && NeedsSettlingLocked(*icon));
    }
    const std::vector<TrayNotification> asked =
        SettleAnswering([](const TrayNotification&) -> LRESULT { return FALSE; });
    CHECK_EQ(static_cast<int>(asked.size()), 1);
    if (!asked.empty()) {
        CHECK_EQ(asked[0].message, static_cast<DWORD>(NIM_MODIFY));
    }
    {
        // Not there either: left to its application, which was told, rather
        // than handed to Explorer by the mod.
        std::lock_guard<std::mutex> lock(g_mutex);
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && !icon->forwardedToShell);
        CHECK(icon && OwedToShell(*icon));
        CHECK(icon && !NeedsSettlingLocked(*icon));
    }
    Feed(modify, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(modify), true);
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && icon->forwardedToShell);
    }
    // A modify Explorer refuses says it does not have the icon.
    Feed(modify, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK(!RecordShellAnswerLocked(Parsed(modify), false));
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && !icon->forwardedToShell);
        CHECK(icon && OwedToShell(*icon));
    }
    // An add refused because Explorer has the icon already - every
    // application's, when the mod is loaded into a running Explorer - is
    // Explorer's once the modify that asks is taken.
    Feed(add, &forward, nullptr, &record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(add), true);
    }
    Feed(add, &forward, nullptr, &record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK(RecordShellAnswerLocked(Parsed(add), false));
    }
    CHECK_EQ(static_cast<int>(SettleAnswering(Accept).size()), 1);
    std::lock_guard<std::mutex> lock(g_mutex);
    const MirroredIcon* icon = IconByKeyLocked(key);
    CHECK(icon && icon->forwardedToShell);
    CHECK(icon && !NeedsSettlingLocked(*icon));
}

void Test_ARefusedAddIsAskedAboutWithAModifyOfTheSameIcon() {
    // DECISIONS 70. Explorer refuses to add an icon it has already, so a
    // refused add is followed by a modify that says which it was. A balloon
    // the add carried is not shown again by it, and it carries no picture:
    // it is asked later, when the application may have destroyed the one
    // the record names.
    const std::vector<BYTE> add =
        MakePayload(NIM_ADD, 0x10, 3, NIF_MESSAGE | NIF_TIP | NIF_ICON | NIF_INFO, L"app",
                    L"C:\\a\\app.exe");
    const TrayNotification n = Parsed(ProbeRecordFor(add));
    CHECK_EQ(n.message, static_cast<DWORD>(NIM_MODIFY));
    CHECK_EQ(n.flags, static_cast<UINT>(NIF_MESSAGE | NIF_TIP));
    CHECK_EQ(n.uID, 3u);
    CHECK_WSTR(n.tip, L"app");
}

void Test_AnAddNeverCarriesAPictureTheModDoesNotOwn() {
    // DECISIONS 72. An add is drawn with a copy of the mod's own picture, made
    // as it is handed over. When there was none to copy, it went with the
    // handle the application last sent - destroyed by then, or by then
    // another icon's picture.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    HICON appIcon = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    bool forward = true;
    Feed(WithIcon(MakePayload(NIM_ADD, 0x9C9C, 3, NIF_MESSAGE | NIF_TIP | NIF_ICON, L"app",
                              L"C:\\a\\app.exe"),
                  appIcon),
         &forward);
    DestroyIcon(appIcon);  // as applications do once the shell has answered
    const std::wstring key = L"app.exe#3";
    {
        // The mod's own copy is gone as well, so copying it fails.
        std::lock_guard<std::mutex> lock(g_mutex);
        MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && icon->icon);
        if (icon && icon->icon) {
            DestroyIcon(icon->icon);
        }
    }
    MoveIconToTray(key, Destination::Primary);
    const std::vector<TrayNotification> handed = SettleAnswering(Accept);
    CHECK(!handed.empty());
    if (!handed.empty()) {
        CHECK_EQ(handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK_EQ(handed[0].flags & NIF_ICON, 0u);
        CHECK(handed[0].icon == nullptr);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (MirroredIcon* icon = IconByKeyLocked(key)) {
        icon->icon = nullptr;  // destroyed above
    }
}

void Test_APictureThatCannotBeCopiedLeavesTheOneBefore() {
    // DECISIONS 72. The store let go of the picture it had before it knew
    // whether it could copy the new one, so a copy that failed left the icon
    // with no picture at all.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    HICON first = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    HICON gone = CopyIcon(LoadIconW(nullptr, IDI_WARNING));
    DestroyIcon(gone);
    bool forward = true;
    Feed(WithIcon(MakePayload(NIM_ADD, 0x9898, 3, NIF_MESSAGE | NIF_TIP | NIF_ICON, L"app",
                              L"C:\\a\\app.exe"),
                  first),
         &forward);
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x9898, 3, NIF_ICON, nullptr, nullptr), gone),
         &forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const MirroredIcon* icon = IconByKeyLocked(L"app.exe#3");
        CHECK(icon && IconIsAlive(icon->icon));
    }
    // A picture taken away on purpose is taken away.
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x9898, 3, NIF_ICON, nullptr, nullptr), nullptr),
         &forward);
    std::lock_guard<std::mutex> lock(g_mutex);
    const MirroredIcon* icon = IconByKeyLocked(L"app.exe#3");
    CHECK(icon && icon->icon == nullptr);
    DestroyIcon(first);
}

void Test_TheModAttachesOnlyOnceItsTrayThreadRuns() {
    // DECISIONS 69. Wh_ModInit took the end of its wait for the tray thread as
    // leave to attach. A thread that then failed left a subclass swallowing
    // icons into trays nothing drew.
    const TrayThreadState was = g_trayThreadState.load();
    for (TrayThreadState state : {TrayThreadState::Starting, TrayThreadState::GaveUp}) {
        g_trayThreadState.store(state);
        const int attempts = WindhawkUtils::SubclassCallCount();
        CHECK(!SubclassShellTrayWindow());
        CHECK_EQ(WindhawkUtils::SubclassCallCount(), attempts);
    }
    g_trayThreadState.store(was);
}

void Test_TrayCallbacksAreWhatExplorerSends() {
    // DECISIONS 62: from version 3, a left button-up is followed by NIN_SELECT
    // and a right one by WM_CONTEXTMENU; version 4 packs the point and the uID.
    const POINT at = {-1700, 1180};
    auto messages = [](const std::vector<TrayCallback>& callbacks, UINT version) {
        std::vector<UINT> out;
        for (const auto& c : callbacks) {
            out.push_back(version >= NOTIFYICON_VERSION_4
                              ? LOWORD(c.lParam)
                              : static_cast<UINT>(c.lParam));
        }
        return out;
    };

    auto v0 = TrayCallbacksFor(0, 7, WM_LBUTTONUP, at);
    CHECK(messages(v0, 0) == std::vector<UINT>{WM_LBUTTONUP});
    CHECK_EQ(v0[0].wParam, static_cast<WPARAM>(7));
    CHECK(messages(TrayCallbacksFor(0, 7, WM_RBUTTONUP, at), 0) ==
          std::vector<UINT>{WM_RBUTTONUP});

    auto v3 = TrayCallbacksFor(NOTIFYICON_VERSION, 7, WM_LBUTTONUP, at);
    CHECK(messages(v3, 3) == (std::vector<UINT>{WM_LBUTTONUP, NIN_SELECT}));
    CHECK_EQ(v3[1].wParam, static_cast<WPARAM>(7));
    CHECK(messages(TrayCallbacksFor(NOTIFYICON_VERSION, 7, WM_RBUTTONUP, at), 3) ==
          (std::vector<UINT>{WM_RBUTTONUP, WM_CONTEXTMENU}));

    auto v4 = TrayCallbacksFor(NOTIFYICON_VERSION_4, 7, WM_LBUTTONUP, at);
    CHECK(messages(v4, 4) == (std::vector<UINT>{WM_LBUTTONUP, NIN_SELECT}));
    CHECK_EQ(HIWORD(v4[1].lParam), 7u);
    CHECK_EQ(static_cast<short>(LOWORD(v4[1].wParam)), -1700);
    CHECK_EQ(static_cast<short>(HIWORD(v4[1].wParam)), 1180);
    CHECK(messages(TrayCallbacksFor(NOTIFYICON_VERSION_4, 7, WM_RBUTTONUP, at), 4) ==
          (std::vector<UINT>{WM_RBUTTONUP, WM_CONTEXTMENU}));

    // A button going down, or a double-click, is just itself.
    CHECK(messages(TrayCallbacksFor(NOTIFYICON_VERSION_4, 7, WM_LBUTTONDOWN, at), 4) ==
          std::vector<UINT>{WM_LBUTTONDOWN});
    CHECK(messages(TrayCallbacksFor(NOTIFYICON_VERSION_4, 7, WM_LBUTTONDBLCLK, at), 4) ==
          std::vector<UINT>{WM_LBUTTONDBLCLK});
}

void Test_OnlyAKnownTaskbarHostLayoutIsUsed() {
    // DECISIONS 61. The offset is dereferenced and called through, so an
    // unknown layout is "no", never a guess.
    const BYTE known[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC1, 0x48};
    size_t offset = 0;
    CHECK(ElementOffsetFromFrameHeight(known, &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x48));

    BYTE otherRegister[sizeof(known)];
    memcpy(otherRegister, known, sizeof(known));
    otherRegister[6] = 0xC2;  // add rdx, not rcx
    offset = 0x77;
    CHECK(!ElementOffsetFromFrameHeight(otherRegister, &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x77));  // left alone

    BYTE negative[sizeof(known)];
    memcpy(negative, known, sizeof(known));
    negative[7] = 0x90;  // a negative displacement
    CHECK(!ElementOffsetFromFrameHeight(negative, &offset));

    CHECK(!ElementOffsetFromFrameHeight(nullptr, &offset));
}

void Test_SystemTraysSymbolsAreResolvedOnlyWhenItIsLoadedToRun() {
    // From Windhawk's catalog review: SystemTray.dll is resolved as Explorer
    // loads it, by a hook on every LoadLibraryExW until then. Only its own
    // load, and only one that will run it, is the one to resolve.
    HMODULE systemTray = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x7FF810000000));
    HMODULE other = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x7FF820000000));
    CHECK(IsSystemTrayLoad(systemTray, 0, systemTray));
    CHECK(IsSystemTrayLoad(systemTray, LOAD_LIBRARY_SEARCH_SYSTEM32, systemTray));
    CHECK(!IsSystemTrayLoad(other, 0, systemTray));
    CHECK(!IsSystemTrayLoad(systemTray, LOAD_LIBRARY_AS_DATAFILE, systemTray));
    CHECK(!IsSystemTrayLoad(systemTray, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE, systemTray));
    CHECK(!IsSystemTrayLoad(systemTray, LOAD_LIBRARY_AS_IMAGE_RESOURCE, systemTray));
    // A load that failed, before SystemTray.dll is there, is not it.
    CHECK(!IsSystemTrayLoad(nullptr, 0, nullptr));
    CHECK(!IsSystemTrayLoad(other, 0, nullptr));
}

// The test displays, noting each time they are asked for while g_mutex is held:
// a thread of its own cannot take it then.
std::atomic<int> g_displaysAsked{0};
std::atomic<int> g_displaysAskedUnderLock{0};

std::vector<MonitorInfoEntry> TestMonitorsNotingTheLock() {
    g_displaysAsked++;
    bool held = false;
    std::thread probe([&held] {
        if (g_mutex.try_lock()) {
            g_mutex.unlock();
        } else {
            held = true;
        }
    });
    probe.join();
    if (held) {
        g_displaysAskedUnderLock++;
    }
    return TestMonitors();
}

void Test_TheDisplaysAreAskedForWithoutHoldingTheLock() {
    // From Windhawk's catalog review. Every two seconds the tray thread asked
    // Windows for the displays - a call or two per display - twice, holding
    // the lock that every application's tray message waits on, on the
    // taskbar's thread; so did moving an icon and a change of settings.
    ForgetPlacements();
    ResetStore(DefaultSettings(), true);
    g_enumerateMonitors = TestMonitorsNotingTheLock;
    g_displaysAsked = 0;
    g_displaysAskedUnderLock = 0;

    ReplayRoutingChanges();
    ApplySettingsToTrackedIcons();
    MoveIconToTray(L"moved.exe#1", Destination::Secondary);
    SyncFloatingTrays();
    DestroyFloatingTrays();

    CHECK(g_displaysAsked.load() >= 4);
    CHECK_EQ(g_displaysAskedUnderLock.load(), 0);
    g_enumerateMonitors = TestMonitors;
    ForgetPlacements();
}

void Test_ATrayThatCannotBeEmbeddedIsLookedForLessOften() {
    // From Windhawk's catalog review. While a display's tray waited to go into
    // its taskbar, the taskbar's thread was asked to walk its elements every
    // two seconds; a tray row a Windows update had changed was looked for that
    // way for as long as Explorer ran. Every tick for the first ten seconds,
    // then every thirty.
    std::vector<int> due;
    for (int tick = 0; tick <= 46; tick++) {
        if (AttachDueAfter(tick)) {
            due.push_back(tick);
        }
    }
    CHECK(due == (std::vector<int>{0, 1, 2, 3, 4, 15, 30, 45}));
}

void Test_TheArrangeWindowIsSizedForItsDisplay() {
    // From Windhawk's catalog review: the arrange window was laid out in
    // fixed pixels, so it came out small on a display at 150%.
    const ArrangeMetrics at96 = ArrangeMetricsFor(96);
    CHECK_EQ(at96.margin, 12);
    CHECK_EQ(at96.listWidth, 250);
    CHECK_EQ(at96.columnWidth, 226);
    CHECK_EQ(at96.height, 460);
    CHECK_EQ(at96.labelHeight, 20);
    CHECK_EQ(at96.hintHeight, 52);
    CHECK_EQ(at96.icon, 16);
    const ArrangeMetrics at144 = ArrangeMetricsFor(144);
    CHECK_EQ(at144.margin, 18);
    CHECK_EQ(at144.listWidth, 375);
    CHECK_EQ(at144.columnWidth, 339);
    CHECK_EQ(at144.height, 690);
    CHECK_EQ(at144.labelHeight, 30);
    CHECK_EQ(at144.hintHeight, 78);
    CHECK_EQ(at144.icon, 24);
}

// Instructions as they sit in memory, little-endian.
std::vector<BYTE> Arm64Code(std::initializer_list<DWORD> instructions) {
    std::vector<BYTE> code(instructions.size() * sizeof(DWORD));
    size_t at = 0;
    for (DWORD instruction : instructions) {
        memcpy(&code[at], &instruction, sizeof(instruction));
        at += sizeof(instruction);
    }
    return code;
}

void Test_OnArm64TheTaskbarHostLayoutIsReadFromItsOwnInstructions() {
    // From Windhawk's catalog review. On an ARM64 PC the mod is built for
    // ARM64, where FrameHeight begins with ARM64 instructions: only the x64
    // ones were known, so no display's tray was ever put in its taskbar. The
    // pattern is the one taskbar-multirow reads:
    //   pacibsp / stp fp, lr, [sp, #-0x10]! / mov fp, sp / ldr x8, [x0, #nn]!
    constexpr DWORD kPacibsp = 0xD503237F;
    constexpr DWORD kSaveFrame = 0xA9BF7BFD;
    constexpr DWORD kSetFrame = 0x910003FD;
    const auto known = Arm64Code({kPacibsp, kSaveFrame, kSetFrame, 0xF8410C08});
    size_t offset = 0;
    CHECK(ElementOffsetFromFrameHeightArm64(known.data(), &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x10));

    // Another offset, and another frame size, are the same layout.
    offset = 0;
    CHECK(ElementOffsetFromFrameHeightArm64(
        Arm64Code({kPacibsp, kSaveFrame, kSetFrame, 0xF8448C08}).data(), &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x48));
    offset = 0;
    CHECK(ElementOffsetFromFrameHeightArm64(
        Arm64Code({kPacibsp, 0xA9BE7BFD, kSetFrame, 0xF8410C08}).data(), &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x10));

    // Anything else is "no", and the offset is left alone.
    const std::vector<std::vector<BYTE>> others = {
        // the load is from x1, not the object
        Arm64Code({kPacibsp, kSaveFrame, kSetFrame, 0xF8410C28}),
        // after the load rather than before it
        Arm64Code({kPacibsp, kSaveFrame, kSetFrame, 0xF8410408}),
        // a negative offset
        Arm64Code({kPacibsp, kSaveFrame, kSetFrame, 0xF85F0C08}),
        // a 32-bit load
        Arm64Code({kPacibsp, kSaveFrame, kSetFrame, 0xB8410C08}),
        // no pointer authentication
        Arm64Code({0xD503201F, kSaveFrame, kSetFrame, 0xF8410C08}),
        // another register pair saved
        Arm64Code({kPacibsp, 0xA9BF53F3, kSetFrame, 0xF8410C08}),
        // the frame pointer not set
        Arm64Code({kPacibsp, kSaveFrame, 0xD503201F, 0xF8410C08}),
    };
    for (const auto& other : others) {
        offset = 0x77;
        CHECK(!ElementOffsetFromFrameHeightArm64(other.data(), &offset));
        CHECK_EQ(offset, static_cast<size_t>(0x77));
    }
    CHECK(!ElementOffsetFromFrameHeightArm64(nullptr, &offset));
    CHECK(!ElementOffsetFromFrameHeightArm64(known.data(), nullptr));

    // Each architecture's decoder knows only its own instructions.
    const BYTE x64[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC1, 0x48,
                        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    CHECK(!ElementOffsetFromFrameHeightArm64(x64, &offset));
    CHECK(!ElementOffsetFromFrameHeightX64(known.data(), &offset));
    CHECK(ElementOffsetFromFrameHeightX64(x64, &offset));

    // The mod reads with the decoder for the architecture it was built for.
    offset = 0;
#if defined(_M_ARM64) || defined(__aarch64__)
    CHECK(ElementOffsetFromFrameHeight(known.data(), &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x10));
#else
    CHECK(!ElementOffsetFromFrameHeight(known.data(), &offset));
    CHECK(ElementOffsetFromFrameHeight(x64, &offset));
    CHECK_EQ(offset, static_cast<size_t>(0x48));
#endif
}

// From an external review. The way into the taskbar reads Explorer's private
// objects at offsets nothing promises. After a Windows update one of those reads
// could land on memory that is not there, and a fault inside Explorer takes the
// shell down. C++'s catch does not stop a fault, and this compiler has no SEH,
// so each such read is made in a way that fails rather than faults, and a
// failure means the tray floats instead.
struct GuardedPages {
    BYTE* readable = nullptr;  // one page, readable
    BYTE* forbidden = nullptr; // the page after it, no access

    GuardedPages() {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        page = info.dwPageSize;
        readable = static_cast<BYTE*>(VirtualAlloc(
            nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        forbidden = readable + page;
        DWORD old = 0;
        VirtualProtect(forbidden, page, PAGE_NOACCESS, &old);
    }
    ~GuardedPages() { VirtualFree(readable, 0, MEM_RELEASE); }
    size_t page = 0;
};

void Test_PrivateMemoryIsReadWithoutFaulting() {
    int target = 0;
    void* pointer = &target;
    void* value = nullptr;
    CHECK(TryReadPointer(&pointer, &value));
    CHECK(value == &target);

    GuardedPages pages;
    value = &target;
    CHECK(!TryReadPointer(pages.forbidden, &value));
    // A pointer straddling into the forbidden page is not read either.
    CHECK(!TryReadPointer(pages.forbidden - 4, &value));
    CHECK(!TryReadPointer(reinterpret_cast<void*>(0x10), &value));
    CHECK(!TryReadPointer(nullptr, &value));
}

void Test_ASubObjectIsFoundOnlyWhereItIs() {
    void* wanted = reinterpret_cast<void*>(0x12345678);
    void* object[6] = {nullptr, nullptr, nullptr, wanted, nullptr, nullptr};
    CHECK(TaskListWndSiteOf(object, wanted) == &object[3]);

    // Not within the twenty slots it looks at.
    void* distant[24] = {};
    distant[21] = wanted;
    CHECK(TaskListWndSiteOf(distant, wanted) == nullptr);

    // An object whose layout changed, ending where readable memory ends: the
    // walk stops at the first slot it cannot read, rather than faulting.
    GuardedPages pages;
    auto* nearTheEnd = reinterpret_cast<void**>(pages.forbidden) - 2;
    nearTheEnd[0] = nullptr;
    nearTheEnd[1] = nullptr;
    CHECK(TaskListWndSiteOf(nearTheEnd, wanted) == nullptr);

    CHECK(TaskListWndSiteOf(nullptr, wanted) == nullptr);
    CHECK(TaskListWndSiteOf(object, nullptr) == nullptr);
}

void Test_OnlyALiveComObjectIsCalledThrough() {
    IStream* stream = nullptr;
    CHECK(SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)));
    CHECK(LooksLikeComObject(stream));
    stream->Release();

    // A null where the object should be, or one with no vtable.
    CHECK(!LooksLikeComObject(nullptr));
    void* noVtable[4] = {};
    CHECK(!LooksLikeComObject(noVtable));

    // A "vtable" whose QueryInterface points at data, not code.
    void* dataTable[3] = {noVtable, noVtable, noVtable};
    void* dataObject[1] = {dataTable};
    CHECK(!LooksLikeComObject(dataObject));

    // An object in memory that is not there.
    GuardedPages pages;
    CHECK(!LooksLikeComObject(pages.forbidden));
}

// ---------------------------------------------------------------------------
// Loading into an Explorer that already has the icons
//
// Found live, switching the mod off and on in Windhawk: installing, updating
// and switching it on all load it into a running Explorer.
// ---------------------------------------------------------------------------

int g_lookUps = 0;

std::wstring SystemInformerOwnsA1A1(HWND window) {
    g_lookUps++;
    return window == reinterpret_cast<HWND>(static_cast<ULONG_PTR>(0xA1A1))
               ? L"C:\\Tools\\SystemInformer.exe"
               : L"";
}

void Test_AnIconThatOnlyUpdatesIsPlacedByItsProgram() {
    // DECISIONS 63. SystemInformer's icons only ever send modifies while
    // Explorer has them, and a modify carries no path, so the rule sending
    // SystemInformer to tray 2 could not match them after a reload.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"SystemInformer.exe", Destination::Secondary}};
    ResetStore(s, true);
    g_lookUpProcessPath = SystemInformerOwnsA1A1;
    g_lookUps = 0;

    bool forward = true;
    bool retract = false;
    Feed(MakePayload(NIM_MODIFY, 0xA1A1, 3, NIF_ICON | NIF_TIP, L"CPU", nullptr),
         &forward, &retract);
    CHECK(!forward);
    CHECK(retract);  // Explorer has it, and must let it go
    CHECK_EQ(g_lookUps, 1);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
        if (!g_icons.empty()) {
            CHECK(g_icons[0].destinationDecided);
            CHECK_EQ(g_icons[0].shownTray, 2);
            CHECK_WSTR(std::wstring(FileNameOf(g_icons[0].exePath)),
                       L"SystemInformer.exe");
        }
    }

    // Placed now, so the next of its four-a-second updates asks nothing.
    Feed(MakePayload(NIM_MODIFY, 0xA1A1, 3, NIF_TIP, L"CPU 12%", nullptr), &forward,
         &retract);
    CHECK_EQ(g_lookUps, 1);
    CHECK(!retract);  // known now: Explorer was told already

    // A message that carries its path is taken at its word.
    Feed(MakePayload(NIM_ADD, 0xB2B2, 1, NIF_MESSAGE | NIF_TIP, L"app",
                     L"C:\\a\\app.exe"),
         &forward);
    CHECK_EQ(g_lookUps, 1);
}

void Test_OnlyAnIconNewToTheModIsTakenBackFromExplorer() {
    // DECISIONS 64. Loaded into a running Explorer, the mod cannot know which
    // icons Explorer holds, so a new icon going to one of its trays is taken
    // back. One it already tracks it knows about, and one Explorer is to keep
    // stays.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    s.rules = {{L"stremio.exe", Destination::Secondary}};
    ResetStore(s, true);

    bool forward = true;
    bool retract = false;
    Feed(MakePayload(NIM_ADD, 0xC3C3, 1, NIF_MESSAGE | NIF_TIP, L"away",
                     L"C:\\apps\\stremio.exe"),
         &forward, &retract);
    CHECK(!forward);
    CHECK(retract);

    Feed(MakePayload(NIM_MODIFY, 0xC3C3, 1, NIF_TIP, L"away 2", nullptr), &forward,
         &retract);
    CHECK(!retract);

    Feed(MakePayload(NIM_ADD, 0xD4D4, 1, NIF_MESSAGE | NIF_TIP, L"kept",
                     L"C:\\apps\\notepad.exe"),
         &forward, &retract);
    CHECK(forward);
    CHECK(!retract);
}

void Test_TheTreeWalkAnchorsOnAnIconBeforeTheFrame() {
    // Found live: after a reload the tray went into the Grid beside the clock.
    // The frame came first in the tree and sits above the row the tray goes
    // into, so walking up from it could not find the row.
    CHECK(AnchorPreference(L"SystemTray.IconView", L"SystemTrayIcon") <
          AnchorPreference(L"SystemTray.IconView", L""));
    CHECK(AnchorPreference(L"SystemTray.IconView", L"") <
          AnchorPreference(L"SystemTray.Stack", L"MainStack"));
    CHECK(AnchorPreference(L"SystemTray.Stack", L"MainStack") <
          AnchorPreference(L"SystemTray.SystemTrayFrame", L""));
    CHECK(AnchorPreference(L"SystemTray.OmniButton", L"NotificationCenterButton") <
          AnchorPreference(L"SystemTray.SystemTrayFrame", L""));
}

// ---------------------------------------------------------------------------
// Findings from the code audit of 2026-09-24 (DECISIONS 73 to 77)
// ---------------------------------------------------------------------------

// A taskbar thread of the tests' own: a message-only window on a thread of its
// own, whose procedure holds that thread on an event when told to, as
// Explorer's might in a call that does not return. Explorer is not involved.
constexpr UINT kHoldTaskbarThread = WM_APP + 0x3A1;
// Runs a message loop until released, as Explorer does in a menu, say: what is
// sent to the window meanwhile is handled inside it.
constexpr UINT kLoopUntilReleased = WM_APP + 0x3A2;
HANDLE g_taskbarRelease = nullptr;
HANDLE g_taskbarLooping = nullptr;
HANDLE g_taskbarHeld = nullptr;

LRESULT CALLBACK TestTaskbarProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == kHoldTaskbarThread) {
        SetEvent(g_taskbarHeld);
        WaitForSingleObject(g_taskbarRelease, INFINITE);
        return 0;
    }
    if (msg == kLoopUntilReleased) {
        SetEvent(g_taskbarLooping);
        while (WaitForSingleObject(g_taskbarRelease, 5) == WAIT_TIMEOUT) {
            MSG queued;
            while (PeekMessageW(&queued, nullptr, 0, 0, PM_REMOVE)) {
                DispatchMessageW(&queued);
            }
        }
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

struct TestTaskbar {
    HWND wnd = nullptr;
    std::thread thread;
};

void StartTestTaskbar(TestTaskbar* taskbar) {
    static const bool registered = [] {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = TestTaskbarProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"SplitTrayTestTaskbar";
        return RegisterClassW(&wc) != 0;
    }();
    (void)registered;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    taskbar->thread = std::thread([taskbar, ready] {
        taskbar->wnd = CreateWindowExW(0, L"SplitTrayTestTaskbar", nullptr, 0, 0, 0, 0, 0,
                                       HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr),
                                       nullptr);
        SetEvent(ready);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            DispatchMessageW(&msg);
        }
    });
    WaitForSingleObject(ready, INFINITE);
    CloseHandle(ready);
}

void StopTestTaskbar(TestTaskbar* taskbar) {
    if (taskbar->wnd) {
        PostMessageW(taskbar->wnd, WM_CLOSE, 0, 0);
    }
    taskbar->thread.join();
}

// Waits up to three seconds for `condition`.
template <typename Condition>
bool WithinSeconds(Condition condition) {
    const ULONGLONG deadline = GetTickCount64() + 3000;
    while (!condition()) {
        if (GetTickCount64() >= deadline) {
            return false;
        }
        Sleep(10);
    }
    return true;
}

void Test_UnloadingWaitsForABusyTaskbarUntilItIsDone() {
    // From Windhawk's catalog review, replacing DECISIONS 73's time limit.
    // Windhawk frees the module as soon as Wh_ModUninit returns, so unloading
    // may not end before the mod's work on the taskbar's thread has: it used
    // to give up after a few seconds and keep the module loaded for the rest
    // of Explorer's life instead. A thread that is busy is waited for, the
    // hand-back is done there once it answers, and the subclass taken off
    // there - all before unloading ends.
    ForgetPlacements();
    ResetStore(DefaultSettings(), true);
    TestTaskbar taskbar;
    StartTestTaskbar(&taskbar);
    CHECK(WindhawkUtils::SetWindowSubclassFromAnyThread(taskbar.wnd,
                                                        ShellTrayWndSubclassProc, 0));
    g_taskbarRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_taskbarHeld = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_shellTrayWnd.store(taskbar.wnd);
    g_unloading.store(true);
    g_handedBack.store(false);
    // Held before unloading begins: a message sent to the thread before it
    // has taken the posted hold would be handled first.
    PostMessageW(taskbar.wnd, kHoldTaskbarThread, 0, 0);
    CHECK(WaitForSingleObject(g_taskbarHeld, 3000) == WAIT_OBJECT_0);

    const int removals = WindhawkUtils::UnsubclassCallCount();
    std::atomic<bool> returned{false};
    std::thread unloading([&] {
        HandBackToShell();
        returned.store(true);
    });
    // Longer than the time limit unloading used to keep to.
    Sleep(600);
    CHECK(!returned.load());
    CHECK(!g_handedBack.load());
    CHECK_EQ(WindhawkUtils::UnsubclassCallCount(), removals);

    SetEvent(g_taskbarRelease);
    CHECK(WithinSeconds([&] { return returned.load(); }));
    unloading.join();
    // Done by the time unloading ended, not after.
    CHECK(g_handedBack.load());
    CHECK_EQ(WindhawkUtils::UnsubclassCallCount(), removals + 1);
    CHECK(g_shellTrayWnd.load() == nullptr);

    StopTestTaskbar(&taskbar);
    CloseHandle(g_taskbarRelease);
    CloseHandle(g_taskbarHeld);
    g_taskbarRelease = nullptr;
    g_taskbarHeld = nullptr;
    g_unloading.store(false);
    g_handedBack.store(false);
}

void Test_UnloadingEndsOnlyOnceTheModsCodeHasLeftTheTaskbarsThread() {
    // DECISIONS 73, without its time limit (from Windhawk's catalog review).
    // The icons being back is not enough. Explorer may run a message loop
    // inside a message the mod's subclass passed on to it - a menu, say - and
    // the hand-back is then done inside that loop, with a call of the
    // subclass still under way below it. Unloading waits for that call to
    // return, however long the loop runs.
    ForgetPlacements();
    ResetStore(DefaultSettings(), true);
    TestTaskbar taskbar;
    StartTestTaskbar(&taskbar);
    CHECK(WindhawkUtils::SetWindowSubclassFromAnyThread(taskbar.wnd,
                                                        ShellTrayWndSubclassProc, 0));
    g_taskbarRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_taskbarLooping = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_shellTrayWnd.store(taskbar.wnd);
    g_unloading.store(true);
    g_handedBack.store(false);
    PostMessageW(taskbar.wnd, kLoopUntilReleased, 0, 0);
    CHECK(WaitForSingleObject(g_taskbarLooping, 3000) == WAIT_OBJECT_0);

    std::atomic<bool> returned{false};
    std::thread unloading([&] {
        HandBackToShell();
        returned.store(true);
    });
    CHECK(WithinSeconds([] { return g_handedBack.load(); }));  // inside the loop
    Sleep(600);
    CHECK(!returned.load());
    CHECK(g_subclassDepth.load() > 0);

    SetEvent(g_taskbarRelease);
    CHECK(WithinSeconds([&] { return returned.load(); }));
    unloading.join();
    CHECK_EQ(g_subclassDepth.load(), 0);

    StopTestTaskbar(&taskbar);
    CloseHandle(g_taskbarRelease);
    CloseHandle(g_taskbarLooping);
    g_taskbarRelease = nullptr;
    g_taskbarLooping = nullptr;
    g_unloading.store(false);
    g_handedBack.store(false);
}

void Test_AnAddThatIsTakenStartsTheIconAtVersionZero() {
    // DECISIONS 74. An add is a registration afresh, at version 0 until its
    // application asks for another. An icon re-registered by GUID from a new
    // window kept the version the old one had asked for, so clicks were packed
    // for version 4 to an application expecting version 0, and a move into
    // Explorer's tray replayed that version too.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    const GUID guid = {0x0DDBA110, 0x4321, 0x8765,
                       {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80}};
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x1000, 7, NIF_MESSAGE | NIF_TIP | NIF_GUID, L"first",
                     L"C:\\apps\\app.exe", &guid),
         &forward);
    Feed(MakeSetVersion(0x1000, 7, NOTIFYICON_VERSION_4), &forward);
    // Its application restarts and registers the GUID again from a new window,
    // asking for no version.
    Feed(MakePayload(NIM_ADD, 0x2000, 9, NIF_MESSAGE | NIF_TIP | NIF_GUID, L"again",
                     L"C:\\apps\\app.exe", &guid),
         &forward);
    CHECK(!forward);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        CHECK_EQ(static_cast<int>(g_icons.size()), 1);
        if (!g_icons.empty()) {
            CHECK_EQ(g_icons[0].version, 0u);
            // A left click is the button alone, as version 0 has it.
            CHECK_EQ(static_cast<int>(TrayCallbacksFor(g_icons[0].version, g_icons[0].uID,
                                                       WM_LBUTTONUP, POINT{1, 2})
                                          .size()),
                     1);
        }
    }

    // An icon Explorer has: Explorer's answer says which it was. An add it
    // refuses - it has the icon already - changes nothing; one it takes is a
    // new icon there.
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);
    const std::vector<BYTE> add =
        MakePayload(NIM_ADD, 0x3000, 2, NIF_MESSAGE | NIF_TIP, L"app", L"C:\\a\\app.exe");
    bool record = false;
    Feed(add, &forward, nullptr, &record);
    CHECK(forward && record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(add), true);
    }
    Feed(MakeSetVersion(0x3000, 2, NOTIFYICON_VERSION_4), &forward);
    Feed(add, &forward, nullptr, &record);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        RecordShellAnswerLocked(Parsed(add), false);
        const MirroredIcon* icon = IconByKeyLocked(L"app.exe#2");
        CHECK(icon && icon->version == static_cast<UINT>(NOTIFYICON_VERSION_4));
    }
    Feed(add, &forward, nullptr, &record);
    std::lock_guard<std::mutex> lock(g_mutex);
    RecordShellAnswerLocked(Parsed(add), true);
    const MirroredIcon* icon = IconByKeyLocked(L"app.exe#2");
    CHECK(icon && icon->version == 0u);
}

void Test_TheArrangeWindowsRowsFollowWhichIconsThereAreAndWhere() {
    // DECISIONS 75. An open arrange window was filled when it opened and again
    // only after a move made in it: an application started or closed
    // meanwhile, or an icon moved from a tray's menu, left rows missing or
    // stale. What it would list now is compared with what it was filled with.
    // Pictures and tooltips are left out: they change several times a second,
    // and filling the lists again resets what the user has selected.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);
    const std::wstring none = ArrangeLayoutNow();

    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5100, 1, NIF_MESSAGE | NIF_TIP, L"main tray",
                     L"C:\\a\\app.exe"),
         &forward);
    const std::wstring one = ArrangeLayoutNow();
    CHECK(one != none);

    HICON picture = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    Feed(MakePayload(NIM_MODIFY, 0x5100, 1, NIF_TIP, L"another tooltip", nullptr), &forward);
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x5100, 1, NIF_ICON, nullptr, nullptr), picture),
         &forward);
    CHECK(ArrangeLayoutNow() == one);

    MoveIconToTray(L"app.exe#1", Destination::Secondary);
    const std::wstring moved = ArrangeLayoutNow();
    CHECK(moved != one);
    SetIconHidden(L"app.exe#1", true);
    CHECK(ArrangeLayoutNow() != moved);
    SetIconHidden(L"app.exe#1", false);
    CHECK(ArrangeLayoutNow() == moved);

    Feed(MakePayload(NIM_DELETE, 0x5100, 1, 0, nullptr, nullptr), &forward);
    CHECK(ArrangeLayoutNow() == none);
    DestroyIcon(picture);
}

void Test_AnEmbeddedCellShowsThePictureTheStoreHas() {
    // DECISIONS 76. A cell updated in place was given a new picture only when
    // there was one, so an application that took its icon's picture away left
    // the old one showing in the taskbar until something else rebuilt the
    // tray. A picture that could not be copied for the cell is another matter:
    // the store still has it (DECISIONS 72), and the cell keeps what it shows.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);

    HICON picture = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    bool forward = true;
    Feed(WithIcon(MakePayload(NIM_ADD, 0x5200, 1, NIF_MESSAGE | NIF_TIP | NIF_ICON, L"app",
                              L"C:\\a\\app.exe"),
                  picture),
         &forward);
    {
        const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
        CHECK_EQ(static_cast<int>(cells.size()), 1);
        if (!cells.empty()) {
            CHECK(CellPictureOf(cells[0]) == CellPicture::Replace);
        }
    }

    // The store's picture cannot be copied for this refresh.
    HICON gone = CopyIcon(LoadIconW(nullptr, IDI_WARNING));
    DestroyIcon(gone);
    HICON kept = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (MirroredIcon* icon = IconByKeyLocked(L"app.exe#1")) {
            kept = icon->icon;
            icon->icon = gone;
        }
    }
    {
        const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
        if (!cells.empty()) {
            CHECK(CellPictureOf(cells[0]) == CellPicture::Keep);
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (MirroredIcon* icon = IconByKeyLocked(L"app.exe#1")) {
            icon->icon = kept;
        }
    }

    // Taken away by its application.
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x5200, 1, NIF_ICON, nullptr, nullptr), nullptr),
         &forward);
    {
        const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
        if (!cells.empty()) {
            CHECK(CellPictureOf(cells[0]) == CellPicture::Clear);
        }
    }
    DestroyIcon(picture);
}

void Test_AnEmbeddedTraysTooltipsFollowTheSetting() {
    // DECISIONS 77. "Show tooltips" was read only by the floating trays: the
    // cells in a taskbar, and in its overflow popup, had their icon's tooltip
    // whatever it said.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.showTooltips = false;
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5300, 1, NIF_MESSAGE | NIF_TIP, L"app", L"C:\\a\\app.exe"),
         &forward);
    {
        const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
        CHECK_EQ(static_cast<int>(cells.size()), 1);
        if (!cells.empty()) {
            CHECK_WSTR(cells[0].tip, L"");
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_settings.showTooltips = true;
    }
    const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
    if (!cells.empty()) {
        CHECK_WSTR(cells[0].tip, L"app");
    }
}

// ---------------------------------------------------------------------------
// Split Tray's trays working like Explorer's own (DECISIONS 78 on)
// ---------------------------------------------------------------------------

// Explorer as it answers, holding what it is given: an add fails for an icon it
// has, anything else for one it does not. Whether it holds an icon hidden
// (NIS_HIDDEN) is kept per icon.
struct StandInExplorer {
    std::set<std::pair<HWND, UINT>> held;
    std::set<std::pair<HWND, UINT>> hidden;
    std::vector<TrayNotification> handed;

    LRESULT Answer(const std::vector<BYTE>& record) {
        const TrayNotification n = Parsed(record);
        handed.push_back(n);
        const auto id = std::make_pair(n.ownerWnd, n.uID);
        bool taken;
        if (n.message == NIM_ADD) {
            taken = held.insert(id).second;
        } else if (n.message == NIM_DELETE) {
            taken = held.erase(id) > 0;
            hidden.erase(id);
        } else {
            taken = held.count(id) > 0;
        }
        if (taken && (n.flags & NIF_STATE) && (n.stateMask & NIS_HIDDEN)) {
            if (n.state & NIS_HIDDEN) {
                hidden.insert(id);
            } else {
                hidden.erase(id);
            }
        }
        return taken ? TRUE : FALSE;
    }
};

std::vector<BYTE> WithBalloon(std::vector<BYTE> payload, PCWSTR text, PCWSTR title,
                              DWORD infoFlags) {
    memset(payload.data() + wire::kInfo, 0, wire::kInfoChars * sizeof(wchar_t));
    memcpy(payload.data() + wire::kInfo, text, wcslen(text) * sizeof(wchar_t));
    memset(payload.data() + wire::kInfoTitle, 0, wire::kInfoTitleChars * sizeof(wchar_t));
    memcpy(payload.data() + wire::kInfoTitle, title, wcslen(title) * sizeof(wchar_t));
    WriteDword(payload.data(), wire::kInfoFlags, infoFlags);
    WriteDword(payload.data(), wire::kFlags,
               ReadDword(payload.data(), wire::kFlags) | NIF_INFO);
    return payload;
}

void Test_AHiddenCopyIsTheWholeIconAndABalloonGoesOnIt() {
    // DECISIONS 78. Explorer shows a balloon, as a Windows notification, only
    // for an icon it holds, and it holds one hidden as well as one it shows:
    // measured, a hidden icon's balloon reached the notification history.
    // The copy Explorer is given is the whole icon, hidden; a balloon is that
    // copy as a modify, with the balloon taken from the application's message.
    HICON picture = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    const std::vector<BYTE> state = MakePayload(
        NIM_ADD, 0x10, 3, NIF_MESSAGE | NIF_TIP | NIF_ICON, L"app", L"C:\\a\\app.exe");
    const TrayNotification copy = Parsed(HiddenCopyRecordFor(state, picture, NIM_ADD));
    CHECK_EQ(copy.message, static_cast<DWORD>(NIM_ADD));
    CHECK((copy.flags & NIF_STATE) && (copy.state & NIS_HIDDEN) &&
          (copy.stateMask & NIS_HIDDEN));
    CHECK(copy.icon == picture);
    CHECK_WSTR(copy.tip, L"app");

    const std::vector<BYTE> message = WithBalloon(
        MakePayload(NIM_MODIFY, 0x10, 3, NIF_TIP, L"its own tooltip", nullptr), L"hello",
        L"title", NIIF_WARNING);
    const std::vector<BYTE> balloon =
        BalloonRecordFor(HiddenCopyRecordFor(state, picture, NIM_MODIFY), message);
    const TrayNotification b = Parsed(balloon);
    CHECK_EQ(b.message, static_cast<DWORD>(NIM_MODIFY));
    CHECK((b.flags & NIF_INFO) != 0);
    CHECK((b.state & NIS_HIDDEN) != 0);  // still hidden
    CHECK_WSTR(b.tip, L"app");           // the icon as the store has it
    CHECK_WSTR(ReadFixedString(balloon.data(), wire::kInfo, wire::kInfoChars), L"hello");
    CHECK_WSTR(ReadFixedString(balloon.data(), wire::kInfoTitle, wire::kInfoTitleChars),
               L"title");
    CHECK_EQ(ReadDword(balloon.data(), wire::kInfoFlags), static_cast<DWORD>(NIIF_WARNING));
    DestroyIcon(picture);
}

void Test_ABalloonForAnIconInSplitTraysTraysIsShownByExplorer() {
    // DECISIONS 78. A balloon for an icon that lives only in Split Tray's trays
    // was swallowed with the rest of its message, so it was never shown.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5400, 1, NIF_MESSAGE | NIF_TIP, L"app", L"C:\\a\\app.exe"),
         &forward);
    Feed(MakeSetVersion(0x5400, 1, NOTIFYICON_VERSION_4), &forward);
    const std::vector<BYTE> message = WithBalloon(
        MakePayload(NIM_MODIFY, 0x5400, 1, 0, nullptr, nullptr), L"first", L"t", NIIF_INFO);
    Feed(message, &forward);
    CHECK(!forward);

    StandInExplorer explorer;
    auto deliver = [&](HWND, const std::vector<BYTE>& record) {
        return explorer.Answer(record);
    };
    // The first: Explorer is given a hidden copy, then its version - so the
    // application's callbacks are packed as it asked - then the balloon.
    CHECK(ShowBalloonThroughShell(Parsed(message), message, deliver));
    CHECK_EQ(static_cast<int>(explorer.handed.size()), 3);
    if (explorer.handed.size() == 3) {
        CHECK_EQ(explorer.handed[0].message, static_cast<DWORD>(NIM_ADD));
        CHECK((explorer.handed[0].state & NIS_HIDDEN) != 0);
        CHECK_EQ(explorer.handed[1].message, static_cast<DWORD>(NIM_SETVERSION));
        CHECK_EQ(explorer.handed[1].version, static_cast<DWORD>(NOTIFYICON_VERSION_4));
        CHECK_EQ(explorer.handed[2].message, static_cast<DWORD>(NIM_MODIFY));
        CHECK((explorer.handed[2].flags & NIF_INFO) != 0);
    }
    CHECK(explorer.hidden.count({reinterpret_cast<HWND>(0x5400), 1u}) == 1);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const MirroredIcon* icon = IconByKeyLocked(L"app.exe#1");
        CHECK(icon && icon->shellHidden && !icon->forwardedToShell);
    }

    // The next goes on the copy Explorer has, after its version.
    explorer.handed.clear();
    CHECK(ShowBalloonThroughShell(Parsed(message), message, deliver));
    CHECK_EQ(static_cast<int>(explorer.handed.size()), 2);

    // Explorer restarted and holds nothing: the version and the balloon are
    // refused, and the copy is made again.
    explorer.held.clear();
    explorer.hidden.clear();
    explorer.handed.clear();
    CHECK(ShowBalloonThroughShell(Parsed(message), message, deliver));
    CHECK_EQ(static_cast<int>(explorer.handed.size()), 5);
    CHECK(explorer.hidden.count({reinterpret_cast<HWND>(0x5400), 1u}) == 1);

    // An icon Explorer shows has its balloons passed on as they are, with the
    // rest of its application's message.
    s.defaultTray = Destination::Primary;
    ResetStore(s, true);
    Feed(MakePayload(NIM_ADD, 0x5500, 1, NIF_MESSAGE | NIF_TIP, L"app", L"C:\\a\\app.exe"),
         &forward);
    const std::vector<BYTE> shown = WithBalloon(
        MakePayload(NIM_MODIFY, 0x5500, 1, 0, nullptr, nullptr), L"shown", L"t", NIIF_INFO);
    Feed(shown, &forward);
    CHECK(forward);
    explorer.handed.clear();
    CHECK(!ShowBalloonThroughShell(Parsed(shown), shown, deliver));
    CHECK(explorer.handed.empty());
}

// An icon of this test's, in tray 2, with a hidden copy in `explorer`. Its
// application sets its state only to hide it; most never set one at all.
void MakeIconWithHiddenCopy(StandInExplorer* explorer, DWORD wnd, UINT uID,
                            DWORD state = 0) {
    bool forward = true;
    Feed(WithState(MakePayload(NIM_ADD, wnd, uID,
                               NIF_MESSAGE | NIF_TIP | (state ? NIF_STATE : 0), L"app",
                               L"C:\\a\\app.exe"),
                   state, state ? NIS_HIDDEN : 0),
         &forward);
    Feed(MakeSetVersion(wnd, uID, NOTIFYICON_VERSION_4), &forward);
    const std::vector<BYTE> message = WithBalloon(
        MakePayload(NIM_MODIFY, wnd, uID, 0, nullptr, nullptr), L"hello", L"t", NIIF_INFO);
    Feed(message, &forward);
    ShowBalloonThroughShell(Parsed(message), message,
                            [&](HWND, const std::vector<BYTE>& record) {
                                return explorer->Answer(record);
                            });
}

void Test_AnIconExplorerHoldsHiddenIsShownNotAddedAgain() {
    // DECISIONS 78. Explorer refuses to add an icon it holds, hidden or not.
    // Moved into Explorer's tray, or handed back as the mod unloads, an icon
    // with a hidden copy is the whole icon again as a modify, which takes
    // NIS_HIDDEN off - unless its application hid it.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    StandInExplorer explorer;
    MakeIconWithHiddenCopy(&explorer, 0x5600, 1);
    MakeIconWithHiddenCopy(&explorer, 0x5600, 2, NIS_HIDDEN);  // hidden by its application
    explorer.handed.clear();

    MoveIconToTray(L"app.exe#1", Destination::Primary);
    MoveIconToTray(L"app.exe#2", Destination::Primary);
    SettleShellIcons([&](HWND, const std::vector<BYTE>& record) {
        return explorer.Answer(record);
    });
    int modifies = 0;
    for (const TrayNotification& n : explorer.handed) {
        CHECK(n.message != static_cast<DWORD>(NIM_ADD));
        if (n.message == NIM_MODIFY) {
            modifies++;
            CHECK((n.flags & NIF_STATE) && (n.stateMask & NIS_HIDDEN));
        }
    }
    CHECK_EQ(modifies, 2);
    CHECK(explorer.hidden.count({reinterpret_cast<HWND>(0x5600), 1u}) == 0);
    CHECK(explorer.hidden.count({reinterpret_cast<HWND>(0x5600), 2u}) == 1);
    std::lock_guard<std::mutex> lock(g_mutex);
    for (PCWSTR key : {L"app.exe#1", L"app.exe#2"}) {
        const MirroredIcon* icon = IconByKeyLocked(key);
        CHECK(icon && icon->forwardedToShell && !icon->shellHidden);
    }
}

void Test_AnApplicationsDeleteAndVersionReachTheHiddenCopy() {
    // DECISIONS 78. Its application's delete takes the hidden copy out of
    // Explorer, and the notifications it showed with it, as deleting an icon
    // Explorer shows does. Its messages are not passed on, even a new version
    // - one refused by an Explorer that has lost the copy would tell the
    // application its tray cannot take it - but each balloon brings the copy
    // up to date, version first: Explorer packs the balloon's callbacks by it.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    StandInExplorer explorer;
    MakeIconWithHiddenCopy(&explorer, 0x5700, 1);
    bool forward = true;
    Feed(MakeSetVersion(0x5700, 1, NOTIFYICON_VERSION), &forward);
    CHECK(!forward);
    Feed(MakePayload(NIM_MODIFY, 0x5700, 1, NIF_TIP, L"new tooltip", nullptr), &forward);
    CHECK(!forward);
    explorer.handed.clear();
    const std::vector<BYTE> message = WithBalloon(
        MakePayload(NIM_MODIFY, 0x5700, 1, 0, nullptr, nullptr), L"again", L"t", NIIF_INFO);
    Feed(message, &forward);
    CHECK(ShowBalloonThroughShell(Parsed(message), message,
                                  [&](HWND, const std::vector<BYTE>& record) {
                                      return explorer.Answer(record);
                                  }));
    CHECK_EQ(static_cast<int>(explorer.handed.size()), 2);
    if (explorer.handed.size() == 2) {
        CHECK_EQ(explorer.handed[0].message, static_cast<DWORD>(NIM_SETVERSION));
        CHECK_EQ(explorer.handed[0].version, static_cast<DWORD>(NOTIFYICON_VERSION));
        CHECK_WSTR(explorer.handed[1].tip, L"new tooltip");
    }

    bool retract = false;
    forward = true;
    Feed(MakePayload(NIM_DELETE, 0x5700, 1, 0, nullptr, nullptr), &forward, &retract);
    CHECK(!forward && retract);
}

void Test_AVersion4IconWithoutShowTipHasItsOwnPopupNotATooltip() {
    // DECISIONS 79. From version 4 Explorer shows an icon's tooltip only when
    // it asks for it with NIF_SHOWTIP; otherwise the application draws its own
    // popup, on NIN_POPUPOPEN. Split Tray's trays showed the tooltip anyway,
    // over the application's popup or instead of it.
    CHECK(UsesStandardTooltip(0, 0));
    CHECK(UsesStandardTooltip(NOTIFYICON_VERSION, NIF_TIP));
    CHECK(!UsesStandardTooltip(NOTIFYICON_VERSION_4, NIF_TIP));
    CHECK(UsesStandardTooltip(NOTIFYICON_VERSION_4, NIF_TIP | NIF_SHOWTIP));

    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5800, 1, NIF_MESSAGE | NIF_TIP, L"own popup", L"C:\\a\\app.exe"),
         &forward);
    Feed(MakeSetVersion(0x5800, 1, NOTIFYICON_VERSION_4), &forward);
    Feed(MakePayload(NIM_ADD, 0x5800, 2, NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP, L"tooltip",
                     L"C:\\a\\app.exe"),
         &forward);
    Feed(MakeSetVersion(0x5800, 2, NOTIFYICON_VERSION_4), &forward);

    const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
    CHECK_EQ(static_cast<int>(cells.size()), 2);
    if (cells.size() == 2) {
        CHECK_WSTR(cells[0].tip, L"");
        CHECK_WSTR(cells[1].tip, L"tooltip");
    }
    const FloatingView view = FloatingViewOf(2, /*withIcons=*/false);
    CHECK_EQ(static_cast<int>(view.tips.size()), 2);
    if (view.tips.size() == 2) {
        CHECK_WSTR(view.tips[0], L"");
        CHECK_WSTR(view.tips[1], L"tooltip");
    }
}

void Test_AnIconIsSelectedFromTheKeyboardAsExplorerDoes() {
    // DECISIONS 80. From version 3 Explorer sends NIN_KEYSELECT for Enter or
    // Space on a focused icon, and WM_CONTEXTMENU for Shift+F10 or the menu
    // key; an older icon gets the clicks those stand for.
    const POINT anchor = {5, 6};
    const auto select4 = KeyboardCallbacksFor(NOTIFYICON_VERSION_4, 7, false, anchor);
    CHECK_EQ(static_cast<int>(select4.size()), 1);
    if (!select4.empty()) {
        CHECK_EQ(select4[0].lParam, static_cast<LPARAM>(MAKELPARAM(NIN_KEYSELECT, 7)));
        CHECK_EQ(select4[0].wParam, static_cast<WPARAM>(MAKEWPARAM(5, 6)));
    }
    const auto menu4 = KeyboardCallbacksFor(NOTIFYICON_VERSION_4, 7, true, anchor);
    CHECK(menu4.size() == 1 &&
          menu4[0].lParam == static_cast<LPARAM>(MAKELPARAM(WM_CONTEXTMENU, 7)));
    const auto select3 = KeyboardCallbacksFor(NOTIFYICON_VERSION, 7, false, anchor);
    CHECK(select3.size() == 1 && select3[0].wParam == 7 &&
          select3[0].lParam == static_cast<LPARAM>(NIN_KEYSELECT));
    const auto select0 = KeyboardCallbacksFor(0, 7, false, anchor);
    CHECK(select0.size() == 2 && select0[0].lParam == WM_LBUTTONDOWN &&
          select0[1].lParam == WM_LBUTTONUP);
    const auto menu0 = KeyboardCallbacksFor(0, 7, true, anchor);
    CHECK(menu0.size() == 2 && menu0[0].lParam == WM_RBUTTONDOWN &&
          menu0[1].lParam == WM_RBUTTONUP);
}

void Test_FocusIsGivenBackToAnIconInSplitTraysTrays() {
    // DECISIONS 80. An application gives the keyboard focus back to its icon
    // with NIM_SETFOCUS once its menu closes. For an icon Explorer does not
    // have, passing it on failed; the mod answers it, and focuses the cell.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.rules = {{L"main.exe", Destination::Primary}};
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5900, 1, NIF_MESSAGE | NIF_TIP, L"tray 2", L"C:\\a\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_SETFOCUS, 0x5900, 1, 0, nullptr, nullptr), &forward);
    CHECK(!forward);
    Feed(MakePayload(NIM_ADD, 0x5901, 1, NIF_MESSAGE | NIF_TIP, L"main", L"C:\\a\\main.exe"),
         &forward);
    Feed(MakePayload(NIM_SETFOCUS, 0x5901, 1, 0, nullptr, nullptr), &forward);
    CHECK(forward);
    Feed(MakePayload(NIM_SETFOCUS, 0x5902, 1, 0, nullptr, nullptr), &forward);
    CHECK(forward);  // one the mod does not know
}

void Test_EveryCellHasANameForScreenReaders() {
    // DECISIONS 81. Screen readers could not reach Split Tray's trays: a cell
    // was a picture and nothing else. Each now has its icon's name - the
    // first line of its tooltip, or its program's - whether tooltips are
    // shown or not.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.showTooltips = false;
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5A00, 1, NIF_MESSAGE | NIF_TIP, L"Backups\nlast run 2 min ago",
                     L"C:\\a\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, 0x5A00, 2, NIF_MESSAGE, nullptr, L"C:\\a\\app.exe"), &forward);
    const std::vector<CellSnapshot> cells = CellSnapshotsOf(2);
    CHECK_EQ(static_cast<int>(cells.size()), 2);
    if (cells.size() == 2) {
        CHECK_WSTR(cells[0].name, L"Backups");
        CHECK_WSTR(cells[1].name, L"app.exe");
    }
}

void Test_ACellIsDrawnAgainOnlyWhenItsPictureChanges() {
    // DECISIONS 82. Every refresh of a tray in the taskbar copied every
    // icon's picture and made a bitmap of it again, on the taskbar's thread,
    // several times a second while SystemInformer draws its graphs - for the
    // one picture that changed. A cell drawn from a picture the store has not
    // changed since is left as it is, and nothing is copied for it.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    ResetStore(s, true);
    HICON first = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    HICON second = CopyIcon(LoadIconW(nullptr, IDI_WARNING));
    bool forward = true;
    Feed(WithIcon(MakePayload(NIM_ADD, 0x5B00, 1, NIF_MESSAGE | NIF_TIP | NIF_ICON, L"app",
                              L"C:\\a\\app.exe"),
                  first),
         &forward);

    std::map<uint64_t, uint64_t> drawn;
    auto drawAll = [&] {
        std::vector<CellPicture> what;
        for (const CellSnapshot& cell : CellSnapshotsOf(2, &drawn)) {
            what.push_back(CellPictureOf(cell));
            if (what.back() != CellPicture::Keep) {
                drawn[cell.serial] = cell.pictureRevision;
            }
        }
        return what;
    };
    CHECK(drawAll() == std::vector<CellPicture>{CellPicture::Replace});
    {
        const std::vector<CellSnapshot> cells = CellSnapshotsOf(2, &drawn);
        CHECK(!cells.empty() && cells[0].icon.get() == nullptr);  // nothing copied
    }
    CHECK(drawAll() == std::vector<CellPicture>{CellPicture::Keep});
    Feed(MakePayload(NIM_MODIFY, 0x5B00, 1, NIF_TIP, L"new tooltip", nullptr), &forward);
    CHECK(drawAll() == std::vector<CellPicture>{CellPicture::Keep});
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x5B00, 1, NIF_ICON, nullptr, nullptr), second),
         &forward);
    CHECK(drawAll() == std::vector<CellPicture>{CellPicture::Replace});
    Feed(WithIcon(MakePayload(NIM_MODIFY, 0x5B00, 1, NIF_ICON, nullptr, nullptr), nullptr),
         &forward);
    CHECK(drawAll() == std::vector<CellPicture>{CellPicture::Clear});
    CHECK(drawAll() == std::vector<CellPicture>{CellPicture::Keep});
    DestroyIcon(first);
    DestroyIcon(second);
}

// A menu item's text, by position.
std::wstring MenuText(HMENU menu, int position) {
    WCHAR text[256] = {};
    GetMenuStringW(menu, static_cast<UINT>(position), text, ARRAYSIZE(text), MF_BYPOSITION);
    return text;
}

void Test_AFloatingTraysMenuOffersEveryOtherTrayAndDoesWhatIsChosen() {
    // The mod's menu on a floating tray is modal, so no test had ever run it:
    // coverage found it (2026-09-26). It is built and acted on apart from
    // being shown. On an icon: a move to each other tray, then the icons in
    // other trays to bring here, the arrange window and the reset.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.rules = {{L"main.exe", Destination::Primary}};
    s.extraTrays = {{0, Corner::BottomLeft}};
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5D00, 1, NIF_MESSAGE | NIF_TIP, L"in tray 2",
                     L"C:\\a\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, 0x5D01, 1, NIF_MESSAGE | NIF_TIP, L"in the primary tray",
                     L"C:\\a\\main.exe"),
         &forward);
    const FloatingView view = FloatingViewOf(2, /*withIcons=*/false);
    CHECK_EQ(static_cast<int>(view.serials.size()), 1);
    if (view.serials.size() != 1) {
        return;
    }
    FloatingTrayMenu menu = BuildFloatingTrayMenu(2, view.serials[0]);
    CHECK(menu.menu != nullptr);
    CHECK_EQ(GetMenuItemCount(menu.menu), 7);
    CHECK(MenuText(menu.menu, 0) == L"Move to the primary tray");
    CHECK(MenuText(menu.menu, 1).starts_with(L"Move to tray 3"));
    CHECK_WSTR(MenuText(menu.menu, 3), L"Move an icon to this tray");
    HMENU here = GetSubMenu(menu.menu, 3);
    CHECK(here && GetMenuItemCount(here) == 1 && MenuText(here, 0) == L"in the primary tray");
    CHECK_WSTR(MenuText(menu.menu, 5), L"Arrange icons\x2026");
    CHECK_WSTR(MenuText(menu.menu, 6), L"Reset moved icons");

    RunFloatingTrayMenuCommand(menu, 2, 0);  // dismissed: nothing
    CHECK_EQ(ShownTrayOf(L"app.exe#1"), 2);
    RunFloatingTrayMenuCommand(menu, 2, FloatingTrayMenu::kMoveThis + 3);
    CHECK_EQ(ShownTrayOf(L"app.exe#1"), 3);
    RunFloatingTrayMenuCommand(menu, 2, FloatingTrayMenu::kMoveHere + 0);
    CHECK_EQ(ShownTrayOf(L"main.exe#1"), 2);
    RunFloatingTrayMenuCommand(menu, 2, FloatingTrayMenu::kMoveHere + 5);  // no such icon
    RunFloatingTrayMenuCommand(menu, 2, FloatingTrayMenu::kResetMoved);
    CHECK_EQ(ShownTrayOf(L"app.exe#1"), 2);
    CHECK_EQ(ShownTrayOf(L"main.exe#1"), 0);
    DestroyMenu(menu.menu);

    // On an empty tray's handle: no moves of an icon of its own, and every
    // icon offered.
    FloatingTrayMenu handle = BuildFloatingTrayMenu(3, 0);
    CHECK(handle.menu && GetMenuItemCount(handle.menu) == 4);
    CHECK_EQ(static_cast<int>(handle.others.size()), 2);
    if (handle.menu) {
        DestroyMenu(handle.menu);
    }
}

// ---------------------------------------------------------------------------
// Screen readers and the keyboard in a floating tray (DECISIONS 85)
// ---------------------------------------------------------------------------

void Test_TheKeyboardMovesAcrossAFloatingTraysCells() {
    // Arrows move between cells, and up and down by a row; Home and End go to
    // the ends. From no cell, any of them goes to the first; other keys do not
    // move it. Five cells in rows of three:  0 1 2 / 3 4.
    CHECK_EQ(NextFocusIndex(-1, VK_RIGHT, 5, 3), 0);
    CHECK_EQ(NextFocusIndex(-1, VK_RETURN, 5, 3), -1);
    CHECK_EQ(NextFocusIndex(0, VK_RIGHT, 5, 3), 1);
    CHECK_EQ(NextFocusIndex(4, VK_RIGHT, 5, 3), 4);
    CHECK_EQ(NextFocusIndex(0, VK_LEFT, 5, 3), 0);
    CHECK_EQ(NextFocusIndex(3, VK_LEFT, 5, 3), 2);
    CHECK_EQ(NextFocusIndex(1, VK_DOWN, 5, 3), 4);
    CHECK_EQ(NextFocusIndex(2, VK_DOWN, 5, 3), 2);  // nothing below it
    CHECK_EQ(NextFocusIndex(4, VK_UP, 5, 3), 1);
    CHECK_EQ(NextFocusIndex(1, VK_UP, 5, 3), 1);
    CHECK_EQ(NextFocusIndex(3, VK_HOME, 5, 3), 0);
    CHECK_EQ(NextFocusIndex(0, VK_END, 5, 3), 4);
    CHECK_EQ(NextFocusIndex(2, 'A', 5, 3), 2);
    CHECK_EQ(NextFocusIndex(0, VK_RIGHT, 0, 3), -1);  // no cells at all
}

// A provider's property, as a client of UI Automation reads it.
VARIANT UiaProperty(IUnknown* element, PROPERTYID property, HRESULT* result = nullptr) {
    VARIANT value;
    VariantInit(&value);
    IRawElementProviderSimple* simple = nullptr;
    HRESULT hr = element ? element->QueryInterface(__uuidof(IRawElementProviderSimple),
                                                   reinterpret_cast<void**>(&simple))
                         : E_POINTER;
    if (SUCCEEDED(hr)) {
        hr = simple->GetPropertyValue(property, &value);
        simple->Release();
    }
    if (result) {
        *result = hr;
    }
    return value;
}

std::wstring UiaName(IUnknown* element) {
    VARIANT value = UiaProperty(element, UIA_NamePropertyId);
    std::wstring name = value.vt == VT_BSTR && value.bstrVal ? value.bstrVal : L"";
    VariantClear(&value);
    return name;
}

LONG UiaControlType(IUnknown* element) {
    VARIANT value = UiaProperty(element, UIA_ControlTypePropertyId);
    return value.vt == VT_I4 ? value.lVal : 0;
}

bool UiaHasFocus(IUnknown* element) {
    VARIANT value = UiaProperty(element, UIA_HasKeyboardFocusPropertyId);
    return value.vt == VT_BOOL && value.boolVal == VARIANT_TRUE;
}

IRawElementProviderFragment* UiaNavigate(IRawElementProviderFragment* from,
                                         NavigateDirection direction) {
    IRawElementProviderFragment* to = nullptr;
    if (from) {
        from->Navigate(direction, &to);
    }
    return to;
}

std::vector<LONG> UiaRuntimeId(IRawElementProviderFragment* element) {
    std::vector<LONG> id;
    SAFEARRAY* parts = nullptr;
    if (element && SUCCEEDED(element->GetRuntimeId(&parts)) && parts) {
        LONG last = -1;
        SafeArrayGetUBound(parts, 1, &last);
        for (LONG i = 0; i <= last; i++) {
            LONG part = 0;
            SafeArrayGetElement(parts, &i, &part);
            id.push_back(part);
        }
        SafeArrayDestroy(parts);
    }
    return id;
}

void Test_AFloatingTrayIsAToolBarOfNamedButtonsForScreenReaders() {
    // A floating tray is a plain window that paints its icons, so to a screen
    // reader it was one blank rectangle. Its provider makes it a tool bar with
    // a button for each icon, named as the taskbar's cells are (DECISIONS 81),
    // in the order they are drawn, each where it is drawn. Read here without
    // a window, straight from the provider.
    ForgetPlacements();
    Settings s = DefaultSettings();
    s.defaultTray = Destination::Secondary;
    s.showTooltips = false;
    ResetStore(s, true);
    bool forward = true;
    Feed(MakePayload(NIM_ADD, 0x5C00, 1, NIF_MESSAGE | NIF_TIP, L"Backups\nlast run 2 min ago",
                     L"C:\\a\\app.exe"),
         &forward);
    Feed(MakePayload(NIM_ADD, 0x5C00, 2, NIF_MESSAGE, nullptr, L"C:\\a\\app.exe"), &forward);
    const FloatingView view = FloatingViewOf(2, /*withIcons=*/false);
    CHECK_EQ(static_cast<int>(view.serials.size()), 2);
    if (view.serials.size() != 2) {
        return;
    }
    g_floatingTrays[2] = FloatingTray{};
    auto* root = new FloatingTrayUia(nullptr, 2);
    root->TestWithoutWindow();

    CHECK_EQ(UiaControlType(static_cast<IRawElementProviderSimple*>(root)),
             static_cast<LONG>(UIA_ToolBarControlTypeId));
    CHECK(UiaName(static_cast<IRawElementProviderSimple*>(root)).find(L"tray 2") !=
          std::wstring::npos);

    IRawElementProviderFragment* first = UiaNavigate(root, NavigateDirection_FirstChild);
    IRawElementProviderFragment* second = UiaNavigate(first, NavigateDirection_NextSibling);
    IRawElementProviderFragment* last = UiaNavigate(root, NavigateDirection_LastChild);
    CHECK(first && second && last);
    CHECK_EQ(UiaControlType(first), static_cast<LONG>(UIA_ButtonControlTypeId));
    CHECK_WSTR(UiaName(first), L"Backups");
    CHECK_WSTR(UiaName(second), L"app.exe");
    CHECK(UiaName(last) == UiaName(second));
    CHECK(UiaNavigate(second, NavigateDirection_NextSibling) == nullptr);
    CHECK(UiaNavigate(first, NavigateDirection_PreviousSibling) == nullptr);
    IRawElementProviderFragment* parent = UiaNavigate(second, NavigateDirection_Parent);
    CHECK(parent == static_cast<IRawElementProviderFragment*>(root));
    if (parent) {
        parent->Release();
    }

    // Each is its own element: runtime ids that differ, and the same object
    // for the same icon however it is reached.
    CHECK(!UiaRuntimeId(first).empty());
    CHECK(UiaRuntimeId(first) != UiaRuntimeId(second));
    CHECK(UiaRuntimeId(second) == UiaRuntimeId(last));

    // Where each is drawn.
    UiaRect bounds = {};
    RECT cell = {};
    CHECK(first && SUCCEEDED(first->get_BoundingRectangle(&bounds)));
    CHECK(CellScreenRect(view.layout, 0, view.Cells(), &cell));
    CHECK_EQ(static_cast<LONG>(bounds.left), cell.left);
    CHECK_EQ(static_cast<LONG>(bounds.top), cell.top);
    CHECK_EQ(static_cast<LONG>(bounds.width), cell.right - cell.left);
    IRawElementProviderFragment* hit = nullptr;
    root->ElementProviderFromPoint(cell.left + 1.0, cell.top + 1.0, &hit);
    CHECK(hit && UiaRuntimeId(hit) == UiaRuntimeId(first));

    // An icon's button can be pressed, as Explorer's are.
    IUnknown* invoke = nullptr;
    IRawElementProviderSimple* firstSimple = nullptr;
    if (first) {
        first->QueryInterface(__uuidof(IRawElementProviderSimple),
                              reinterpret_cast<void**>(&firstSimple));
    }
    CHECK(firstSimple && SUCCEEDED(firstSimple->GetPatternProvider(UIA_InvokePatternId,
                                                                   &invoke)) &&
          invoke);

    // The keyboard's cell is the one with the focus, while the tray has it.
    CHECK(!UiaHasFocus(second));
    g_floatingTrays[2].focusSerial = view.serials[1];
    CHECK(!UiaHasFocus(second));  // the tray itself does not have it
    g_floatingTrays[2].focused = true;
    CHECK(UiaHasFocus(second));
    CHECK(!UiaHasFocus(first));
    IRawElementProviderFragment* focus = nullptr;
    root->GetFocus(&focus);
    CHECK(focus && UiaRuntimeId(focus) == UiaRuntimeId(second));

    // An icon that goes takes its button with it: a screen reader holding it
    // is told it is no longer there, not handed a stale name.
    Feed(MakePayload(NIM_DELETE, 0x5C00, 1, 0, nullptr, nullptr), &forward);
    HRESULT gone = S_OK;
    VARIANT stale = UiaProperty(first, UIA_NamePropertyId, &gone);
    VariantClear(&stale);
    CHECK_EQ(gone, static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE));
    CHECK_WSTR(UiaName(second), L"app.exe");

    // An empty tray's handle is a button too, for the mod's menu.
    Feed(MakePayload(NIM_DELETE, 0x5C00, 2, 0, nullptr, nullptr), &forward);
    IRawElementProviderFragment* handle = UiaNavigate(root, NavigateDirection_FirstChild);
    CHECK(handle && UiaName(handle) == L"Split Tray menu");

    for (IUnknown* held : std::initializer_list<IUnknown*>{
             first, second, last, hit, invoke, firstSimple, focus, handle}) {
        if (held) {
            held->Release();
        }
    }
    root->Disconnect();
    CHECK(UiaNavigate(root, NavigateDirection_FirstChild) == nullptr);
    root->Release();
    g_floatingTrays.erase(2);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("split-tray regression tests\n\n");

    TestRunner runner;

    printf("wire protocol (against real shell32 captures)\n");
    runner.Run("parses the real Unicode v4 payload", Test_ParsesRealUnicodeV4Payload);
    runner.Run("parses a GUID-identified icon", Test_ParsesGuidIdentifiedIcon);
    runner.Run("legacy and ANSI callers arrive normalised",
               Test_ShellNormalisesLegacyAndAnsiCallers);
    runner.Run("parses NIM_SETVERSION", Test_ParsesSetVersion);
    runner.Run("rejects non-tray WM_COPYDATA", Test_RejectsNonTrayCopyData);
    runner.Run("a record of another shape is left to Explorer",
               Test_ARecordOfAnotherShapeIsLeftToExplorer);
    runner.Run("replay rewrites only dwMessage",
               Test_PayloadWithMessageRewritesOnlyTheMessage);
    runner.Run("parses Shell_NotifyIconGetRect's question", Test_ParsesAnIconRectQuery);
    runner.Run("rejects what is not a rect question",
               Test_RejectsWhatIsNotAnIconRectQuery);
    runner.Run("rect answer is what shell32 reads", Test_IconRectReplyIsWhatShell32Reads);

    printf("\nsettings\n");
    runner.Run("parses destination and corner", Test_ParsesDestinationAndCorner);
    runner.Run("parses hex colour, rejects garbage",
               Test_ParsesHexColourAndRejectsGarbage);
    runner.Run("reads the routing array", Test_LoadSettingsReadsTheRoutingArray);
    runner.Run("clamps hostile values", Test_LoadSettingsClampsHostileValues);

    printf("\nrouting\n");
    runner.Run("matches exe name case-insensitively",
               Test_RoutesByExecutableNameCaseInsensitively);
    runner.Run("rules with a separator match the path",
               Test_RulesWithASeparatorMatchThePath);
    runner.Run("first matching rule wins", Test_FirstMatchingRuleWins);
    runner.Run("empty patterns are ignored", Test_EmptyRulePatternIsIgnored);
    runner.Run("plans forward/mirror per destination",
               Test_PlanForwardsAndMirrorsPerDestination);
    runner.Run("missing monitor falls back to primary",
               Test_MissingSecondaryMonitorFallsBackToPrimary);
    runner.Run("destinations name trays", Test_DestinationsNameTrays);
    runner.Run("placement codes read what every version wrote",
               Test_PlacementCodesReadWhatEveryVersionWrote);
    runner.Run("file name extraction", Test_FileNameOfHandlesEveryShape);

    printf("\nmonitors, trays and layout\n");
    runner.Run("every display but the primary gets a tray",
               Test_EveryDisplayButThePrimaryGetsATray);
    runner.Run("one display means no trays unless asked",
               Test_OneDisplayMeansNoTraysUnlessAsked);
    runner.Run("extra trays come after the displays, keep their numbers",
               Test_ExtraTraysComeAfterTheDisplaysAndKeepTheirNumbers);
    runner.Run("a disabled extra tray keeps its number but is not there",
               Test_ADisabledExtraTrayKeepsItsNumberButIsNotThere);
    runner.Run("displays are found by number or as primary",
               Test_DisplaysAreFoundByNumberOrAsPrimary);
    runner.Run("trays are described by where they are",
               Test_TraysAreDescribedByWhereTheyAre);
    runner.Run("a display keeps its tray number while it is unplugged",
               Test_ADisplayKeepsItsTrayNumberWhileItIsUnplugged);
    runner.Run("a display seen for the first time takes the number of one that is away",
               Test_ADisplaySeenForTheFirstTimeTakesTheNumberOfOneThatIsAway);
    runner.Run("a display that becomes the primary gives up its tray",
               Test_ADisplayThatBecomesThePrimaryGivesUpItsTray);
    runner.Run("tray lands inside a negative-coordinate monitor",
               Test_TrayLandsInsideAMonitorAtNegativeCoordinates);
    runner.Run("honours every corner", Test_LayoutHonoursEveryCorner);
    runner.Run("wraps onto more rows", Test_LayoutWrapsOntoMoreRows);
    runner.Run("empty with no icons", Test_LayoutIsEmptyWithNoIcons);
    runner.Run("scales with monitor DPI", Test_LayoutScalesWithMonitorDpi);
    runner.Run("hit test matches the painted grid",
               Test_HitTestMatchesThePaintedGrid);
    runner.Run("a cell's screen rect is where it is painted",
               Test_CellScreenRectIsWhereTheCellIsPainted);
    runner.Run("island bounds scale to the screen", Test_IslandBoundsScaleToTheScreen);

    printf("\nicon identity\n");
    runner.Run("uses GUID when present", Test_IconIdentityUsesGuidWhenPresent);
    runner.Run("falls back to window and id",
               Test_IconIdentityFallsBackToWindowAndId);

    printf("\nstore and routing decisions\n");
    runner.Run("secondary-only is swallowed and mirrored",
               Test_SecondaryOnlyIconIsSwallowedAndMirrored);
    runner.Run("primary-only is forwarded, not mirrored",
               Test_PrimaryOnlyIconIsForwardedAndNotMirrored);
    runner.Run("both appears in each tray",
               Test_BothShowsInTheMirrorAndStaysInThePrimaryTray);
    runner.Run("the mod says where only the icons the shell lacks are",
               Test_TheModAnswersWhereOnlyForIconsTheShellDoesNotHave);
    runner.Run("a rect question by GUID is answered by GUID",
               Test_AnIconAskedAboutByGuidIsFoundByGuid);
    runner.Run("routing is sticky per icon", Test_RoutingIsStickyForTheLifeOfAnIcon);
    runner.Run("partial modify keeps untouched fields",
               Test_PartialModifyKeepsUntouchedFields);
    runner.Run("hidden icons skipped when asked",
               Test_HiddenIconsAreSkippedWhenAsked);
    runner.Run("hidden icons mirrored by default",
               Test_MirrorsHiddenIconsByDefault);
    runner.Run("no mirroring without the monitor",
               Test_NoMirroringWhenTheSecondaryMonitorIsGone);
    runner.Run("delete of an unknown icon is forwarded",
               Test_DeleteOfAnUnknownIconIsForwarded);
    runner.Run("set version recorded, forwarded only where the icon is",
               Test_SetVersionIsRecordedAndOnlyForwardedToAShellThatHasTheIcon);
    runner.Run("fold keeps what a partial modify leaves out",
               Test_FoldKeepsWhatAPartialModifyLeavesOut);
    runner.Run("fold does not keep a balloon", Test_FoldDoesNotKeepABalloon);
    runner.Run("fold applies state through its mask",
               Test_FoldAppliesStateThroughItsMask);
    runner.Run("an add starts the record afresh", Test_AnAddStartsTheRecordAfresh);
    runner.Run("add record draws with the mod's own picture",
               Test_AddRecordDrawsWithTheModsOwnPicture);
    runner.Run("set-version record carries the version",
               Test_SetVersionRecordCarriesTheVersion);
    runner.Run("the store keeps the whole icon, not the last message",
               Test_TheStoreKeepsTheWholeIconNotTheLastMessage);
    runner.Run("putting an icon back replays its version too",
               Test_PuttingAnIconBackReplaysItsVersionToo);
    runner.Run("asks apps to re-register only when Explorer will not",
               Test_OnlyAsksAppsToReRegisterWhenExplorerWillNot);
    runner.Run("the taskbar is this process's only if it owns one",
               Test_TheTaskbarIsThisProcessesOnlyIfItOwnsOne);
    runner.Run("multiple icons per window stay distinct",
               Test_MultipleIconsFromTheSameWindowAreDistinct);
    runner.Run("settings change moves icons between trays",
               Test_SettingsChangeMovesIconsBetweenTrays);

    printf("\nper-icon placement\n");
    runner.Run("key prefers the GUID", Test_PlacementKeyPrefersTheGuid);
    runner.Run("key falls back to file name and id",
               Test_PlacementKeyFallsBackToFileNameAndId);
    runner.Run("a remembered placement beats the rules",
               Test_RememberedPlacementBeatsTheRules);
    runner.Run("placement survives a restart", Test_PlacementSurvivesARestart);
    runner.Run("every placement survives a restart, however many",
               Test_EveryPlacementSurvivesARestartHoweverMany);
    runner.Run("a stored string is read whatever its length",
               Test_AStoredStringIsReadWhateverItsLength);
    runner.Run("what is remembered of an icon not seen for a year is forgotten",
               Test_WhatIsRememberedOfAnIconNotSeenForAYearIsForgotten);
    runner.Run("an upgrade forgets nothing, and starts the year then",
               Test_AnUpgradeForgetsNothingAndStartsTheYearThen);
    runner.Run("an icon is seen when it arrives, and every day it stays",
               Test_AnIconIsSeenWhenItArrivesAndEveryDayItStays);
    runner.Run("moving an icon retracts it from the secondary tray",
               Test_MoveIconToTrayRetractsItFromTheSecondaryTray);

    printf("\nmore than one of Split Tray's trays\n");
    runner.Run("a rule can send an icon to any tray", Test_ARuleCanSendAnIconToAnyTray);
    runner.Run("an empty floating tray still has a handle",
               Test_AnEmptyFloatingTrayStillHasAHandle);
    runner.Run("moving between Split Tray's trays leaves the shell alone",
               Test_MovingBetweenSplitTraysTraysLeavesTheShellAlone);
    runner.Run("an icon whose tray goes away falls back and returns",
               Test_AnIconWhoseTrayGoesAwayFallsBackAndReturns);
    runner.Run("tray numbers are remembered across restarts",
               Test_TrayNumbersAreRememberedAcrossRestarts);
    runner.Run("an icon for a tray that is not there waits in the primary tray",
               Test_AnIconForATrayThatIsNotThereWaitsInThePrimaryTray);
    runner.Run("a disabled tray is not drawn; its icons wait in the primary tray",
               Test_ADisabledTrayIsNotDrawnAndItsIconsWaitInThePrimaryTray);
    runner.Run("an icon its application hid stays hidden after a settings change",
               Test_AnIconItsApplicationHidStaysHiddenAfterASettingsChange);
    runner.Run("every icon has its own serial", Test_EveryIconHasItsOwnSerial);
    runner.Run("forgetting placements gives the rules back",
               Test_ForgettingPlacementsGivesTheRulesBack);

    printf("\nchoosing the overflow\n");
    runner.Run("hidden icons survive a restart", Test_HiddenIconsSurviveARestart);
    runner.Run("every hidden icon survives a restart, however many",
               Test_EveryHiddenIconSurvivesARestartHoweverMany);
    runner.Run("showing every hidden icon clears storage too",
               Test_ShowingEveryHiddenIconClearsStorageToo);
    runner.Run("the user's choice decides before the count",
               Test_TheUsersChoiceDecidesTheOverflowBeforeTheCount);
    runner.Run("arrange window's default move is between the trays",
               Test_ArrangeWindowDefaultMoveIsBetweenTheTrays);

    printf("\ndeciding a tray with enough to decide from\n");
    runner.Run("a modify before the add does not freeze the wrong tray",
               Test_AModifyBeforeTheAddDoesNotFreezeTheWrongTray);
    runner.Run("a settings change keeps icons the user moved",
               Test_ASettingsChangeKeepsIconsTheUserMoved);

    printf("\nicon identity with a GUID\n");
    runner.Run("an empty GUID is not an identity",
               Test_EmptyGuidDoesNotMakeEveryIconTheSameIcon);
    runner.Run("a real GUID identifies across windows",
               Test_ARealGuidStillIdentifiesAnIconAcrossWindows);

    printf("\ndragging along the row\n");
    runner.Run("a shift never removes the dragged cell",
               Test_CellShiftMovesTheCellWithoutEverRemovingIt);
    runner.Run("shift step counts", Test_CellShiftToItsOwnSlotDoesNothing);

    printf("\nfindings from the review of 2026-09-23\n");
    runner.Run("a tooltip update does not reveal an icon its application hid",
               Test_ATooltipUpdateDoesNotRevealAnIconItsApplicationHid);
    runner.Run("an icon re-registered by GUID is clicked in its new window",
               Test_AnIconReRegisteredByGuidIsClickedInItsNewWindow);
    runner.Run("a floating tray draws from its own copy of each icon",
               Test_AFloatingTrayDrawsFromItsOwnCopyOfEachIcon);
    runner.Run("a move takes effect when Explorer is handed it",
               Test_AMoveTakesEffectWhenExplorerIsHandedIt);
    runner.Run("an icon removed before its move is not added back",
               Test_AnIconRemovedBeforeItsMoveIsNotAddedBack);
    runner.Run("a move undone before it is delivered hands Explorer nothing",
               Test_AMoveUndoneBeforeItIsDeliveredHandsExplorerNothing);
    runner.Run("tray callbacks are what Explorer sends",
               Test_TrayCallbacksAreWhatExplorerSends);
    runner.Run("only a known TaskbarHost layout is used",
               Test_OnlyAKnownTaskbarHostLayoutIsUsed);
    runner.Run("on ARM64 the TaskbarHost layout is read from its own instructions",
               Test_OnArm64TheTaskbarHostLayoutIsReadFromItsOwnInstructions);
    runner.Run("SystemTray.dll is resolved only when it is loaded to run",
               Test_SystemTraysSymbolsAreResolvedOnlyWhenItIsLoadedToRun);
    runner.Run("the displays are asked for without holding the lock",
               Test_TheDisplaysAreAskedForWithoutHoldingTheLock);
    runner.Run("a tray that cannot be embedded is looked for less often",
               Test_ATrayThatCannotBeEmbeddedIsLookedForLessOften);
    runner.Run("the arrange window is sized for its display",
               Test_TheArrangeWindowIsSizedForItsDisplay);
    runner.Run("private memory is read without faulting",
               Test_PrivateMemoryIsReadWithoutFaulting);
    runner.Run("a sub-object is found only where it is",
               Test_ASubObjectIsFoundOnlyWhereItIs);
    runner.Run("only a live COM object is called through",
               Test_OnlyALiveComObjectIsCalledThrough);

    printf("\nloading into an Explorer that already has the icons\n");
    runner.Run("an icon that only updates is placed by its program",
               Test_AnIconThatOnlyUpdatesIsPlacedByItsProgram);
    runner.Run("only an icon new to the mod is taken back from Explorer",
               Test_OnlyAnIconNewToTheModIsTakenBackFromExplorer);
    runner.Run("the tree walk anchors on an icon before the frame",
               Test_TheTreeWalkAnchorsOnAnIconBeforeTheFrame);

    printf("\nfindings from the second review of 2026-09-23\n");
    runner.Run("an add carries what arrived while it waited",
               Test_AnAddCarriesWhatArrivedWhileItWaited);
    runner.Run("what Explorer is handed follows the last move",
               Test_WhatExplorerIsHandedFollowsTheLastMove);
    runner.Run("a refused add is asked again, then left to its application",
               Test_AnAddExplorerRefusesIsAskedAgainThenLeftToItsApplication);
    runner.Run("an add refused for an icon Explorer has is recorded as there",
               Test_AnAddRefusedForAnIconExplorerHasIsRecordedAsThere);
    runner.Run("an icon removed while Explorer took it back is taken out again",
               Test_AnIconRemovedWhileExplorerTookItBackIsTakenOutAgain);
    runner.Run("an embedded tray draws from its own copy of each icon",
               Test_AnEmbeddedTrayDrawsFromItsOwnCopyOfEachIcon);

    printf("\nfindings from the third review of 2026-09-24\n");
    runner.Run("every icon is handed back as it is when the mod unloads",
               Test_EveryIconIsHandedBackAsItIsWhenTheModUnloads);
    runner.Run("the hand-back leaves alone an icon Explorer refused its application",
               Test_TheHandBackLeavesAloneAnIconExplorerRefusedItsApplication);
    runner.Run("a hand-back that arrives during a round is done after it",
               Test_AHandBackThatArrivesDuringARoundIsDoneAfterIt);
    runner.Run("a settling round is not started inside another",
               Test_ASettlingRoundIsNotStartedInsideAnother);
    runner.Run("what arrives while Explorer takes an icon back follows it",
               Test_WhatArrivesWhileExplorerTakesAnIconBackFollowsIt);
    runner.Run("a version Explorer does not take is asked for again",
               Test_AVersionExplorerDoesNotTakeIsAskedForAgain);
    runner.Run("Explorer's answer to an application's own message is recorded",
               Test_ExplorersAnswerToAnApplicationsOwnMessageIsRecorded);
    runner.Run("a refused add is asked about with a modify of the same icon",
               Test_ARefusedAddIsAskedAboutWithAModifyOfTheSameIcon);
    runner.Run("an add never carries a picture the mod does not own",
               Test_AnAddNeverCarriesAPictureTheModDoesNotOwn);
    runner.Run("a picture that cannot be copied leaves the one before",
               Test_APictureThatCannotBeCopiedLeavesTheOneBefore);
    runner.Run("the mod attaches only once its tray thread runs",
               Test_TheModAttachesOnlyOnceItsTrayThreadRuns);

    printf("\nfindings from the code audit of 2026-09-24\n");
    runner.Run("unloading waits for a busy taskbar until it is done",
               Test_UnloadingWaitsForABusyTaskbarUntilItIsDone);
    runner.Run("unloading ends only once the mod's code has left the taskbar's thread",
               Test_UnloadingEndsOnlyOnceTheModsCodeHasLeftTheTaskbarsThread);
    runner.Run("an add that is taken starts the icon at version 0",
               Test_AnAddThatIsTakenStartsTheIconAtVersionZero);
    runner.Run("the arrange window's rows follow which icons there are and where",
               Test_TheArrangeWindowsRowsFollowWhichIconsThereAreAndWhere);
    runner.Run("an embedded cell shows the picture the store has",
               Test_AnEmbeddedCellShowsThePictureTheStoreHas);
    runner.Run("an embedded tray's tooltips follow the setting",
               Test_AnEmbeddedTraysTooltipsFollowTheSetting);

    printf("\nworking like Explorer's own tray\n");
    runner.Run("a hidden copy is the whole icon, and a balloon goes on it",
               Test_AHiddenCopyIsTheWholeIconAndABalloonGoesOnIt);
    runner.Run("a balloon for an icon in Split Tray's trays is shown by Explorer",
               Test_ABalloonForAnIconInSplitTraysTraysIsShownByExplorer);
    runner.Run("an icon Explorer holds hidden is shown, not added again",
               Test_AnIconExplorerHoldsHiddenIsShownNotAddedAgain);
    runner.Run("an application's delete and version reach the hidden copy",
               Test_AnApplicationsDeleteAndVersionReachTheHiddenCopy);
    runner.Run("a version 4 icon without NIF_SHOWTIP has its own popup, not a tooltip",
               Test_AVersion4IconWithoutShowTipHasItsOwnPopupNotATooltip);
    runner.Run("an icon is selected from the keyboard as Explorer does",
               Test_AnIconIsSelectedFromTheKeyboardAsExplorerDoes);
    runner.Run("focus is given back to an icon in Split Tray's trays",
               Test_FocusIsGivenBackToAnIconInSplitTraysTrays);
    runner.Run("every cell has a name for screen readers",
               Test_EveryCellHasANameForScreenReaders);
    runner.Run("a cell is drawn again only when its picture changes",
               Test_ACellIsDrawnAgainOnlyWhenItsPictureChanges);

    printf("\ngaps the coverage measurement found\n");
    runner.Run("a floating tray's menu offers every other tray and does what is chosen",
               Test_AFloatingTraysMenuOffersEveryOtherTrayAndDoesWhatIsChosen);

    printf("\nscreen readers and the keyboard in a floating tray\n");
    runner.Run("the keyboard moves across a floating tray's cells",
               Test_TheKeyboardMovesAcrossAFloatingTraysCells);
    runner.Run("a floating tray is a tool bar of named buttons for screen readers",
               Test_AFloatingTrayIsAToolBarOfNamedButtonsForScreenReaders);

    printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
