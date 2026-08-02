#!/usr/bin/env python3
"""Reject changed lines that reintroduce a legacy pattern a sweep already removed.

A sweep that is not enforced regrows. `tasks/language-hygiene.yaml` T1 and T2
converted 578 `NULL`s and 223 `#ifndef _included_*` guards across the tree; the
strings and enum work is converging on the same kind of zero. Nothing stops the
next change from writing one back, and nobody reviewing a 40-file diff will spot
a single `sprintf`.

The enforcement is deliberately the same shape as check_format.py: **changed
lines only**. Most of the tree is unconverted Darwinia code, and a whole-file
rule would fail every file that has not had its conversion task yet — so the
check would have to be disabled to get any work done, which is the same as not
having it. Checking what a change *writes* means the ratchet only ever turns one
way, and an unconverted file stays legal until someone converts it.

    python3 tools/check_hygiene.py                  # against origin/main
    python3 tools/check_hygiene.py --base HEAD~1
    python3 tools/check_hygiene.py --all FILE...    # whole file, for diagnosis

`--all` is a diagnostic, not a gate. Run whole-file over the tree today and it
reports hundreds of findings, because stages 4 and 5 have not run yet — that is
the backlog those plans exist for, not a regression.

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
        "use std::string / std::format — see tasks/strings-modernised.yaml",
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


def violations(path: Path, numbered: list[tuple[int, str]]) -> list[str]:
    found = []
    for number, line in numbered:
        if EXEMPT_MARKER in line:
            continue
        stripped = strip_noise(line)
        for rule in RULES:
            target = line if rule.raw else stripped
            if rule.regex.search(target):
                found.append(f"{path}:{number}: {rule.name} — {rule.message}\n    {line.strip()}")
    return found


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
        failures.extend(violations(path, numbered))

    if failures:
        print("Changed lines reintroduce patterns the modernisation removed:\n")
        for failure in failures:
            print(failure)
        print(
            f"\n{len(failures)} finding(s). These are ratchets, not style preferences — "
            "see tasks/language-hygiene.yaml.\n"
            f"A genuine exception is marked with `{EXEMPT_MARKER}` in a comment on the line, "
            "with a reason."
        )
        return 1

    print(f"Hygiene OK ({checked} changed file(s) checked).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
