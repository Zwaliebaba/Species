#!/usr/bin/env python3
"""Check that each .vcxproj lists exactly the source files present on disk.

A .cpp that exists but is missing from ClCompile silently does not get compiled;
a ClInclude entry pointing at a deleted header makes the project fail to load in
Visual Studio. Neither shows up as a compiler error, and both are easy to
introduce when adding or removing files without opening the IDE — which is
exactly how an agent works.

The matching .vcxproj.filters file is checked too, since a file missing there
still builds but disappears from the Solution Explorer tree.

Both the six library projects and every Tests/<Name>Tests project are checked. A
test file that never gets compiled is worse than a library file that never gets
compiled: the suite still reports green, and it reports green for a test nobody
is running.

    python3 tools/check_project_files.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

LIBRARY_PROJECTS = [
    "GameLogic",
    "NeuronClient",
    "NeuronCore",
    "NeuronServer",
    "Server",
    "Species",
]


def test_projects() -> list[str]:
    """Every Tests/<Name>Tests directory, discovered rather than listed.

    The six projects above are fixed; test projects are not — one is added every
    time a library gains coverage. A test project left off a hard-coded list
    would be silently unchecked, which is precisely the failure this tool exists
    to catch, so the list is derived from the tree instead.
    """
    tests_root = REPO_ROOT / "Tests"
    if not tests_root.is_dir():
        return []
    return sorted(
        f"Tests/{path.name}"
        for path in tests_root.iterdir()
        if path.is_dir() and (path / f"{path.name}.vcxproj").exists()
    )

# Suffix -> the MSBuild item type a file of that kind belongs to.
ITEM_TYPE_FOR_SUFFIX = {
    ".cpp": "ClCompile",
    ".h": "ClInclude",
    ".inc": "ClInclude",
}

INCLUDE_ATTR_RE = re.compile(r'<(ClCompile|ClInclude)\s+Include="([^"]+)"')


def listed_files(project_file: Path) -> dict[str, set[str]]:
    """item type -> set of file names the project file references."""
    text = project_file.read_text(encoding="utf-8-sig")
    result: dict[str, set[str]] = {"ClCompile": set(), "ClInclude": set()}
    for item_type, include in INCLUDE_ATTR_RE.findall(text):
        result[item_type].add(include.replace("\\", "/"))
    return result


def files_on_disk(project_dir: Path) -> dict[str, set[str]]:
    result: dict[str, set[str]] = {"ClCompile": set(), "ClInclude": set()}
    for path in sorted(project_dir.iterdir()):
        item_type = ITEM_TYPE_FOR_SUFFIX.get(path.suffix)
        if item_type:
            result[item_type].add(path.name)
    return result


def report(problems: list[str], label: str, entries: set[str], project: str) -> None:
    for entry in sorted(entries):
        problems.append(f"  {project}: {label} {entry}")


def main() -> int:
    problems: list[str] = []

    for project_path in LIBRARY_PROJECTS + test_projects():
        project = Path(project_path).name
        project_dir = REPO_ROOT / project_path
        project_file = project_dir / f"{project}.vcxproj"
        filters_file = project_dir / f"{project}.vcxproj.filters"

        disk = files_on_disk(project_dir)
        listed = listed_files(project_file)

        for item_type in ("ClCompile", "ClInclude"):
            report(
                problems,
                f"on disk but not in {project}.vcxproj ({item_type}):",
                disk[item_type] - listed[item_type],
                project,
            )
            report(
                problems,
                f"listed in {project}.vcxproj ({item_type}) but missing from disk:",
                listed[item_type] - disk[item_type],
                project,
            )

        if filters_file.exists():
            filtered = listed_files(filters_file)
            for item_type in ("ClCompile", "ClInclude"):
                report(
                    problems,
                    f"in {project}.vcxproj but not in .filters ({item_type}):",
                    listed[item_type] - filtered[item_type],
                    project,
                )
                report(
                    problems,
                    f"in .filters but not in {project}.vcxproj ({item_type}):",
                    filtered[item_type] - listed[item_type],
                    project,
                )

    if problems:
        print("Project files are out of sync with the files on disk:\n")
        print("\n".join(problems))
        print(
            "\nAdd or remove the corresponding <ClCompile Include=\"...\" /> or"
            "\n<ClInclude Include=\"...\" /> entries in both the .vcxproj and its"
            "\n.vcxproj.filters. See CODING_STANDARDS.md, 'Adding and removing files'."
        )
        return 1

    print("Project files match the tree.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
