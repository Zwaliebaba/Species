#!/usr/bin/env python3
"""Check that no project includes a header from a layer above it.

The intended dependency direction is described in docs/ARCHITECTURE.md:

    NeuronCore                      shared foundation, no dependencies
      NeuronClient                  presentation: render, sound, input, UI
      NeuronServer                  authoritative simulation host
        GameLogic                   entities, buildings, teams
          Species                   client executable
          Server                    server executable

An include is a violation when the header it names is owned by a project the
including project is not allowed to depend on. Because every project puts the
others on its include path and all includes are written as bare basenames
(`#include "App.h"`), the owning project is resolved by looking the basename up
against the files actually on disk.

The tree still carries violations inherited from the original single-binary
Darwinia layout. Those are recorded in tools/layering_allowlist.txt so the check
fails only on *new* ones; the allowlist is expected to shrink to empty as the
modernization proceeds, never to grow.

    python3 tools/check_layering.py             # fail on unlisted violations
    python3 tools/check_layering.py --update    # rewrite the allowlist
    python3 tools/check_layering.py --rename OLD NEW   # a file moved or was renamed
    python3 tools/check_layering.py --prune            # drop entries that are now fixed

Renaming or moving a file makes its allowlisted entries stop matching, and the
same violations then look brand new. `--rename` rewrites just those entries, so
the change shows up in the diff as the rename it is rather than being absorbed
silently by `--update`. It refuses to invent entries that did not already exist.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
ALLOWLIST_PATH = Path(__file__).resolve().parent / "layering_allowlist.txt"

# project -> projects it is allowed to include from (transitively closed below)
DIRECT_DEPENDENCIES: dict[str, set[str]] = {
    "NeuronCore": set(),
    "NeuronClient": {"NeuronCore"},
    "NeuronServer": {"NeuronCore"},
    "GameLogic": {"NeuronCore", "NeuronClient", "NeuronServer"},
    "Species": {"GameLogic", "NeuronClient", "NeuronCore"},
    "Server": {"GameLogic", "NeuronServer", "NeuronCore"},
}

# project -> its directory relative to the repo root. Test projects live under
# Tests/; everything else is named after its directory.
PROJECT_DIRS: dict[str, str] = {project: project for project in DIRECT_DEPENDENCIES}


def add_test_projects() -> None:
    """Give every Tests/<Name>Tests directory a node above the library it covers.

    Derived from the tree rather than listed, so a newly added test project is
    covered the moment it exists instead of the moment somebody remembers to
    edit this file.

    <Name>Tests depends on <Name> and, transitively, on whatever <Name> may
    depend on. That is the whole rule: **a test may reach no further up than the
    code it covers already does.** Without it the suite becomes a second place
    upward dependencies can accumulate unnoticed — and a worse one, because a
    test reaching into Species to set up a fixture looks like diligence rather
    than like the layering violation it is.
    """
    tests_root = REPO_ROOT / "Tests"
    if not tests_root.is_dir():
        return
    for path in sorted(tests_root.iterdir()):
        library = path.name.removesuffix("Tests")
        if not path.is_dir() or library == path.name or library not in DIRECT_DEPENDENCIES:
            continue
        DIRECT_DEPENDENCIES[path.name] = {library}
        PROJECT_DIRS[path.name] = f"Tests/{path.name}"


add_test_projects()

SOURCE_SUFFIXES = {".cpp", ".h", ".inc"}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def allowed_dependencies(project: str) -> set[str]:
    """DIRECT_DEPENDENCIES closed over itself, plus the project itself."""
    seen = {project}
    pending = list(DIRECT_DEPENDENCIES[project])
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        pending.extend(DIRECT_DEPENDENCIES[current])
    return seen


def build_header_index() -> dict[str, str]:
    """basename -> owning project, for every header in the tree.

    pch.h exists once per project and always resolves to the including project's
    own copy, so it is excluded here and handled at the call site.
    """
    index: dict[str, str] = {}
    for project, directory in PROJECT_DIRS.items():
        for path in (REPO_ROOT / directory).iterdir():
            if path.suffix in {".h", ".inc"} and path.name != "pch.h":
                index[path.name] = project
    return index


def load_allowlist() -> set[str]:
    if not ALLOWLIST_PATH.exists():
        return set()
    return {
        line.strip()
        for line in ALLOWLIST_PATH.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    }


def write_allowlist(entries: set[str]) -> None:
    header = (
        "# Pre-existing layering violations, inherited from the original\n"
        "# single-binary Darwinia layout. tools/check_layering.py fails on any\n"
        "# violation that is NOT listed here.\n"
        "#\n"
        "# This file may only shrink. Adding a line means a new upward dependency\n"
        "# was introduced, which is exactly what the check exists to prevent.\n"
        "#\n"
        "# Format: <including project>/<including file> -> <included header>\n"
        "#\n"
        "# Regenerate with: python3 tools/check_layering.py --update\n\n"
    )
    ALLOWLIST_PATH.write_text(header + "\n".join(sorted(entries)) + "\n", encoding="utf-8")


def find_violations(header_index: dict[str, str]) -> list[str]:
    violations: list[str] = []
    for project in sorted(DIRECT_DEPENDENCIES):
        permitted = allowed_dependencies(project)
        directory = PROJECT_DIRS[project]
        for path in sorted((REPO_ROOT / directory).iterdir()):
            if path.suffix not in SOURCE_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for included in INCLUDE_RE.findall(text):
                name = Path(included).name
                owner = header_index.get(name)
                if owner is not None and owner not in permitted:
                    violations.append(f"{directory}/{path.name} -> {name}")
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--update",
        action="store_true",
        help="rewrite the allowlist from the current tree instead of checking",
    )
    parser.add_argument(
        "--prune",
        action="store_true",
        help="remove allowlist entries whose violations no longer exist; refuses to add any",
    )
    parser.add_argument(
        "--rename",
        nargs=2,
        metavar=("OLD", "NEW"),
        help="rewrite allowlist entries after a file was renamed or moved, "
        "e.g. --rename GameLogic/Foo.cpp GameLogic/Bar.cpp",
    )
    args = parser.parse_args()

    if args.rename:
        old_path, new_path = args.rename
        allowlist = load_allowlist()
        moved = {e for e in allowlist if e.split(" -> ")[0] == old_path}
        if not moved:
            print(f"no allowlist entries for '{old_path}' — nothing to rename")
            return 1
        allowlist -= moved
        allowlist |= {e.replace(old_path, new_path, 1) for e in moved}
        write_allowlist(allowlist)
        print(f"renamed {len(moved)} entr(y/ies): {old_path} -> {new_path}")
        return 0

    violations = set(find_violations(build_header_index()))

    if args.prune:
        allowlist = load_allowlist()
        added = violations - allowlist
        if added:
            print("Refusing to prune: these violations are new, not fixed.\n")
            for entry in sorted(added):
                print(f"  {entry}")
            print("\nPrune only shrinks. Resolve these first.")
            return 1
        removed = allowlist - violations
        write_allowlist(allowlist & violations)
        print(f"pruned {len(removed)} fixed entr(y/ies); {len(violations)} remain")
        return 0

    if args.update:
        write_allowlist(violations)
        print(f"wrote {len(violations)} entries to {ALLOWLIST_PATH.relative_to(REPO_ROOT)}")
        return 0

    allowlist = load_allowlist()

    new = sorted(violations - allowlist)
    if new:
        print("New layering violations (an include reaching into a higher layer):\n")
        for entry in new:
            print(f"  {entry}")
        print(
            "\nMove the shared declaration down into a layer both sides may see, or"
            "\ninvert the dependency with an interface. Do not add these to"
            f"\n{ALLOWLIST_PATH.relative_to(REPO_ROOT)} — that file may only shrink."
        )
        return 1

    fixed = sorted(allowlist - violations)
    if fixed:
        print("These allowlisted violations no longer exist. Please remove them:\n")
        for entry in fixed:
            print(f"  {entry}")
        print("\n  python3 tools/check_layering.py --update")
        return 1

    print(f"Layering OK ({len(allowlist)} known violations still allowlisted).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
