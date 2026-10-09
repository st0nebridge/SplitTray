#!/usr/bin/env python3
"""Check the mod against the rules of Windhawk's catalog that a local build does
not otherwise see until a pull request to the catalog (DECISIONS 94).

1. It declares `@architecture x86-64` and nothing else. Split Tray needs
   Windows 11, which is 64-bit only, and without the line the catalog also
   builds it for 32-bit x86, where it does not compile.
2. No table of symbol hooks sits under a preprocessor condition. The catalog
   reads the tables from the source to cache the symbols for users, and it
   cannot evaluate a condition of the mod's own, such as SPLITTRAY_NO_XAML.
3. Every global or static that holds XAML or another WinRT object carries
   [[clang::no_destroy]]. When Explorer exits, the C++ runtime would destroy
   it on whichever thread is exiting, after XAML has gone: a crash or a hang at
   sign-out. A weak reference or an event token is safe, so it is left alone.
4. The readme's images are pinned to a tag or a commit, not a branch. The
   catalog keeps a copy of each image the first time it sees it, and flags a
   later change to the picture at the same address.

Usage:
    python tools/check-catalog.py
Exits non-zero on a problem.
"""
import io
import os
import re
import sys

MOD = os.path.join("src", "split-tray.wh.cpp")

# A WinRT type named through the namespaces the mod uses.
WINRT_TYPE = re.compile(r"\b(?:winrt|wf|wux|wuxc|wuxm|wuxmi)::")
# Event revokers hold the element weakly but remove the handler when destroyed,
# which is a call into XAML.
REVOKER = re.compile(r"_revoker\b")
# Safe to destroy anywhere: weak_ref only touches an in-process control block,
# and an event token is plain data.
SAFE_WINRT = re.compile(r"winrt::weak_ref<|winrt::event_token\b")

DECLARATION = re.compile(
    r"^(?P<attr>\[\[clang::no_destroy\]\]\s+)?"
    r"(?P<static>static\s+)?"
    r"(?P<type>[A-Za-z_][\w:<>,\s\*&]*?)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*"
    r"(?:\{[^;]*\}|=[^;]*)?;\s*(?://.*)?$"
)
NOT_A_TYPE = re.compile(
    r"^(?:return|using|namespace|typedef|constexpr|enum|struct|class|goto|"
    r"delete|throw|case|else|co_return)\b"
)


def line_of(src, pos):
    return src.count("\n", 0, pos) + 1


def mask_literals(src):
    """The source with comments, string and character literals blanked out,
    newlines kept, so that braces and keywords in them are not counted."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif re.match(r'(?:L|u8|u|U)?R"', src[i:i + 4]) and (
            i == 0 or not (src[i - 1].isalnum() or src[i - 1] == "_")
        ):
            quote = src.index('"', i)
            paren = src.index("(", quote)
            closing = ")" + src[quote + 1:paren] + '"'
            j = src.find(closing, paren)
            j = n if j < 0 else j + len(closing)
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def metadata_values(src, key):
    block = re.search(r"// ==WindhawkMod==\n(.*?)// ==/WindhawkMod==", src, re.S)
    if not block:
        return None
    return re.findall(r"^// @" + key + r"[ \t]+(.+?)[ \t]*$", block.group(1), re.M)


def architecture_problems(src):
    values = metadata_values(src, "architecture")
    if values is None:
        return [(1, "no ==WindhawkMod== block")]
    if values != ["x86-64"]:
        return [(1, "@architecture must be exactly x86-64, found %s" % (values or "none"))]
    return []


def conditional_symbol_table_problems(src):
    problems = []
    masked = mask_literals(src)
    depth = 0
    for number, line in enumerate(masked.split("\n"), start=1):
        directive = re.match(r"\s*#\s*(\w+)", line)
        if directive:
            word = directive.group(1)
            if word in ("if", "ifdef", "ifndef"):
                depth += 1
            elif word == "endif":
                depth = max(0, depth - 1)
            continue
        if depth and re.search(r"\bSYMBOL_HOOK\b", line):
            problems.append(
                (number, "a table of symbol hooks under a preprocessor condition: "
                         "the catalog cannot read it"))
    return problems


def holds_winrt(type_text, owning):
    stripped = type_text
    # Drop each weak_ref<...> whole, nested template arguments and all.
    while True:
        match = SAFE_WINRT.search(stripped)
        if not match:
            break
        if match.group(0).endswith("<"):
            depth, j = 1, match.end()
            while j < len(stripped) and depth:
                depth += {"<": 1, ">": -1}.get(stripped[j], 0)
                j += 1
            stripped = stripped[:match.start()] + stripped[j:]
        else:
            stripped = stripped[:match.start()] + stripped[match.end():]
    if WINRT_TYPE.search(stripped) or REVOKER.search(stripped):
        return True
    return any(re.search(r"\b" + re.escape(name) + r"\b", stripped) for name in owning)


def scopes(masked):
    """For each line: whether it starts at namespace scope (no function, class
    or other brace open around it), and the name of the struct it is directly
    inside, if any."""
    stack = []  # one entry per open brace: "ns", ("struct", name) or "other"
    statement = []
    result = []
    for line in masked.split("\n"):
        at_namespace = all(kind == "ns" for kind in stack)
        inside = stack[-1][1] if stack and isinstance(stack[-1], tuple) else None
        result.append((at_namespace, inside))
        if re.match(r"\s*#", line):
            continue
        for c in line:
            if c == "{":
                head = "".join(statement)
                if re.search(r"\bnamespace\b[\w:\s]*$", head) or re.search(r'\bextern\s*$', head):
                    stack.append("ns")
                else:
                    struct = re.search(r"\b(?:struct|class)\s+(\w+)[^;{()]*$", head)
                    stack.append(("struct", struct.group(1)) if struct else "other")
                statement = []
            elif c == "}":
                if stack:
                    stack.pop()
                statement = []
            elif c == ";":
                statement = []
            else:
                statement.append(c)
        statement.append(" ")
    return result


def xaml_global_problems(src):
    masked = mask_literals(src)
    lines = masked.split("\n")
    where = scopes(masked)

    # Members of each struct, to know which structs hold WinRT objects by value.
    members = {}
    for (at_namespace, inside), line in zip(where, lines):
        match = DECLARATION.match(line.strip())
        if inside and match and not NOT_A_TYPE.match(line.strip()):
            members.setdefault(inside, []).append(match.group("type"))
    owning = set()
    changed = True
    while changed:
        changed = False
        for name, types in members.items():
            if name not in owning and any(holds_winrt(t, owning) for t in types):
                owning.add(name)
                changed = True

    problems = []
    for number, ((at_namespace, inside), line) in enumerate(zip(where, lines), start=1):
        text = line.strip()
        match = DECLARATION.match(text)
        if not match or NOT_A_TYPE.match(text):
            continue
        is_global = at_namespace and not match.group("static") and line[:1] not in (" ", "\t")
        is_static = bool(match.group("static")) and not inside
        if not (is_global or is_static):
            continue
        if holds_winrt(match.group("type"), owning) and not match.group("attr"):
            problems.append(
                (number, "%s holds XAML or another WinRT object and is destroyed by the "
                         "runtime at Explorer's exit: give it [[clang::no_destroy]] and "
                         "release it on unload" % match.group("name")))
    return problems


def image_problems(src):
    readme = re.search(r"// ==WindhawkModReadme==\n(.*?)// ==/WindhawkModReadme==", src, re.S)
    if not readme:
        return []
    problems = []
    for match in re.finditer(r"!\[[^\]]*\]\(\s*([^)\s]+)", readme.group(1)):
        url = match.group(1)
        branch = re.match(
            r"https://raw\.githubusercontent\.com/[^/]+/[^/]+/(?:refs/heads/)?(main|master|HEAD)/",
            url)
        if branch:
            problems.append(
                (line_of(src, readme.start(1) + match.start(1)),
                 "readme image on the %s branch: pin it to a tag or a commit, since "
                 "the catalog keeps the first copy it sees" % branch.group(1)))
    return problems


def check(src):
    problems = []
    for rule in (architecture_problems, conditional_symbol_table_problems,
                 xaml_global_problems, image_problems):
        problems.extend(rule(src))
    return sorted(problems)


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo_root)
    with io.open(MOD, encoding="utf-8") as f:
        src = f.read()
    problems = check(src)
    for number, message in problems:
        print("%s:%d: %s" % (MOD, number, message))
    if problems:
        print("FAIL: %d catalog rule(s) broken" % len(problems))
        return 1
    print("    clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
