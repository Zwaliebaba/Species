#!/usr/bin/env python3
"""Catch legacy container calls left on a converted member.

WHY THIS EXISTS. containers-replaced converted about forty members from the
inherited LList/DArray family to std::vector, across roughly a thousand call
sites. Three separate CI failures came from the same shape of mistake: a call
site the sweep did not reach, still asking a std::vector for Size(), GetData()
or ValidIndex(). Every one was a compile error, so CI did catch them — after a
full Windows build each time, which is the slowest possible way to learn it.

The check is a cross-file type map, which is why it is not a check_hygiene
rule: hygiene matches patterns on changed lines, and this needs to know what
type `m_tasks` is in a header the changed line never mentions.

WHAT IT CANNOT DO. It matches on member NAME, not on a resolved expression
type, so a name declared as a vector in one class and a SlotMap in another is
skipped and counted rather than guessed at. That is not hypothetical —
m_buildings, m_spirits and m_lights are each both, and getting the receiver
wrong on one of them was itself one of the three failures. An earlier draft
reported them as "check the receiver by hand" and produced several hundred
lines nobody would read, which is a worse failure than the blind spot.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PROJECTS = ("NeuronCore", "NeuronClient", "NeuronServer", "GameLogic", "Species", "Server", "Tests")

# Methods the legacy containers had that std::vector does not. SlotMap keeps
# several of these deliberately (it is the DArray replacement), so a hit only
# matters when the receiver is a vector — hence the type map below.
LEGACY_METHODS = (
    "Size",
    "PutData",
    "PutDataAtStart",
    "PutDataAtEnd",
    "PutDataAtIndex",
    "GetData",
    "GetPointer",
    "RemoveData",
    "EmptyAndDelete",
    "EmptyAndDeleteArray",
    "ValidIndex",
    "FindData",
    "MarkUsed",
    "MarkNotUsed",
    "NumUsed",
    "SetStepSize",
    "SetStepDouble",
    "SetSize",
    "LookupTree",
)

VECTOR_DECL = re.compile(r"\bstd::vector\s*<.+>\s*\*?\s*(m_\w+)\s*[;=]")
OTHER_DECL = re.compile(r"\b(?:SlotMap|FastSlotMap|Array2D|SurfaceMap2D)\s*<.+>\s*\*?\s*(m_\w+)\s*[;=]")
CALL = re.compile(r"\b(m_\w+)\s*\.\s*(" + "|".join(LEGACY_METHODS) + r")\s*\(")


def strip_noise(text: str) -> str:
    """Remove comments and string literals so neither can produce a finding."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return text


def sources() -> list[Path]:
    found: list[Path] = []
    for project in PROJECTS:
        root = REPO_ROOT / project
        if not root.is_dir():
            continue
        found.extend(p for p in sorted(root.rglob("*")) if p.suffix in (".h", ".cpp", ".inc"))
    return found


def main() -> int:
    files = sources()

    vectors: dict[str, set[str]] = {}
    others: dict[str, set[str]] = {}
    for path in files:
        text = strip_noise(path.read_text(encoding="utf-8", errors="replace"))
        rel = str(path.relative_to(REPO_ROOT))
        for name in VECTOR_DECL.findall(text):
            vectors.setdefault(name, set()).add(rel)
        for name in OTHER_DECL.findall(text):
            others.setdefault(name, set()).add(rel)

    # A name that is a vector in one class and a slot map in another cannot be
    # resolved from the name alone, and reporting those produced hundreds of
    # lines of "check by hand" that nobody would read. They are skipped and
    # counted instead, so the blind spot is stated rather than hidden.
    # m_buildings, m_spirits and m_lights are the ones this loses.
    unresolvable = sorted(set(vectors) & set(others))
    decidable = {name: where for name, where in vectors.items() if name not in others}

    failures: list[str] = []
    for path in files:
        rel = str(path.relative_to(REPO_ROOT))
        stripped = strip_noise(path.read_text(encoding="utf-8", errors="replace"))
        # Keep line numbers aligned with the original file.
        for number, line in enumerate(stripped.splitlines(), start=1):
            for member, method in CALL.findall(line):
                if member not in decidable:
                    continue
                where = ", ".join(sorted(decidable[member]))
                failures.append(
                    f"  {rel}:{number}: {member}.{method}() — {member} is a std::vector "
                    f"(declared in {where})"
                )

    if failures:
        print("Legacy container calls left on a std::vector:\n")
        for failure in failures:
            print(failure)
        print(
            f"\n{len(failures)} finding(s). These are compile errors; the check exists so you "
            "find them in a second rather than after a full Windows build.\n"
            "See tasks/containers-replaced.yaml."
        )
        return 1

    skipped = f", {len(unresolvable)} name(s) ambiguous and skipped" if unresolvable else ""
    print(f"Containers OK ({len(files)} file(s), {len(decidable)} converted member name(s) tracked{skipped}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
