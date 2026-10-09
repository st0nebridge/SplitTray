#!/usr/bin/env python3
"""Regression tests for tools/check-catalog.py: the rules of Windhawk's catalog
that Split Tray 1.3.1 broke (DECISIONS 94). Each rule is shown failing on the
shape 1.3.1 had and passing on the fixed one, and the mod's own source passes.

Usage:
    python tests/regression/test_check_catalog.py
"""
import importlib.util
import io
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_spec = importlib.util.spec_from_file_location(
    "check_catalog", os.path.join(ROOT, "tools", "check-catalog.py"))
catalog = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(catalog)

HEADER = """// ==WindhawkMod==
// @id              split-tray
// @include         explorer.exe
{architecture}// ==/WindhawkMod==
"""


def header(*architectures):
    return HEADER.format(
        architecture="".join("// @architecture    %s\n" % a for a in architectures))


def names(problems):
    return [message.split(" ")[0] for _, message in problems]


class Architecture(unittest.TestCase):
    def test_none_declared_is_a_problem(self):
        self.assertEqual(len(catalog.architecture_problems(header())), 1)

    def test_x86_64_alone_is_right(self):
        self.assertEqual(catalog.architecture_problems(header("x86-64")), [])

    def test_32_bit_as_well_is_a_problem(self):
        self.assertEqual(len(catalog.architecture_problems(header("x86", "x86-64"))), 1)

    def test_no_metadata_block_is_a_problem(self):
        self.assertEqual(len(catalog.architecture_problems("int x;\n")), 1)


class SymbolTables(unittest.TestCase):
    TABLE = ("WindhawkUtils::SYMBOL_HOOK taskbarDllHooks[] = {\n"
             "    {{LR\"(public: int __cdecl TaskbarHost::FrameHeight(void)const )\"}, &g_f},\n"
             "};\n")

    def test_a_table_under_a_condition_of_the_mods_own_is_a_problem(self):
        src = "#ifndef SPLITTRAY_NO_XAML\n" + self.TABLE + "#endif\n"
        problems = catalog.conditional_symbol_table_problems(src)
        self.assertEqual([line for line, _ in problems], [2])

    def test_a_table_after_the_condition_has_closed_is_fine(self):
        src = "#ifndef SPLITTRAY_NO_XAML\nint x;\n#endif\n" + self.TABLE
        self.assertEqual(catalog.conditional_symbol_table_problems(src), [])

    def test_nested_conditions_are_counted(self):
        src = ("#if A\n#ifdef B\n#endif\n" + self.TABLE + "#endif\n" + self.TABLE)
        problems = catalog.conditional_symbol_table_problems(src)
        self.assertEqual([line for line, _ in problems], [4])

    def test_a_mention_in_a_comment_is_not_a_table(self):
        src = "#if A\n// SYMBOL_HOOK tables are read by the catalog\n#endif\n"
        self.assertEqual(catalog.conditional_symbol_table_problems(src), [])


class XamlGlobals(unittest.TestCase):
    def problems(self, src):
        return names(catalog.xaml_global_problems(src))

    def test_a_xaml_global_without_the_attribute_is_a_problem(self):
        self.assertEqual(self.problems("wux::DispatcherTimer g_timer{nullptr};\n"),
                         ["g_timer"])

    def test_the_attribute_makes_it_fine(self):
        self.assertEqual(self.problems(
            "[[clang::no_destroy]] wux::DispatcherTimer g_timer{nullptr};\n"), [])

    def test_inside_a_namespace_counts_as_global(self):
        self.assertEqual(self.problems(
            "namespace SplitTrayXaml {\nwuxc::Flyout g_flyout{nullptr};\n}\n"),
            ["g_flyout"])

    def test_weak_references_and_tokens_are_safe(self):
        self.assertEqual(self.problems(
            "winrt::weak_ref<wuxc::StackPanel> g_panel;\n"
            "winrt::event_token g_token{};\n"), [])

    def test_a_revoker_holds_on_to_xaml(self):
        self.assertEqual(self.problems(
            "std::list<wux::FrameworkElement::Loaded_revoker> g_revokers;\n"),
            ["g_revokers"])

    def test_a_revoker_named_without_its_namespace_still_does(self):
        self.assertEqual(self.problems("std::list<Loaded_revoker> g_revokers;\n"),
                         ["g_revokers"])

    def test_a_container_of_a_struct_holding_xaml_is_a_problem(self):
        src = ("struct Tray {\n"
               "    HWND wnd = nullptr;\n"
               "    wux::XamlRoot root = nullptr;\n"
               "};\n"
               "std::vector<std::unique_ptr<Tray>> g_trays;\n")
        self.assertEqual(self.problems(src), ["g_trays"])

    def test_a_struct_holding_only_weak_references_is_fine(self):
        src = ("struct Drag {\n"
               "    bool down = false;\n"
               "    winrt::weak_ref<wuxc::StackPanel> panel;\n"
               "};\n"
               "Drag g_drag;\n")
        self.assertEqual(self.problems(src), [])

    def test_a_struct_holding_one_that_holds_xaml_is_a_problem(self):
        src = ("struct Inner {\n    wuxc::Border cell{nullptr};\n};\n"
               "struct Outer {\n    Inner inner;\n};\n"
               "std::map<int, Outer> g_outer;\n")
        self.assertEqual(self.problems(src), ["g_outer"])

    def test_ownership_is_found_whatever_order_the_structs_come_in(self):
        src = ("struct Inner;\n"
               "struct Outer {\n    std::unique_ptr<Inner> inner;\n};\n"
               "struct Inner {\n    wuxc::Border cell{nullptr};\n};\n"
               "std::vector<Outer> g_outer;\n")
        self.assertEqual(self.problems(src), ["g_outer"])

    def test_a_function_local_static_is_a_problem(self):
        src = ("wux::Controls::ControlTemplate FaceTemplate() {\n"
               "    static wux::Controls::ControlTemplate cached{nullptr};\n"
               "    static bool tried = false;\n"
               "    return cached;\n"
               "}\n")
        self.assertEqual(self.problems(src), ["cached"])

    def test_locals_and_members_are_not_globals(self):
        src = ("void Draw() {\n    wuxc::Border cell{nullptr};\n}\n"
               "struct Holder {\n    wuxc::Border cell{nullptr};\n};\n")
        self.assertEqual(self.problems(src), [])

    def test_a_local_is_not_a_global_however_it_is_indented(self):
        src = "void Draw() {\nwuxc::Border cell{nullptr};\n}\n"
        self.assertEqual(self.problems(src), [])

    def test_plain_globals_are_left_alone(self):
        self.assertEqual(self.problems(
            "std::mutex g_mutex;\nstd::atomic<HWND> g_wnd{nullptr};\n"
            "std::map<std::wstring, int, std::less<>> g_seen;\n"), [])

    def test_braces_in_strings_and_comments_do_not_move_the_scope(self):
        src = ("const wchar_t* kA = LR\"(const CTaskBand::`vftable'{for `X'})\";\n"
               "const wchar_t* kB = L\"<Button Content='{TemplateBinding X}'/>\";\n"
               "const wchar_t* kC = LR\"(a \"{\" quoted)\";\n"
               "const wchar_t* kD = L\"{\";\n"
               "const wchar_t kE = L'{';\n"
               "// a { in a comment\n"
               "/* and { in\n   another */\n"
               "wux::DispatcherTimer g_timer{nullptr};\n")
        self.assertEqual(self.problems(src), ["g_timer"])


class Images(unittest.TestCase):
    def readme(self, url):
        return ("// ==WindhawkModReadme==\n/*\n![tray](%s)\n*/\n"
                "// ==/WindhawkModReadme==\n" % url)

    def test_an_image_on_a_branch_is_a_problem(self):
        src = self.readme(
            "https://raw.githubusercontent.com/st0nebridge/SplitTray/main/docs/images/tray-2.png")
        self.assertEqual([line for line, _ in catalog.image_problems(src)], [3])

    def test_an_image_pinned_to_a_tag_is_fine(self):
        src = self.readme(
            "https://raw.githubusercontent.com/st0nebridge/SplitTray/v1.3.1/docs/images/tray-2.png")
        self.assertEqual(catalog.image_problems(src), [])


class TheModItself(unittest.TestCase):
    def test_the_mod_keeps_every_rule(self):
        with io.open(os.path.join(ROOT, "src", "split-tray.wh.cpp"), encoding="utf-8") as f:
            src = f.read()
        self.assertEqual(catalog.check(src), [])


if __name__ == "__main__":
    # On stdout: Windows PowerShell 5.1 can turn a native command's stderr into a
    # terminating error in tools/build.ps1.
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=1))
