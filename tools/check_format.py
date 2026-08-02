#!/usr/bin/env python3
"""Check clang-format compliance on changed lines only.

Most of the tree is unconverted Darwinia code that does not match .clang-format.
Reformatting it wholesale would produce a six-figure diff and destroy `git blame`
for the entire project, so formatting is enforced only on the lines a change
actually touches. The tree converges on the target style as code is worked on.

    python3 tools/check_format.py                   # against origin/main
    python3 tools/check_format.py --base HEAD~1
    python3 tools/check_format.py --fix             # rewrite the files in place
    python3 tools/check_format.py --all FILE...     # whole-file, for migrations
    python3 tools/check_format.py --rename Old=New  # identifier rename, repeatable

A pure identifier rename is the awkward case. It touches every line mentioning
the old name — often thousands of legacy lines — but authors none of them. Making
those lines pass .clang-format would either reformat fragments of files that are
otherwise untouched (leaving them half-converted, which CODING_STANDARDS.md
forbids) or balloon a mechanical rename into a tree-wide reformat.

`--rename Old=New` resolves that: a changed line is skipped when reversing the
rename reproduces a line that was already in the base revision. Lines the change
actually wrote are still checked, so a rename cannot smuggle in unformatted code.

CI cannot be told about a rename on the command line, so renames are also read
from `Rename: Old=New` trailers in the commit messages being checked. Declaring
it in the commit that performs it keeps the claim next to the diff that proves
it, where a reviewer will see it.
"""
from __future__ import annotations

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FORMATTED_SUFFIXES = {".cpp", ".h", ".inc"}

# @@ -old,count +new,count @@
HUNK_RE = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=REPO_ROOT, capture_output=True, text=True, check=True
    ).stdout


def clang_format_binary() -> str:
    for candidate in ("clang-format", "clang-format-20", "clang-format-19", "clang-format-18"):
        try:
            subprocess.run([candidate, "--version"], capture_output=True, check=True)
            return candidate
        except (OSError, subprocess.CalledProcessError):
            continue
    sys.exit("clang-format not found on PATH")


def changed_line_ranges(base: str) -> dict[Path, list[tuple[int, int]]]:
    """path -> list of (first, last) line ranges added or modified since `base`."""
    diff = git("diff", "-U0", "--diff-filter=d", base, "--", "*.cpp", "*.h", "*.inc")

    ranges: dict[Path, list[tuple[int, int]]] = {}
    current: Path | None = None
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = Path(line[6:])
            continue
        if current is None:
            continue
        match = HUNK_RE.match(line)
        if not match:
            continue
        start = int(match.group(1))
        count = int(match.group(2)) if match.group(2) is not None else 1
        if count:  # count == 0 means the hunk only deleted lines
            ranges.setdefault(current, []).append((start, start + count - 1))
    return ranges


RENAME_TRAILER_RE = re.compile(r"^\s*Rename:\s*(\S+)=(\S+)\s*$", re.M)


def declared_renames(base: str) -> list[tuple[str, str]]:
    """Renames declared as `Rename: Old=New` trailers in the commits since `base`."""
    try:
        log = git("log", "--format=%B", f"{base}..HEAD")
    except subprocess.CalledProcessError:
        return []
    return RENAME_TRAILER_RE.findall(log)


def renamed_paths(base: str) -> dict[str, str]:
    """new path -> old path, for files git detects as renamed since `base`.

    A renamed file has no content at its new path in the base revision, so its
    lines would all look newly authored without this.
    """
    output = git("diff", "-M", "--diff-filter=R", "--name-status", base)
    mapping = {}
    for line in output.splitlines():
        parts = line.split("\t")
        if len(parts) == 3 and parts[0].startswith("R"):
            _, old, new = parts
            mapping[new] = old
    return mapping


def drop_rename_only_lines(
    base: str,
    path: Path,
    line_ranges: list[tuple[int, int]],
    renames: list[tuple[str, str]],
    moved: dict[str, str],
) -> list[tuple[int, int]]:
    """Remove lines whose only difference from `base` is one of the renames."""
    base_path = moved.get(path.as_posix(), path.as_posix())
    try:
        original = git("show", f"{base}:{base_path}").splitlines()
    except subprocess.CalledProcessError:
        return line_ranges  # genuinely new file — nothing was renamed into it
    before = set(original)

    current = (REPO_ROOT / path).read_text(encoding="utf-8", errors="replace").splitlines()
    authored: list[int] = []
    for start, end in line_ranges:
        for number in range(start, end + 1):
            if number > len(current):
                continue
            line = current[number - 1]
            reverted = line
            for old, new in renames:
                reverted = reverted.replace(new, old)
            if reverted != line and reverted in before:
                continue  # pure rename of a pre-existing line
            authored.append(number)

    # Re-collapse the surviving line numbers into ranges.
    collapsed: list[tuple[int, int]] = []
    for number in sorted(authored):
        if collapsed and number == collapsed[-1][1] + 1:
            collapsed[-1] = (collapsed[-1][0], number)
        else:
            collapsed.append((number, number))
    return collapsed


def format_file(binary: str, path: Path, line_ranges: list[tuple[int, int]] | None) -> str:
    command = [binary, f"--assume-filename={path}"]
    for start, end in line_ranges or []:
        command.append(f"--lines={start}:{end}")
    source = (REPO_ROOT / path).read_bytes()
    result = subprocess.run(command, input=source, capture_output=True, check=True)
    return result.stdout.decode("utf-8", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/main", help="ref to diff against")
    parser.add_argument("--fix", action="store_true", help="rewrite files instead of reporting")
    parser.add_argument(
        "--rename",
        action="append",
        default=[],
        metavar="OLD=NEW",
        help="treat lines that differ only by this identifier rename as unchanged",
    )
    parser.add_argument(
        "--all",
        nargs="*",
        metavar="FILE",
        help="format whole files rather than changed lines (use for migration commits)",
    )
    args = parser.parse_args()

    binary = clang_format_binary()

    if args.all is not None:
        targets = {Path(f): None for f in args.all}
    else:
        try:
            base = git("merge-base", "HEAD", args.base).strip()
        except subprocess.CalledProcessError:
            base = args.base
        targets = dict(changed_line_ranges(base))
        renames = [tuple(r.split("=", 1)) for r in args.rename]
        renames += declared_renames(base)
        if renames:
            moved = renamed_paths(base)
            targets = {
                path: kept
                for path, ranges in targets.items()
                if (kept := drop_rename_only_lines(base, path, ranges, renames, moved))
            }

    failures: list[str] = []
    for path, line_ranges in targets.items():
        if path.suffix not in FORMATTED_SUFFIXES or not (REPO_ROOT / path).exists():
            continue
        original = (REPO_ROOT / path).read_text(encoding="utf-8", errors="replace")
        formatted = format_file(binary, path, line_ranges)
        if original == formatted:
            continue
        if args.fix:
            (REPO_ROOT / path).write_text(formatted, encoding="utf-8", newline="")
            print(f"formatted {path}")
            continue
        failures.append(
            "".join(
                difflib.unified_diff(
                    original.splitlines(keepends=True),
                    formatted.splitlines(keepends=True),
                    fromfile=f"a/{path}",
                    tofile=f"b/{path}",
                )
            )
        )

    if failures:
        print("Changed lines do not match .clang-format:\n")
        print("\n".join(failures))
        print("Fix with:  python3 tools/check_format.py --fix")
        return 1

    print(f"Formatting OK ({len(targets)} changed file(s) checked).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
