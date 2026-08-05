#!/usr/bin/env python3
"""Reject changes that increase a file's count of a legacy pattern a sweep removed.

A sweep that is not enforced regrows. `tasks/Archive/language-hygiene.yaml` T1 and T2
converted 578 `NULL`s and 223 `#ifndef _included_*` guards across the tree; the
strings and enum work is converging on the same kind of zero. Nothing stops the
next change from writing one back, and nobody reviewing a 40-file diff will spot
a single `sprintf`.

The enforcement is per file and per rule: a file may not come out of a change
with more occurrences than it went in with. An absolute rule would fail nearly
every file in the tree, because stages 4 and 5 have not run — so the check would
have to be disabled to get any work done, which is the same as not having it. A
ratchet leaves unconverted files legal until someone converts them, and makes
regrowth impossible in the ones already done.

Counting rather than judging line by line is deliberate; see rising_rules for
why, and for what the approach cannot catch.

    python3 tools/check_hygiene.py                  # against origin/main
    python3 tools/check_hygiene.py --base HEAD~1
    python3 tools/check_hygiene.py --all FILE...    # whole file, for diagnosis

`--all` is a diagnostic, not a gate. Run whole-file over the tree today and it
reports hundreds of findings, because stages 4 and 5 have not run yet — that is
the backlog those plans exist for, not a regression.

One rule is not a modernisation ratchet but a conversion-bug ratchet, and it is
here because this is the file that already knows how to count per-file
occurrences. `while (list[0])` was a legal "is there a first element" test while
these containers were `LList`, whose operator[] and GetData both return nullptr
out of range. `std::vector::operator[]` is undefined there, and in a debug build
it asserts. containers-replaced converted the Eclipse UI toolkit in one batch and
got this wrong twice, in EclWindow's destructor and EclResetDirtyRectangles; both
crashed on the first window with no buttons. Test the container — `while
(!v.empty())` — or iterate it.

Escape hatch: a line carrying the marker `hygiene-ok` in a comment is skipped.
It exists for the genuine exceptions, which do occur — language-hygiene T1 found
two `NULL`s that are typedef'd integers in hand-aligned table terminators
(ControlTypes.cpp, InputDriver.cpp) and left them deliberately. Use it with a
reason on the same line, and expect to justify it in review.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CHECKED_SUFFIXES = {".cpp", ".h", ".inc"}

# The projects the rules apply to. Tests/ is included deliberately: test code is
# code, and a test that reintroduces sprintf is no better than production code
# that does.
CHECKED_ROOTS = (
    "NeuronCore",
    "NeuronClient",
    "NeuronServer",
    "GameLogic",
    "Species",
    "Server",
    "Tests",
)

EXEMPT_MARKER = "hygiene-ok"

# @@ -old,count +new,count @@
HUNK_RE = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


class Rule:
    def __init__(self, name: str, pattern: str, message: str, raw: bool = False):
        self.name = name
        self.regex = re.compile(pattern)
        self.message = message
        # raw rules match the untouched line. Preprocessor directives are not
        # code in the sense strip_noise() means, and stripping would not change
        # them anyway — but being explicit keeps the two kinds apart.
        self.raw = raw


RULES = [
    Rule(
        "NULL",
        r"\bNULL\b",
        "use nullptr — language-hygiene T1 removed 578 of these",
    ),
    Rule(
        "include guard",
        r"^\s*#\s*ifndef\s+_included",
        "use #pragma once — `_lowercase` at global scope is a reserved identifier",
        raw=True,
    ),
    Rule(
        "C string call",
        r"\b(?:strcpy|strncpy|strcat|sprintf|snprintf)\b",
        "use std::string / std::format — see tasks/Archive/strings-modernised.yaml",
    ),
    Rule(
        "subscript as a loop condition",
        r"^\s*while\s*\(\s*!?\s*[\w.>\-]+\[[^\]]*\]\s*\)",
        "test the container, not element [0] — LList returned nullptr out of range and std::vector does not",
        raw=True,
    ),
    Rule(
        "plain enum",
        r"^\s*enum\s+(?!class\b|struct\b)[A-Za-z_]",
        "use enum class — an unscoped enum converts to int silently",
        raw=True,
    ),
]


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=REPO_ROOT, capture_output=True, text=True, check=True
    ).stdout


def strip_noise(line: str) -> str:
    """Blank out string literals and comments so they cannot trip a rule.

    False positives are what kills a check like this: the first time it rejects
    a comment that says "was NULL before", someone adds `# noqa` reflexively to
    everything and it stops meaning anything. layering-inversion T13 recorded the
    same lesson from the other side — a plain substring search matched
    "dialog_apply" while hunting for `g_app` and hid five files.

    Single-line only. A rule pattern inside a multi-line /* */ block is still
    matched, which is a known and accepted false positive; `hygiene-ok` covers it
    on the rare occasions it happens.
    """
    out = []
    index = 0
    length = len(line)
    while index < length:
        char = line[index]
        if char == "/" and index + 1 < length and line[index + 1] == "/":
            break
        if char == "/" and index + 1 < length and line[index + 1] == "*":
            end = line.find("*/", index + 2)
            if end == -1:
                break
            out.append(" " * (end + 2 - index))
            index = end + 2
            continue
        if char in "\"'":
            quote = char
            index += 1
            out.append(" ")
            while index < length:
                if line[index] == "\\":
                    index += 2
                    out.append("  ")
                    continue
                if line[index] == quote:
                    break
                index += 1
                out.append(" ")
            index += 1
            out.append(" ")
            continue
        out.append(char)
        index += 1
    return "".join(out)


def is_checked(path: Path) -> bool:
    if path.suffix not in CHECKED_SUFFIXES:
        return False
    parts = path.as_posix().split("/")
    return bool(parts) and parts[0] in CHECKED_ROOTS


def violations(path: Path, numbered: list[tuple[int, str]], rules: list[Rule]) -> list[str]:
    """Report the changed lines matching `rules`, which have already been shown
    to have risen for this file. Nothing is reported for a rule whose count held
    steady, however many of its call sites the change happened to touch."""
    found = []
    for number, line in numbered:
        if EXEMPT_MARKER in line:
            continue
        stripped = strip_noise(line)
        for rule in rules:
            target = line if rule.raw else stripped
            if rule.regex.search(target):
                found.append(f"{path}:{number}: {rule.name} — {rule.message}\n    {line.strip()}")
    return found


def count_matches(lines: list[str], rule: Rule) -> int:
    """How many times `rule` fires across `lines`, ignoring exempted ones."""
    total = 0
    for line in lines:
        if EXEMPT_MARKER in line:
            continue
        target = line if rule.raw else strip_noise(line)
        total += len(rule.regex.findall(target))
    return total


_MOVES: dict[str, dict[str, Path]] = {}


def moves(base: str) -> dict[str, Path]:
    """destination -> source, for every file this change appears to have moved.

    Without this a `git mv` reads as a brand new file: the base content is
    empty, so every occurrence of every rule counts as one this change
    introduced. layering-inversion T15 moved 32 files at once and the check
    reported 33 findings against code it had not touched a character of. The
    honest alternative would have been 33 `hygiene-ok` markers on lines that
    are not exceptions to anything, which is precisely how a ratchet stops
    meaning what it says.

    Two sources, because git's own answer is not enough on its own. Git pairs
    a delete with an add by content similarity, and a whole-file clang-format
    reformat rewrites every line: T15 moved Species/EntityGrid.cpp unchanged
    and git scored it 16% similar to its base version, because a reformat
    commit sat in between. Lowering --find-renames far enough to catch that
    would pair unrelated files across the tree.

    So a delete and an add sharing a basename are treated as a move when the
    basename is unambiguous — exactly one of each. That is what a directory
    move looks like, it is immune to any amount of rewriting, and the failure
    it can produce is bounded: at worst a genuinely new file is compared
    against an unrelated namesake that vanished in the same change, and the
    ratchet reads a wrong-but-nonzero baseline rather than an empty one.
    """
    if base in _MOVES:
        return _MOVES[base]

    status = git("diff", "--name-status", "--find-renames", base, "--", "*.cpp", "*.h", "*.inc")
    found: dict[str, Path] = {}
    added: dict[str, list[str]] = {}
    deleted: dict[str, list[str]] = {}
    for line in status.splitlines():
        fields = line.split("\t")
        if fields[0].startswith("R") and len(fields) == 3:
            found[fields[2]] = Path(fields[1])
        elif fields[0] == "A" and len(fields) == 2:
            added.setdefault(Path(fields[1]).name, []).append(fields[1])
        elif fields[0] == "D" and len(fields) == 2:
            deleted.setdefault(Path(fields[1]).name, []).append(fields[1])

    for name, destinations in added.items():
        sources = deleted.get(name, [])
        if len(destinations) == 1 and len(sources) == 1:
            found.setdefault(destinations[0], Path(sources[0]))

    _MOVES[base] = found
    return found


def base_lines(base: str, path: Path) -> list[str]:
    """The file as of `base`, following a rename, or empty for a genuinely new file."""
    try:
        return git("show", f"{base}:{path.as_posix()}").splitlines()
    except subprocess.CalledProcessError:
        pass

    source = moves(base).get(path.as_posix())
    if source is None:
        return []
    try:
        return git("show", f"{base}:{source.as_posix()}").splitlines()
    except subprocess.CalledProcessError:
        return []


def rising_rules(base: str, path: Path, current: list[str]) -> list[Rule]:
    """The rules whose count in `path` went UP against `base`.

    Counting per file rather than judging line by line is what makes this
    survive reformatting, and the reason is not theoretical. clang-format
    reflows: it reindents a changed line's neighbours, and it joins and splits
    statements. A nine-line sprintf call collapsed into two lines is the same
    call, but no line-based comparison can see that — it looks like a brand new
    one. The first version of this check reported exactly that, and the only
    way to silence it would have been a `hygiene-ok` marker that then hides a
    real call from the conversion task obliged to remove it.

    A count cannot be fooled by moving, reindenting, joining or splitting, and
    it states the property the ratchet actually wants: a file may not come out
    of a change with more of these than it went in with.

    The hole this leaves, stated plainly because it is wider than it first
    looks: a file whose count for a rule FALLS can absorb a new occurrence of
    that same rule without tripping. Convert ten sprintf calls and write one
    back, and the count still went down, so nothing is reported. That is only
    reachable in a file being actively converted in the same commit — which is
    the one moment a reviewer is already reading those lines — and the
    alternative is a line-based rule that cries wolf every time clang-format
    reflows a statement. A check that is wrong often enough to be suppressed
    protects nothing.

    It also reports the rule rather than the specific occurrence when a file
    both adds and removes, for the same reason: it knows the count moved, not
    which line is to blame.
    """
    original = base_lines(base, path)
    return [rule for rule in RULES if count_matches(current, rule) > count_matches(original, rule)]


def added_lines(base: str) -> dict[Path, list[tuple[int, str]]]:
    """path -> [(line number, text)] for lines this change added or rewrote.

    Untracked files count as entirely added, matching check_format.py: a new file
    that sails through locally and fails in CI on the first commit is the wrong
    way round for a pre-push check.
    """
    diff = git("diff", "-U0", "--diff-filter=d", base, "--", "*.cpp", "*.h", "*.inc")

    added: dict[Path, list[tuple[int, str]]] = {}
    current: Path | None = None
    number = 0
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = Path(line[6:])
            continue
        if line.startswith("--- "):
            continue
        match = HUNK_RE.match(line)
        if match:
            number = int(match.group(1))
            continue
        if current is not None and line.startswith("+"):
            added.setdefault(current, []).append((number, line[1:]))
            number += 1

    for name in git("ls-files", "--others", "--exclude-standard").splitlines():
        path = Path(name)
        if path.suffix not in CHECKED_SUFFIXES:
            continue
        text = (REPO_ROOT / path).read_text(encoding="utf-8", errors="replace")
        added[path] = list(enumerate(text.splitlines(), start=1))

    return added


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/main", help="ref to diff against")
    parser.add_argument(
        "--all",
        nargs="*",
        metavar="FILE",
        help="scan whole files rather than changed lines (diagnostic, not a gate)",
    )
    args = parser.parse_args()

    if args.all is not None:
        targets = {}
        for name in args.all:
            path = Path(name)
            text = (REPO_ROOT / path).read_text(encoding="utf-8", errors="replace")
            targets[path] = list(enumerate(text.splitlines(), start=1))
    else:
        try:
            base = git("merge-base", "HEAD", args.base).strip()
        except subprocess.CalledProcessError:
            base = args.base
        targets = added_lines(base)

    failures: list[str] = []
    checked = 0
    for path, numbered in sorted(targets.items()):
        # --all is explicit about its targets; the root filter is for diffs.
        if args.all is None and not is_checked(path):
            continue
        checked += 1
        if args.all is not None:
            rules = RULES
        else:
            rules = rising_rules(base, path, [line for _, line in numbered] if not (REPO_ROOT / path).exists()
                                 else (REPO_ROOT / path).read_text(encoding="utf-8", errors="replace").splitlines())
            if not rules:
                continue
        failures.extend(violations(path, numbered, rules))

    if failures:
        print("Changed lines reintroduce patterns the modernisation removed:\n")
        for failure in failures:
            print(failure)
        print(
            f"\n{len(failures)} finding(s). These are ratchets, not style preferences — "
            "see tasks/Archive/language-hygiene.yaml.\n"
            f"A genuine exception is marked with `{EXEMPT_MARKER}` in a comment on the line, "
            "with a reason."
        )
        return 1

    print(f"Hygiene OK ({checked} changed file(s) checked).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
