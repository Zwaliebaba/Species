# Species

**World Core** — a C++23 game built on the Neuron engine.

The goal is a large-scale realtime multiplayer world in which player colonies
work, live and survive persistently. The codebase is partway there: it began as
the Darwinia source and is being reshaped — renamed, relayered and modernised —
into a foundation an authoritative world server can be built on.

---

## Layout

| | |
|---|---|
| `NeuronCore/` | Sockets, threads, wire protocol, filesystem, assertions |
| `NeuronClient/` | OpenGL renderer, sound, input, the Eclipse UI toolkit |
| `NeuronServer/` | Authoritative simulation host — sequences client letters; no world yet |
| `GameLogic/` | Entities, buildings, teams, unit behaviour, and the world model |
| `Species/` | Client executable |
| `Server/` | Headless server executable — ticks the host at 10 Hz |
| `GameData/` | Levels, shapes, textures, sounds, scripts |
| `Tests/` | One `<Name>Tests` project per library |

Engine code is in `namespace Neuron`, game code in `namespace Species`. Includes
point downward only, with no allowlist.

~113,000 lines of C++23. No third-party dependencies — it links only against the
operating system.

## Building

Visual Studio 2026 (toolset v145), Windows, ARM64 or x64:

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
```

Full detail in [`docs/BUILD.md`](docs/BUILD.md).

## Tests

One `<Name>Tests` project per library under `Tests/`, on the Microsoft Native
Unit Test Framework that ships with Visual Studio — no third-party dependency,
so "clone and build" still holds. CI builds and runs all of them on every push.

```powershell
vstest.console.exe ARM64\Debug\*Tests.dll /Platform:ARM64
```

What to test and where it goes: [`docs/TESTING.md`](docs/TESTING.md).

## Contributing

Start with [`AGENTS.md`](AGENTS.md) — it covers the current priority, the layering
rules, and what to run before pushing. Then:

- [`CODING_STANDARDS.md`](CODING_STANDARDS.md) — style, ownership, strings and determinism
- [`docs/TESTING.md`](docs/TESTING.md) — what earns a test, and what a test may touch
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — how the layers fit together
- [`docs/TASK_DAG.md`](docs/TASK_DAG.md) — how work is broken down
- [`docs/GLOSSARY.md`](docs/GLOSSARY.md) — the game's domain vocabulary

Before pushing:

```bash
python3 tools/check_project_files.py   # .vcxproj matches the files on disk
python3 tools/check_layering.py        # no upward includes
python3 tools/check_task_dag.py        # task plans are valid DAGs
python3 tools/check_containers.py      # no legacy container call on a vector
python3 tools/check_math_types.py      # no legacy math call on a native type
python3 tools/check_sound_effects.py   # Effects.txt and the FX enum still line up
python3 tools/check_format.py          # changed lines match .clang-format
python3 tools/check_hygiene.py         # changed lines do not reintroduce NULL,
                                       # _included guards, strcpy or plain enum
```

then build and run the suite. CI runs the same eight and fails on anything
skipped; [`AGENTS.md`](AGENTS.md) explains what each one exists to catch.

## Licence

**Internal research project. Not for commercial use, and not for distribution.**
See [`LICENSE`](LICENSE).

Species derives from the Darwinia source by Introversion Software. The terms in
`LICENSE` cover this project's own contributions only — the licence covering the
original source has not been established, so treat the provenance as unresolved
rather than permissive.
