# Architecture

How Species is put together, what each layer is responsible for, and where the
structure currently disagrees with the intent.

This describes the code as it exists. Where something is aspirational it says so.

---

## Layers

```
                    ┌──────────────────────────────────┐
                    │           NeuronCore             │  static lib
                    │  sockets, threads, byte streams  │
                    │  wire protocol, filesystem,      │
                    │  assertions, Neuron:: helpers    │
                    └───────────────┬──────────────────┘
                    ┌───────────────┴──────────────────┐
        ┌───────────▼───────────┐      ┌───────────────▼───────────┐
        │     NeuronClient      │      │       NeuronServer        │  static libs
        │  OpenGL renderer,     │      │  authoritative simulation │
        │  sound, input, the    │      │  host                     │
        │  Eclipse UI toolkit,  │      │                           │
        │  resources            │      │        (stub)             │
        └───────────┬───────────┘      └───────────────┬───────────┘
                    └───────────────┬──────────────────┘
                    ┌───────────────▼──────────────────┐
                    │            GameLogic             │  static lib
                    │  entities, buildings, teams,     │
                    │  unit behaviour, in-game windows │
                    └───────────────┬──────────────────┘
            ┌───────────────────────┴───────────────────────┐
   ┌────────▼─────────┐                          ┌──────────▼────────┐
   │     Species      │  exe                     │      Server       │  exe
   │  app, world,     │                          │                   │
   │  camera, levels  │                          │      (stub)       │
   └──────────────────┘                          └───────────────────┘
```

Dependencies point downward only. `tools/check_layering.py` enforces it against
`tools/layering_allowlist.txt`, which holds the inherited violations the
migration has yet to remove.

Each static library carries a `Tests/<Name>Tests` project that sits directly
above it and inherits its dependencies — a test may reach no further up than the
code it covers already does, and the same checker enforces that. The two
executables have none: an `.exe` cannot be linked into a test DLL, so code in
`Species` or `Server` worth testing is code that belongs in a library. See
[`TESTING.md`](TESTING.md).

### NeuronCore

The foundation. Everything else may depend on it; it may depend on nothing.

- **Networking:** `NetLib`, `NetSocket`, `NetSocketListener`, `NetUdpPacket`,
  `NetThread`, `NetMutex`. UDP, with Win32 implementations
  (`NetThreadWin32.cpp`, `NetMutexWin32.cpp`) behind platform-neutral headers.
  `NetLibApple.h` exists but is not built.
- **Protocol:** `NetworkUpdate` (client→server events), `ServerToClientLetter`
  (server→client broadcasts), `TeamControls`, `ProtocolLimits.h` and the
  `ByteStream.h` read/write macros. Fixed 42-byte packets
  (`NETWORKUPDATE_BYTESTREAMSIZE`). The two endpoints that speak it live above:
  `ClientToServer` in `NeuronClient`, `Server` and `ServerToClient` in
  `NeuronServer`.
- **Settings:** `Preferences`, which reads and writes the preferences file
  without the resource system. The host overlays its shipped defaults through
  `PrefsManager::SetDefaultsProvider`.
- **Platform:** `FileSys`, `Debug.h` (`ASSERT`, `DebugTrace`, `Fatal`),
  `NeuronHelper.h` (`BaseException`, `NonCopyable`, `ScopedHandle`,
  `ENUM_HELPER`).

This is the layer furthest through modernisation: `FileSys`, `Debug` and
`NeuronHelper` are fully Neuron-style, and `Server.cpp` is partly converted.

**It has no upward includes left**, down from thirty when
`tasks/neuroncore-layering.yaml` was written. What is left is the door rather
than the violations: `NeuronCore.vcxproj` still lists `NeuronClient`, `Species`
and `GameLogic` in `AdditionalIncludeDirectories`, so a new upward include would
still compile. Closing that, and linking a `Server.exe` that ticks, is T10.

### NeuronClient

Presentation and platform services for a graphical client.

- **Rendering:** OpenGL, `Shape`/`ShapeFragment` model system, `Texture`,
  `Bitmap`, sprites, text renderers, `OGLExtensions`.
- **Sound:** `SoundSystem`, `SoundLibrary2d`, `SoundLibrary3dDSound` and a
  software mixer fallback, `SoundInstance`, `SoundParameter`.
- **Input:** a composable driver stack — `InputDriverSimple`, `Chord`, `Conjoin`,
  `Alias`, `Invert`, `Pipe`, `Idle`, `Prefs` — resolving to `ControlTypes`.
- **UI:** the **Eclipse** toolkit (`Eclipse`, `EclWindow`, `EclButton`), which
  every in-game window derives from.
- **Networking:** `ClientToServer`, the client's endpoint — inbox, outbox,
  sockets and sequence ids. Moved up out of `NeuronCore` by T8.
- **Utilities that do not belong here:** none left. The containers (`LList`,
  `DArray`, `BTree`, `FastDArray`, `HashTable`), the maths types (`Vector3`,
  `Matrix33/34`, `MathUtils`), `HiResTime`, `Profiler` and `Preferences` have all
  moved down into `NeuronCore`.

### NeuronServer

The authoritative host. `Server` holds the client list, team registry, sequence
counter, inbox, outbox and the per-sequence sync values; `ServerToClient` is one
connection to one client. Both moved down out of `NeuronCore`, which is what
turned this from a stub into a library a headless binary can link.

It depends on `NeuronCore` and nothing else — `NeuronServer` → `NeuronClient` is
the one direction the layering forbids outright, because it is what would make a
headless server impossible again.

### GameLogic

The bulk of the inherited code, ~48k lines. Entities (`Darwinian`, `Engineer`,
`Officer`, `Armour`, `Spider`, `Centipede`, `SoulDestroyer`, …), buildings
(`Factory`, `Generator`, `RadarDish`, `GunTurret`, `LaserFence`, `Teleport`, …),
`Ai`, `Weapons`, and the in-game windows built on Eclipse.

Its includes still reach up into `Species` for `Location`, `Team`,
`GlobalWorld`, `LevelFile` and `Camera` — the largest remaining violation
cluster, reflecting Darwinia's original design where everything lived in one
binary. `App` is no longer among them: the subsystem pointers, the application
state and the few app-level actions moved to `NeuronClient` behind
`WorldPointers.h`, `AppState.h` and the `AppCommands` interface, so nothing
below `Species` includes `App.h`.

### Species

The client executable. Application object and main loop (`App`, `Main`),
world and level model (`GlobalWorld`, `Location`, `LevelFile`, `Landscape`,
`Water`, `Clouds`), camera, `Team`/`Unit`, the task manager and its interface,
particle systems, and the location editor.

### Server

A stub: `WinMain.cpp` returning 0, and a `pch`. Links `NeuronCore` and
`NeuronServer` and nothing else — the library it needs now exists, but nothing
drives it. Making it tick is T10.

---

## Runtime model

The multiplayer model is inherited from Darwinia and is **deterministic
lockstep**, not client-server authority in the modern sense.

- The server assigns each broadcast a `m_sequenceId` and sends
  `ServerToClientLetter`s over UDP. Clients track
  `m_lastValidSequenceIdFromServer` and buffer out-of-order arrivals.
- Clients send `NetworkUpdate` events — `ClientJoin`, `RequestTeam`,
  `SelectUnit`, `CreateUnit`, `AimBuilding`, `RunProgram`, … — as *intent*. The
  server sequences them; every client then applies the same sequence and is
  expected to compute the same result.
- Each frame is advanced in `NUM_SLICES_PER_FRAME` (10) slices so heavy physics
  can be spread across the frame.
- `g_predictionTime` runs the local simulation slightly ahead of the last
  confirmed server advance to hide latency.
- A periodic `Syncronise` update carries a checksum; `Server::m_sync` records one
  value per sequence id so divergence is detected.

**This makes bit-identical simulation a hard requirement, not a nicety.**
`GenerateSyncValue()` sums entity positions and velocities in container index
order, so iteration order, container identity, floating-point arithmetic order
and the `speciesRandom()` call sequence are all load-bearing. `DArray` indices
are part of object identity on the wire (`WorldObjectId::m_index` is serialised
verbatim). The constraints this puts on ordinary refactoring are spelled out in
[`CODING_STANDARDS.md`](../CODING_STANDARDS.md#determinism) — read that before
changing anything reachable from `Location::Advance`.

**This will not scale to the target.** Deterministic lockstep requires every
client to simulate the entire world, which is incompatible with a large
persistent world holding many colonies. Replacing it is future work and is
explicitly out of scope until the layering and modernisation land — which is
precisely why extracting `NeuronCore` from the client matters: a real
authoritative server cannot be written while the network layer depends on the
renderer's containers and the game's entity types.

**Recorded for that replacement, so it is not forgotten (owner, 2026-08-02):**
when the protocol changes, object identity should stop being a slot index and
become a deterministically generated stable object id — a counter incremented
in sequenced creation order assigns identical ids on every client — resolved
through a lookup table. Slot reuse means a stale `WorldObjectId` can silently
alias whatever entity later occupies the slot; generated ids cannot alias, and
they free the storage layer's container choice. This is deliberately **not**
part of the modernisation plans: it changes the wire format and the entity
update order, both frozen while behaviour must not change, and slot-index
resolution is hot-path O(1) where a tree map is not. What the plans do now is
keep the swap cheap: `Neuron::SlotMap` (`tasks/containers-replaced.yaml` T3)
exposes a narrow handle-in/reference-out API so no caller does raw index
arithmetic, leaving the eventual identity change one bounded edit rather than
a tree-wide hunt.

---

## Content

`GameData/` holds everything the game loads at runtime: `Levels/`, `Locations/`,
`Shapes/`, `Textures/`, `Sprites/`, `Sounds/`, `Terrain/`, `Icons/`, `Scripts/`,
`Language/`, and the top-level configuration files (`Game.txt`, `Effects.txt`,
`Sounds.txt`, `Stats.txt`, `DefaultPreferences.txt`).

`GameData.targets` stages the whole tree next to the executable after each build,
incrementally, and clears it on `Clean`. Content is resolved at runtime relative
to the working directory — `FileSys::GetFullPathA` prefixes the home directory,
and `Resource.cpp` asks for paths like `Shapes/foo.shp`.

CI asserts the staging actually happened; a build where it silently stopped would
produce an executable that starts and then fails to find any asset.

Tests do not read `GameData/`. An integration test that needs content on disk
creates it under its own temporary directory — reading the real tree in place
couples the suite to content that changes for unrelated reasons, and writing into
it makes the next test's failure somebody else's problem.

---

## Where to look

If a type name means nothing to you — `Spirit`, `TrunkPort`, `Incubator` —
[`GLOSSARY.md`](GLOSSARY.md) defines it.

| Concern | Start at |
|---|---|
| Frame loop, timing, prediction | `Species/Main.cpp` |
| Application state, subsystem ownership | `Species/App.cpp` |
| Wire protocol | `NeuronCore/NetworkUpdate.h`, `ServerToClientLetter.h` |
| Server tick and client registry | `NeuronCore/Server.cpp` |
| Client-side netcode | `NeuronCore/ClientToServer.cpp` |
| Entity behaviour | `GameLogic/Entity.cpp`, `GameLogic/Darwinian.cpp` |
| Building behaviour | `GameLogic/Building.cpp` |
| Rendering entry | `Species/Renderer.cpp`, `Species/LandscapeRenderer.cpp` |
| UI toolkit | `NeuronClient/Eclipse.cpp`, `EclWindow.cpp` |
| Content loading | `NeuronClient/Resource.cpp`, `Species/LevelFile.cpp` |
