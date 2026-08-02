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

Exits 1 if any changed line is misformatted, printing the diff that would fix it.
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
