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

**The check is strict.** Any upward include anywhere fails it. There is no
allowlist and no way to record an exception — tools/layering_allowlist.txt held
628 inherited violations when tasks/layering-inversion.yaml started and zero when
it finished, and the file was deleted rather than left empty so that nobody can
reopen it by adding a line. If a change needs an upward include, the change is
wrong: move the shared declaration down into a layer both sides can see, or
invert the dependency with an interface, as that plan did fifteen times.

    python3 tools/check_layering.py

INCLUDES ARE NOT THE ONLY WAY UP, which is the second half of this check and the
more easily missed one. A lower layer can DECLARE a symbol in its own header and
let the executable define it, and every include-based check in the world will
call that clean while the linker quietly resolves upward. The tree contained one
for years: NeuronClient/WindowManager.h declared `void AppMain();` and
WindowManager.cpp called it from WinMain, with the definition in Species. It
surfaced only when GameLogicTests grew an object graph big enough to pull that
object file into a DLL with no executable attached. See layering-inversion T18.

So this also reports any free function or extern variable that a NeuronCore,
NeuronClient or GameLogic header declares and only an executable defines. Class
members are exempt and deliberately so: a pure-virtual method declared low and
overridden high is dependency INVERSION — the call goes through a vtable the
lower layer owns — which is the pattern the plan used to remove these in the
first place.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

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


# A class body, so it can be cut out. A member declared low and defined high is
# dependency inversion through a vtable, not a link-time reach upward, and is
# exactly the pattern that removed these violations — so only namespace-scope
# declarations are interesting here.
CLASS_HEAD_RE = re.compile(r"\b(?:class|struct)\s+\w+[^;{]*\{")

# `void AppMain();` — a return type, a name, a parameter list, a semicolon.
FUNCTION_DECL_RE = re.compile(r"^[A-Za-z_][\w:*&<>,\s]*?\b([A-Za-z_]\w*)\s*\([^;{)]*\)\s*;", re.M)
# `void SoundSystem::Advance() {` or `void AppMain() {`
FUNCTION_DEF_RE = re.compile(r"^[A-Za-z_][\w:*&<>,\s]*?\b([A-Za-z_]\w*)\s*\([^;{)]*\)\s*\{", re.M)
EXTERN_DECL_RE = re.compile(r"^\s*extern\s+[\w:*&<>,\s]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;", re.M)
# `App* g_app = nullptr;` / `int g_sliceNum;` at file scope
VARIABLE_DEF_RE = re.compile(r"^[A-Za-z_][\w:*&<>,\s]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*(?:=|;)", re.M)

LIBRARIES = ("NeuronCore", "NeuronClient", "NeuronServer", "GameLogic")
EXECUTABLES = ("Species", "Server")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//.*$", "", text, flags=re.M)


def strip_class_bodies(text: str) -> str:
    """Remove every class and struct body, leaving namespace scope behind."""
    out, index = [], 0
    while True:
        match = CLASS_HEAD_RE.search(text, index)
        if not match:
            out.append(text[index:])
            return "".join(out)
        out.append(text[index : match.start()])
        depth, position = 1, match.end()
        while depth and position < len(text):
            if text[position] == "{":
                depth += 1
            elif text[position] == "}":
                depth -= 1
            position += 1
        index = position


def names_in(paths, pattern, transform=None) -> set[str]:
    found: set[str] = set()
    for path in paths:
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        found |= set(pattern.findall(transform(text) if transform else text))
    return found


def sources(projects, suffix: str) -> list[Path]:
    return [
        path
        for project in projects
        for path in sorted((REPO_ROOT / PROJECT_DIRS[project]).iterdir())
        if path.suffix == suffix
    ]


def find_link_violations() -> list[str]:
    """Symbols a library header declares that only an executable defines.

    An include check cannot see these. The library compiles, because the
    declaration is right there in its own header; the executable links, because
    it supplies the definition; and anything else that links the library — a
    test DLL, a second executable — fails with an unresolved external on a
    symbol it has no reason to know about.
    """
    headers = sources(LIBRARIES, ".h")
    declared = names_in(headers, FUNCTION_DECL_RE, strip_class_bodies)
    declared |= names_in(headers, EXTERN_DECL_RE, strip_class_bodies)

    library_sources = sources(LIBRARIES, ".cpp")
    defined_below = names_in(library_sources, FUNCTION_DEF_RE)
    defined_below |= names_in(library_sources, VARIABLE_DEF_RE)

    executable_sources = sources(EXECUTABLES, ".cpp")
    defined_above = names_in(executable_sources, FUNCTION_DEF_RE)
    defined_above |= names_in(executable_sources, VARIABLE_DEF_RE)

    return sorted((declared & defined_above) - defined_below)


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
    argparse.ArgumentParser(description=__doc__).parse_args()

    failed = False

    includes = sorted(find_violations(build_header_index()))
    if includes:
        failed = True
        print("Layering violations — an include reaching into a higher layer:\n")
        for entry in includes:
            print(f"  {entry}")
        print(
            "\nMove the shared declaration down into a layer both sides may see, or"
            "\ninvert the dependency with an interface. There is no allowlist to add"
            "\nthese to; see tasks/layering-inversion.yaml for fifteen worked examples."
        )

    links = find_link_violations()
    if links:
        if failed:
            print()
        failed = True
        print("Layering violations — declared in a library header, defined only in an executable:\n")
        for name in links:
            print(f"  {name}")
        print(
            "\nThe include graph looks clean and the linker does not: anything else that"
            "\nlinks the library — a test DLL, the other executable — fails with an"
            "\nunresolved external. Move the definition down, or invert the dependency"
            "\nwith an interface the library owns. See layering-inversion T18."
        )

    if failed:
        return 1

    print("Layering OK — no upward include, and no symbol declared low and defined high.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
