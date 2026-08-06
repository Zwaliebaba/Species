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
        │  Eclipse UI toolkit,  │      │  host: clients, teams,    │
        │  resources            │      │  sequence, sync values     │
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
   │  app, world,     │                          │  headless host,   │
   │  camera, levels  │                          │  ticks at 10 Hz   │
   └──────────────────┘                          └───────────────────┘
```

Dependencies point downward only, and do so today — `tools/check_layering.py`
enforces it strictly, with no allowlist. It also rejects a symbol declared in a
library header and defined only in an executable, which reaches upward through
the linker rather than the preprocessor.

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

This is the layer furthest through modernisation: `FileSys`, `Debug`,
`NeuronHelper`, `SlotMap`, `SliceWalker`, `LookupTable` and `VectorUtils` are
fully Neuron-style and live in `namespace Neuron`. The networking and protocol
types are not there yet and are still at global scope — see
[Namespaces](#namespaces), because that split is what a forward declaration in
this layer has to get right.

**It has no upward includes left**, down from thirty when
`tasks/Archive/neuroncore-layering.yaml` was written, and the door is shut as
well: `NeuronCore.vcxproj` lists no other project in
`AdditionalIncludeDirectories` at all, so a new upward include does not compile.
That was T10, and it is what let `Server.exe` link and tick.

### NeuronClient

Presentation and platform services for a graphical client.

- **Rendering:** OpenGL, `Shape`/`ShapeFragment` model system, `Texture`,
  `Bitmap`, sprites, text renderers, `OGLExtensions`.
- **Sound:** `SoundSystem`, `SoundInstance`, `SoundParameter` above a
  `SoundLibrary3d` backend. ONE backend remains: `SoundLibraryXAudio2` — a mono
  source voice per channel plus a stereo one for music, X3DAudio positioning and
  doppler, the resonant low pass on XAudio2's own per-voice biquad, echo on
  FXECHO and reverb on XAUDIO2FX. `SoundLibrary3dDSound`,
  `SoundLibrary3dSoftware` and `SoundLibrary2d`'s WinMM output layer were
  deleted by `sound-xaudio2` T7 and T8, and the `SoundLibrary` preference that
  chose between them by T10. The seam stays virtual because it is what let the
  replacement be A/B'd against them. A device that disappears mid-game is
  reported through `IXAudio2EngineCallback::OnCriticalError`, which parks the
  backend silent; `SoundSystem::Advance` then rebuilds it every five seconds
  until a device comes back.
- **Input:** a composable driver stack — `InputDriverSimple`, `Chord`,
  `Conjoin`, `Invert`, `Idle` — resolving to `ControlTypes`. `Alias`, `Pipe`
  and `Prefs` were deleted by `input-native-events` T1: no binding data used
  them and `Pipe` was never even registered.
- **UI:** the **Eclipse** toolkit (`Eclipse`, `EclWindow`, `EclButton`), which
  every in-game window derives from.
- **Networking:** `ClientToServer`, the client's endpoint — inbox, outbox,
  sockets and sequence ids. Moved up out of `NeuronCore` by T8.
- **Utilities that do not belong here:** none left. `MathUtils`, `HiResTime`,
  `Profiler` and `Preferences` have all moved down into `NeuronCore`. The
  inherited maths types moved down too and have since been deleted outright —
  storage is DirectXMath's own, and `NeuronMath.h` holds the conventions rather
  than a type. So did the inherited containers; see `SlotMap` below.

### NeuronServer

The authoritative host. `Server` holds the client list, team registry, sequence
counter, inbox, outbox and the per-sequence sync values; `ServerToClient` is one
connection to one client. Both moved down out of `NeuronCore`, which is what
turned this from a stub into a library a headless binary can link.

It depends on `NeuronCore` and nothing else — `NeuronServer` → `NeuronClient` is
the one direction the layering forbids outright, because it is what would make a
headless server impossible again.

### GameLogic

The bulk of the inherited code, ~48k lines. Entities (`Citizen`, `Engineer`,
`Officer`, `Armour`, `Spider`, `Centipede`, `SoulDestroyer`, …), buildings
(`Factory`, `Generator`, `RadarDish`, `GunTurret`, `LaserFence`, `Teleport`, …),
`Ai`, `Weapons`, and the in-game windows built on Eclipse.

**It reaches up into `Species` for nothing.** It used to, for `Location`,
`Team`, `GlobalWorld`, `LevelFile` and `Camera` — the largest violation cluster
in the tree, reflecting Darwinia's original design where everything lived in one
binary. `tasks/Archive/layering-inversion.yaml` resolved every one of them, and
mostly by MOVING THE MODEL DOWN rather than by inverting a dependency: the world
model — `Location`, `GlobalWorld`, `Team`, `Unit`, `LevelFile`, the grids, the
routing system, the landscape — lives here now, which is why this layer is the
bulk of the tree.

What did invert went behind `*Access` interfaces in `NeuronClient` — `Renderer`,
`Camera`, `Script`, `UserInput`, `TaskManagerInterface`, `ControlHelp`,
`LocationEditor`, `GameCursor` — and `App` went with them: the subsystem
pointers, the application state and the app-level actions are in
`WorldPointers.h`, `AppState.h` and the `AppCommands` interface, so nothing
below `Species` includes `App.h`.

### Species

The client executable, and it is now a thin one — ~15k lines. Application
object and main loop (`App`, `Main`), the camera, the renderer entry point, the
task manager interface, the location editor, the start sequence and the game
menu.

**The world model is NOT here any more.** `GlobalWorld`, `Location`,
`LevelFile`, `Landscape`, `Water`, `Team`, `Unit` and the particle systems all
moved down into `GameLogic` during `layering-inversion`, because a static
library cannot include a header from the executable that links it. If you are
looking for the world, look one layer down.

### Server

The headless binary. It links `NeuronCore` and `NeuronServer` and nothing else,
constructs the host and ticks it at 10 Hz; `Server.exe --ticks 20` runs it for
two seconds and reports the sequence id it reached. CI runs exactly that on
every push and fails if the id has not advanced, which is condition 3 of the
definition of done in [`AGENTS.md`](../AGENTS.md) checked rather than asserted.

It does not simulate a world. The host sequences whatever clients send it,
which is what it always did — what changed is that nothing above `NeuronServer`
has to be linked in for it to do so.

---

## Namespaces

Two, and the boundary between them is the layer boundary above.

| Namespace | Holds | Reached from outside by |
|---|---|---|
| `Neuron` | all of `NeuronClient` and `NeuronServer`, and the converted half of `NeuronCore` | a tree-wide using-directive |
| `Species` | all of `GameLogic` and all of the `Species` executable | nothing — its only users are inside it |

**`NeuronCore` is only PARTLY namespaced, and that is the fact that bites.**
`FileSys`, `Debug`, `NeuronHelper`, `SlotMap`, `SliceWalker`, `LookupTable` and
`VectorUtils` are in `namespace Neuron`. The networking and protocol types —
`NetLib`, `NetMutex`, `NetSocket`, `NetSocketListener`, `NetworkUpdate`,
`ServerToClientLetter`, `Profiler`, `TeamControls` — are still at global scope.
So "is this a Neuron type?" is a per-type question in that layer, not a
per-project one.

**Nothing had to be qualified when the engine moved.** `NeuronCore.h` ends with
`using namespace Neuron;` and every project's pch includes it, so every engine
name resolves unqualified from anywhere, exactly as before the migration. The
game namespace deliberately has no equivalent: `GameLogic` and the `Species`
executable are its only code and both are inside it, so they see each other
without one.

The exception is the test DLLs. `Tests/GameLogicTests` sits outside the game
namespace looking in, so its five game-touching sources each carry a
`using namespace Species;`. A `.cpp` is the right place for one — nothing
includes it.

### What a namespace change actually breaks

Not call sites. **Forward declarations.** A using-directive makes a name
findable; it does not make `class Renderer;` in a GameLogic header declare
`Neuron::Renderer`. It declares a new `::Renderer`, and the error surfaces at
LINK time pointing somewhere else entirely. 51 forward declarations across the
tree are wrapped for this reason, and `tools/check_layering.py` can see none of
them — a forward declaration includes nothing.

Three more shapes, each of which cost a CI round during
`tasks/Archive/namespace-migration.yaml`:

- **A block-scope `extern` does not join the enclosing namespace.** With no
  visible namespace-scope declaration to match, it gets external linkage in the
  GLOBAL namespace. `WindowManager.cpp` and `Win32EventHandler.cpp` each
  declared one inside the function that used it, and after the wrap they named
  `::g_keys` while the definition was `Neuron::g_keys`. All four such
  declarations in the tree are at namespace scope now.
- **A member of a global class cannot be defined inside a namespace.**
  `TeamControls` is a NeuronCore type still at global scope and
  `GameLogic/Team.cpp` defines its `Advance()`; that definition sits outside the
  game namespace, deliberately.
- **An elaborated type specifier declares a new type.** `void f(class Profiler*)`
  inside a namespace quietly means `Neuron::Profiler`, never defined.

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
and the RNG call sequence are all load-bearing.

**Which RNG, precisely: `syncrand`, not `speciesRandom`.** There are two
streams and this file used to name the wrong one. `syncrand`/`syncfrand`/
`syncsfrand` (`NeuronCore/MathUtils.cpp`, a Mersenne Twister) is the lockstep
stream the simulation draws from; `speciesRandom`/`frand`/`sfrand`
(`NeuronCore/Random.cpp`, an LCG over `holdrand`) is for cosmetics — particles,
render jitter, UI, sound — and cannot be made deterministic, because terrain and
tree generation reseed it wholesale and sound consumes it at a client-dependent
rate. `tasks/Archive/determinism.yaml` T3 and T4 are the reading that settled
it, and T5 fixed six sites that drew SIMULATION state from the cosmetic one. `SlotMap` indices
are part of object identity on the wire (`WorldObjectId::m_index` is serialised
verbatim), and the flavour matters: `FastSlotMap` pops a freelist and `SlotMap`
scans lowest-first, so they hand out different indices after a removal. The constraints this puts on ordinary refactoring are spelled out in
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
resolution is hot-path O(1) where a tree map is not. That has not changed now
those plans are finished — it is still unowned work needing a plan of its own.
What they did was keep the swap cheap: `Neuron::SlotMap`
(`tasks/Archive/containers-replaced.yaml` T3)
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
| World and level model | `GameLogic/Location.cpp`, `GlobalWorld.cpp`, `LevelFile.cpp` |
| Application state, subsystem ownership | `Species/App.cpp` |
| Wire protocol | `NeuronCore/NetworkUpdate.h`, `ServerToClientLetter.h` |
| Server tick and client registry | `NeuronServer/Server.cpp` |
| Client-side netcode | `NeuronClient/ClientToServer.cpp` |
| Entity behaviour | `GameLogic/Entity.cpp`, `GameLogic/Citizen.cpp` |
| Building behaviour | `GameLogic/Building.cpp` |
| Rendering entry | `Species/Renderer.cpp`, `GameLogic/LandscapeRenderer.cpp` |
| UI toolkit | `NeuronClient/Eclipse.cpp`, `EclWindow.cpp` |
| Content loading | `NeuronClient/Resource.cpp`, `GameLogic/LevelFile.cpp` |
