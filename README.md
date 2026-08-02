# Species

**World Core** — a C++20 game built on the Neuron engine.

The goal is a large-scale realtime multiplayer world in which player colonies
work, live and survive persistently. The codebase is partway there: it began as
the Darwinia source and is being reshaped — renamed, relayered and modernised —
into a foundation an authoritative world server can be built on.

> **Status: not runnable.** The most recent commit is titled *"Cleanup done, But
> does not work yet"*. This is a project under reconstruction, not a playable
> game.

---

## Layout

| | |
|---|---|
| `NeuronCore/` | Sockets, threads, wire protocol, filesystem, assertions |
| `NeuronClient/` | OpenGL renderer, sound, input, the Eclipse UI toolkit |
| `NeuronServer/` | Authoritative simulation host *(stub)* |
| `GameLogic/` | Entities, buildings, teams, unit behaviour |
| `Species/` | Client executable |
| `Server/` | Server executable *(stub)* |
| `GameData/` | Levels, shapes, textures, sounds, scripts |

~113,000 lines of C++20. No third-party dependencies — it links only against the
operating system.

## Building

Visual Studio 2026 (toolset v145), Windows, ARM64 or x64:

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
```

Full detail in [`docs/BUILD.md`](docs/BUILD.md).

## Contributing

Start with [`AGENTS.md`](AGENTS.md) — it covers the current priority, the layering
rules, and what to run before pushing. Then:

- [`CODING_STANDARDS.md`](CODING_STANDARDS.md) — style, and the modernisation plan
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — how the layers fit together
- [`docs/TASK_DAG.md`](docs/TASK_DAG.md) — how work is broken down

Before pushing:

```bash
python3 tools/check_project_files.py
python3 tools/check_layering.py
python3 tools/check_task_dag.py
python3 tools/check_format.py
```

## Provenance

Species derives from the Darwinia source by Introversion Software. The repository
does not currently carry a `LICENSE` file; one should be added reflecting the
terms the original source was made available under.
