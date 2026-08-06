#!/usr/bin/env python3
"""Keep GameData/Effects.txt and the FX enum aligned position for position.

WHY THIS EXISTS. SoundSystem::LoadEffects appends one DspBlueprint per EFFECT
block, so an effect's index IS its line position in Effects.txt, and
SoundLibrary3d.h's enum is a second, independent copy of that ordering. Nothing
in the game checks that the two agree:

  * LoadEffects sizes the table with NUM_FILTERS and then ignores it while
    appending, so a count mismatch is not noticed.
  * LoadtimeVerify would be the natural place, but it only checks that WAV
    files referenced by sample groups exist -- and its one call site is
    commented out, so it does not run at all.
  * ParseSoundEffect resolves an EFFECT name in Sounds.txt against the
    blueprint table and DEBUG_ASSERTs on a miss, which fires only in a Debug
    build, only once that sound is first loaded.

So a reordered block, or an enumerator added without its data row, applies the
WRONG EFFECT to a sound and the game still builds, still loads and still runs.
That is the exact failure sound-xaudio2 T4 was rescheduled to avoid, and it is
worth two seconds of CI rather than an ear.

WHAT IT CHECKS.

  1. Effects.txt has one block per enumerator, in the same order, with matching
     names -- ResonantLowPass <-> DSP_RESONANTLOWPASS, ignoring case and the
     underscores the enum spells names with.
  2. Every "EFFECT <name>" in Sounds.txt names a block Effects.txt declares.
  3. No block declares more than MAX_PARAMS parameters, because DspHandle
     stores them in a fixed array of that size and ParseSoundEffect indexes it
     without a bound check.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
EFFECTS = REPO_ROOT / "GameData" / "Effects.txt"
SOUNDS = REPO_ROOT / "GameData" / "Sounds.txt"
HEADER = REPO_ROOT / "NeuronClient" / "SoundLibrary3d.h"
INSTANCE_HEADER = REPO_ROOT / "NeuronClient" / "SoundInstance.h"

# The data files predate UTF-8 and are not written in it.
ENCODING = "latin-1"


def read(path: Path) -> str:
    return path.read_text(encoding=ENCODING)


def parse_effects() -> list[tuple[str, int]]:
    """Every EFFECT block in file order, with how many parameters it declares."""
    blocks: list[tuple[str, int]] = []
    name = None
    params = 0
    for line in read(EFFECTS).splitlines():
        stripped = line.split("#", 1)[0].strip()
        if not stripped:
            continue
        if stripped.upper().startswith("EFFECT "):
            name = stripped.split(None, 1)[1].strip()
            params = 0
        elif stripped.upper() == "END":
            if name is not None:
                blocks.append((name, params))
            name = None
        elif name is not None:
            params += 1
    return blocks


def parse_enum() -> list[str]:
    """The FX enumerators, in declaration order, up to NUM_FILTERS."""
    text = read(HEADER)
    match = re.search(r"enum\s*\{(.*?)\}\s*;", text, re.DOTALL)
    while match and "NUM_FILTERS" not in match.group(1):
        match = re.search(r"enum\s*\{(.*?)\}\s*;", text[match.end() :], re.DOTALL)
    if not match:
        sys.exit(f"{HEADER.name}: no enum containing NUM_FILTERS found")

    names = []
    for raw in match.group(1).split(","):
        token = re.sub(r"//.*", "", raw).strip()
        if not token or token == "NUM_FILTERS":
            continue
        names.append(token)
    return names


def parse_max_params() -> int:
    match = re.search(r"#define\s+MAX_PARAMS\s+(\d+)", read(INSTANCE_HEADER))
    if not match:
        sys.exit(f"{INSTANCE_HEADER.name}: MAX_PARAMS is not defined")
    return int(match.group(1))


def normalise(name: str) -> str:
    """DSP_RESONANTLOWPASS and ResonantLowPass are the same effect. So are
    DSP_I3DL2REVERB and I3DL2Reverb, and DSP_SIMPLE_REVERB and SimpleReverb."""
    return re.sub(r"^DSP_", "", name).replace("_", "").upper()


def main() -> int:
    blocks = parse_effects()
    enumerators = parse_enum()
    max_params = parse_max_params()
    problems: list[str] = []

    if len(blocks) != len(enumerators):
        problems.append(
            f"Effects.txt declares {len(blocks)} effect(s) and the enum has "
            f"{len(enumerators)} enumerator(s) before NUM_FILTERS; every index below "
            "the shorter of the two is suspect"
        )

    for index, (block, enumerator) in enumerate(zip(blocks, enumerators)):
        if normalise(block[0]) != normalise(enumerator):
            problems.append(
                f"index {index}: Effects.txt has '{block[0]}' where the enum has "
                f"'{enumerator}' -- one of the two moved without the other"
            )

    for name, count in blocks:
        if count > max_params:
            problems.append(
                f"effect '{name}' declares {count} parameters and MAX_PARAMS is "
                f"{max_params}; ParseSoundEffect would write past DspHandle::m_params"
            )

    declared = {normalise(name) for name, _ in blocks}
    used = set()
    for line in read(SOUNDS).splitlines():
        match = re.match(r"\s*EFFECT\s+(\S+)\s*$", line)
        if match:
            used.add(match.group(1))
    for name in sorted(used):
        if normalise(name) not in declared:
            problems.append(f"Sounds.txt uses effect '{name}', which Effects.txt does not declare")

    if problems:
        for problem in problems:
            print(f"sound effects: {problem}")
        return 1

    print(f"sound effects: OK — {len(blocks)} effect(s) aligned, {len(used)} name(s) used in Sounds.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
