# Prompt: analyse Locations, design the int64×int64 procedural open world

Copy everything below the rule into a fresh agent session on this repository
(branch of your choice). It produces **design documents under `docs/`** — not
code changes, and not task-DAG files. A planning prompt in the style of
`_modernization-prompt.md` follows *after* the owner has read the design; do
not write it in the same session.

The facts embedded below were read on 2026-08-04 at `67966fd`; the prompt
instructs the agent to re-read rather than trust them. The three decisions in
*Decisions already made* were taken by the project owner on 2026-08-04. They
are not open. An agent that re-opens them is wasting the session.

---

You are working in the **Species** repository, a ~113k-line C++20 Windows game
(six MSBuild projects, one header-only dependency) partway through being
modernised from the inherited Darwinia source into Neuron-style C++20 with
enforced layer boundaries. `AGENTS.md` states the long-term goal: *a
large-scale realtime multiplayer world in which player colonies work, live and
survive persistently.* Nothing in the tree is designed for that yet. This
session designs it.

## Mission

Produce a **detailed analysis of the current Location implementation and a
design for scaling it to a procedurally generated open world addressed by
int64×int64 world-units**, in which:

- **buildings are constructed by players** at runtime, not placed by designers
  in level files, and
- **entities are user-generated or system-generated** — consequences of player
  action, or of the world's own procedural ecology — not spawned from authored
  `InstantUnits` lists.

The deliverable is `docs/OPEN_WORLD.md` (split into `docs/OPEN_WORLD_<AREA>.md`
files if one file becomes unwieldy — the main file then carries the map).
Analysis first, design second, in the same document set: the design must be
argued *from* the analysis, not beside it.

This session is **design only**. You write, commit and report documents. You do
not change code, you do not write task plans, and you do not start
implementation — `AGENTS.md` places world/persistence systems out of
implementation scope until the modernisation lands, and this document is how
that future work gets scoped, not how it gets started.

## Decisions already made

Record these in the document so readers inherit them. Do not re-ask.

1. **The unit is today's world-unit; the address space is int64×int64.** A
   world coordinate is a pair of signed 64-bit integers in the same unit the
   current maps use (The Garden is ~2,002 of them across). The space is
   addressable, not fillable: design for sparseness. (Owner, 2026-08-04.)
2. **The end state is the persistent multiplayer world.** Authoritative
   server, streaming clients, persistence — the `AGENTS.md` ambition, not a
   larger single-player map. Design for that end state; phase it explicitly if
   you need intermediate milestones. (Owner, 2026-08-04.)
3. **The fate of the authored maps is yours to decide.** Whether The Garden
   and the campaign maps are stamped into the procedural world as authored
   set-pieces, kept as a separate legacy mode, or mined for mechanics and
   retired — analyse, recommend, and put the question to the owner with your
   recommendation (Step 4). (Owner, 2026-08-04.)

Decisions already recorded in the tree that this design builds on rather than
reopens:

- **Deterministic lockstep will not scale and is to be replaced.**
  `docs/ARCHITECTURE.md` *Runtime model* says so in terms: every client
  simulates the whole world, which is incompatible with the target. Replacing
  it is this design's job, not a risk to it.
- **Object identity becomes a stable generated id when the protocol changes**
  (owner, 2026-08-02, recorded in `docs/ARCHITECTURE.md`): a slot index is
  reused and can alias; a deterministically allocated id cannot, and it frees
  the container choice. Your identity design starts from that decision.
- **Mixed-architecture lockstep is heading to "not supported"** (owner,
  2026-08-03): DirectXMath dispatches to SSE on x64 and NEON on ARM64 and the
  scalar path was declined. Under a server-authoritative model the question
  changes shape — your design must state exactly which computations still
  require bit-identical results on which machines, because that decides
  whether clients may generate terrain locally (Step 2, area C).

## The constraints that survive

- **No code.** The current priority (`AGENTS.md`) is modernisation; nothing in
  this design licenses implementation, and the document must say so on its
  first page.
- **The determinism rules still govern the tree you are analysing.** Read
  `CODING_STANDARDS.md#determinism` before claiming anything about what may
  change; where your design relaxes a constraint (and replacing lockstep
  relaxes several), name the constraint and say what replaces it.
- **Frozen names stay frozen.** New systems get new names; existing
  Darwinia-derived names follow `CODING_STANDARDS.md#renaming-away-from-darwinia`
  and are not renamed by a design document.
- **Honesty.** You are almost certainly on Linux: you cannot build or run the
  game. Capacity figures are arithmetic, not measurements — label them so.

## Step 0 — read before designing

In this order, completely:

1. `AGENTS.md` — priority, scope, known issues, decisions taken and declined.
2. `docs/ARCHITECTURE.md` — the layers and especially *Runtime model*.
3. `docs/GLOSSARY.md` — the domain vocabulary; this design must speak it.
4. `CODING_STANDARDS.md` — determinism above all.
5. The world model, in source: `GameLogic/Location.h/.cpp`,
   `GlobalWorld.h/.cpp`, `LevelFile.h/.cpp`, `Landscape.h/.cpp`,
   `LandscapeRenderer.h`, `EntityGrid.h`, `ObstructionGrid.h`, `Team.h`,
   `Unit.h`, `Entity.h`, `Building.h`, `Spirit.h`, `RoutingSystem.h`,
   `NeuronCore/WorldObjectId.h`, `NeuronCore/NetworkUpdate.h`,
   `NeuronCore/ProtocolLimits.h`, `NeuronServer/Server.h`.
6. One authored map end to end: `GameData/Levels/MapGarden.txt` +
   `MissionGardenLiberate.txt`, against `LevelFile::ParseMapFile`.
7. `tasks/_modernization-prompt.md` and `docs/TASK_DAG.md` — the standard the
   eventual planning prompt will hold this design to.

## Step 1 — analyse the current implementation

The analysis section must let a reader who has never opened `Location.cpp`
understand what exists and why it cannot scale. Facts read at `67966fd`, with
pointers — **re-verify each by reading; the tree moves**:

| Fact | Where |
|---|---|
| A Location is one self-contained map, torn down whole and rebuilt from two text files (map + mission) on entry | `Location::Init`, `Location::Empty`, `LevelFile` |
| The Garden is 2,002×2,002 world-units at `cellSize 10.66` (~188×188 height samples); defaults are 2,000×2,000 at 12 | `GameData/Levels/MapGarden.txt`, `LevelFile.h` `LandscapeDef` |
| Terrain is a single merged `SurfaceMap2D<float>` heightmap + normal map, generated at load by seeded diamond-square tiles, then flattened under buildings | `LandscapeTile::Generate`, `Landscape::GenerateHeightMap`, `LandscapeFlattenArea` |
| The heightmap container lives in the presentation layer | `NeuronClient/2dSurfaceMap.h` |
| Positions are 32-bit float `Vector3` throughout (mid-migration to `XMFLOAT3`) | `NeuronCore/Vector3.h`, `tasks/Archive/directxmath-migration.yaml` |
| Spatial indexes are whole-map allocations: `EntityGrid` at 8×8 units *per team*, `ObstructionGrid` at 64×64 | `Location.cpp` (`Init`), `EntityGrid.h`, `ObstructionGrid.h` |
| Simulation is `Location::Advance` — 10 Hz ticks × 10 slices, five categories, one shared LCG whose call sequence is load-bearing, a sync checksum over every position and velocity | `Location.cpp`, `SliceWalker`, `speciesRandom`, `GenerateSyncValue` |
| The `#pragma omp parallel for` across the five Advance categories is **inert** — no project enables OpenMP — and would race the shared LCG if it ever ran | `Location::Advance`, absence of `<OpenMPSupport>` in any `.vcxproj` |
| Identity is `{teamId, unitId, slot index, uniqueId}` with the slot index serialised on the wire | `NeuronCore/WorldObjectId.h` |
| The wire vocabulary is 13 fixed-size update types; **none places a building** | `NeuronCore/NetworkUpdate.h` |
| Buildings: 57 concrete types, int ids per location, placed only by the editor into level files; `m_dynamic` marks per-mission ones; several link to each other by id | `Building.h`, `LevelFile::ParseBuildings` |
| Entities: 18 concrete types; spawned at load from `InstantUnits`, at runtime by buildings (Incubator, SpawnPoint, Factory, AntHill, Triffid, Egg + Spirit) and by player programs through the Task Manager | `Entity.h`, `LevelFile.h`, `TaskManager` |
| Teams are a hard protocol constant, 4, woven into grids, ports and the wire | `NeuronCore/ProtocolLimits.h` `NUM_TEAMS` |
| The campaign layer is discrete `GlobalLocation`s on a rendered sphere, linked by trunk ports, with spirits transferring between them and a per-profile text save | `GlobalWorld.h`, `SphereWorld`, `TransferSpirits` |
| Persistence today is authored text files plus the campaign save; no runtime world state is stored | `LevelFile::Save`, `GlobalWorld::SaveGame` |

Arithmetic the analysis should carry (as arithmetic, clearly labelled):

- A 32-bit float holds unit-exact integers to 2²⁴ ≈ 16.8M units — about 8,400
  Gardens from the origin, or ~10⁻¹² of the int64 axis. A double reaches 2⁵³ ≈
  9.0×10¹⁵, still under 0.05% of the axis. **Integer global coordinates are
  forced**; float math survives only in region-local frames.
- Materialising the world at today's cell size would be ~3×10³⁶ height samples.
  **The world is a function, not a file**: generation on demand, persistence
  of deltas only.

Beyond confirming the table, the analysis must:

- Produce a **member-by-member disposition of `Location`** (and the same for
  `GlobalWorld` and `LevelFile`): for each field and responsibility, does it
  become per-chunk, per-region, per-world, per-client, or retired? This table
  is the spine of the migration story.
- Trace **every assumption of location-scale coordinates** you can find —
  camera, water and cloud extents, fog, `GroundHeight(float,float)` in
  `LocationAccess`, `GlobalWorld::LocationHit`'s 5,000-unit radius, routing
  waypoints, `m_spawnPoint`/`m_roamRange` on entities — the design has to
  answer each, and the ones you miss become surprises later.
- Record the two known RNG-discipline defects (`SoundInstance` and
  `LandscapeRenderer::GetLandscapeColour`, both in `AGENTS.md` *Known
  issues*) as the cautionary tale your generation design must not repeat, and
  the unexplained landscape-shape change (`directxmath-migration` T13) as
  evidence that float-based generation is fragile across builds — let alone
  architectures.

## Step 2 — the design areas

Cover all of these. For each: the problem in one paragraph, the options
considered, a recommendation with consequences, and what it demands of the
areas it touches. Where an area turns on an unanswered owner question, say so
and point at Step 4.

**A. Coordinates and precision.** The global coordinate type (int64 pair; or
chunk id + intra-chunk offset — pick one canonical form and define
conversions); region-local float frames for simulation and rendering; origin
rebasing or camera-relative rendering; what goes on the wire
(region-relative, presumably); the rule that no absolute world position ever
enters float arithmetic. State how this coexists with the in-flight
DirectXMath migration rather than fighting it.

**B. The world model.** What replaces `Location` and `GlobalWorld`: chunks
(generation and persistence granularity) vs regions (simulation and interest
granularity) — define both, their sizes (justified with arithmetic: memory per
chunk, entities per region, grid storage), their lifecycles (generate → load →
activate → deactivate → persist → evict), and which of today's subsystems
(grids, walkers, water, clouds, lights) becomes per-chunk, per-region or an
environment layer. Include where the heightmap container must live for a
headless server to use it (it is in `NeuronClient` today).

**C. Procedural generation.** Deterministic generation as a pure function of
`(worldSeed, chunkCoord, layer)`. The chunk-border continuity problem — the
current diamond-square tiles need whole-tile context and do not tile; the
candidate answers (lattice noise that is naturally local, overlapping
generation windows, coarse-to-fine LOD cascades for continents → biomes →
local detail) and a recommendation. Biome and point-of-interest placement by
deterministic sparse scattering. **The reproducibility decision**: if clients
generate terrain locally, generation must be bit-identical across ARM64/x64 —
which float noise is not guaranteed to be — so either generation is
integer/fixed-point arithmetic, or the server ships generated chunks and
clients merely cache. Recommend one; this couples to bandwidth (area I) and
cheating posture (area F). RNG discipline as a design rule: generation streams
keyed by position and layer, never the simulation stream. Where authored
content fits per your Decision-3 recommendation (the level-file format as a
prefab/stamp format is one candidate). Terrain mutability beyond
flatten-under-buildings is an owner question (Step 4) — design the delta
representation so the answer can change later without a new store format.

**D. Persistence.** The baseline is regenerable, so the store holds deltas
only: player buildings, terrain modifications, entity populations, claims,
research, the spirit economy. Per-chunk delta records keyed by chunk
coordinate; snapshot vs event-log vs hybrid, compaction, versioning/format
evolution, crash consistency; what of it the client may cache. World-scale
identity allocation (64-bit ids; per-region ranges or `(region, counter)`
pairs — align with the recorded stable-id decision). Today's text formats are
authoring tools, not a store — say what the store is instead, and note that
anything beyond OS APIs and header-only code is a dependency question for the
owner (Step 4).

**E. Simulation at scale.** Simulation exists only where it matters: active
regions around players (and around whatever else the design decides deserves
one — contested colonies, active virus fronts). Dormant regions: frozen state
plus catch-up-on-wake rules, per system — production, population growth and
decay, virus spread as a coarse model over the region graph — with explicit
statements of what is closed-form, what is stepped, and what is simply not
simulated while dormant. Entity and projectile behaviour at region seams.
What survives of the slice machinery as per-region tick budgets. What
determinism is still required and why (per-region reproducibility for replay
and debugging is worth wanting; cross-client lockstep is gone). Offline
progression is an owner question (Step 4); design the catch-up machinery so
either answer fits.

**F. Server topology, interest management, protocol.** From one host
sequencing lockstep intents to an authoritative simulation: whether one
process schedules active regions first, and how the design grows to multiple
region hosts (assignment, handoff, the seam a colony straddles); client
interest by region subscription + radius; replication (per-tick deltas to
subscribers, client prediction); what replaces the 42-byte fixed packet
vocabulary — including the messages that do not exist today: place-building,
demolish, claim, terrain-edit, chunk request/response. Validation moves to
the server — lockstep trusted every client by construction, an authoritative
world trusts none. Ownership beyond `NUM_TEAMS 4`: players, colonies,
alliances, and the system factions (virus, wildlife) — and what that does to
every per-team array (`EntityGrid` holds one cell array per team today).

**G. Player-built buildings.** The full loop, named end to end: blueprint
availability (the BlueprintStore/Console/Relay chain and `GlobalResearch` are
the existing fiction to build on) → client-side placement preview → placement
intent on the wire → server validation (terrain slope and water, obstruction
against the per-chunk grid, resource cost, claim permission) → construction
as gameplay (instant vs staged; ports and Citizen labour — the
ConstructionYard mechanic — as the existing vocabulary for "being built") →
terrain flattening as a persistent delta → the building joining the per-chunk
obstruction grid and persistence record → linked networks (pylons, fences,
tracks link by int id today; ids must survive at world scale) → damage,
decay, demolition, refund. Ownership and permissions: who may build where,
what a claim is (SafeArea is a seed of the idea), and the griefing surface.
Produce a **disposition table over all 57 building types**: player-buildable
/ system-generated (world gen or virus) / authored-only / retired, with the
open questions each class raises.

**H. Entities: user-generated and system-generated.** Define both terms
against the existing spawn paths: user-generated — spawned as a consequence
of player action (Task Manager programs, Incubators, SpawnPoints, promotion,
armour) — and system-generated — spawned by the world itself (biome-keyed
ecology, virus nests as generated structures with AntHill/Triffid/
SporeGenerator as their organs, ambient wildlife). Population governance:
per-region budgets and caps (`SpawnPopulationLock` exists as prior art),
what the population does while dormant (ties to E), and what bounds
system-generated pressure so the world stays survivable. The spirit economy
at world scale — spirits are the conserved currency between death and birth;
today they even transfer between locations (`TransferSpirits`) — decide
whether conservation is global, regional, or abandoned. AI at scale: per-map
`AITarget`/`AISpawnPoint` hints become what? Pathfinding: per-location
routes and the obstruction grid become per-chunk navigation with
hierarchical routing across chunk borders. Entity identity across region
handoff (ties to D and F). Produce the same **disposition table over the 18
entity types**.

**I. Client streaming and rendering.** Chunk acquisition (generate locally
vs fetch — the C decision), cache and eviction; the landscape renderer from
one whole-map display list to per-chunk meshes with LOD and seam stitching,
a far-field/horizon answer, and water and clouds unbounded; origin rebasing
at the camera; the world map view that replaces the sphere (`SphereWorld`,
`GlobalInternet` — the campaign UI) driven from coarse generation layers;
what `GroundHeight` and the other `LocationAccess` queries resolve through
when the ground under a sound might not be resident. Name what this design
deliberately does *not* redesign (the renderer's internals, the sound
system, Eclipse, entity behaviour state machines) so the scope has edges.

**J. The migration path.** Not a task DAG — a phasing argument: the ordered
list of separable milestones from "modernisation finishes" to "persistent
world", each with what it proves and what it deliberately fakes (e.g. a
single-region world behind the new coordinate types; a local server
process; persistence before multiplayer, or after — argue it). State
explicitly what the current modernisation should preserve to keep this
design cheap — the narrow `SlotMap` handle API is the recorded exemplar;
identify the equivalents this design needs (seams around `LevelFile`
parsing, `LocationAccess` as the world-query interface, the heightmap
container's layer, not spreading location-scale float signatures further).
These become notes for the open plans, not new tasks.

## Step 3 — how to shape the documents

- **Analysis before design, argument before conclusion.** Every design claim
  about current code carries a `file:line`-style pointer a reader can follow.
- **Decision records.** Each significant choice gets: the options, the
  recommendation, the consequences, and what would reopen it. Match the tone
  of the decisions recorded in `AGENTS.md` — dated, owned, revisitable.
- **Numbers first.** Chunk sizes, memory per resident chunk, entities per
  active region, deltas per colony, bandwidth per subscribed client —
  arithmetic with stated assumptions beats adjectives. Label estimates as
  estimates.
- **Plain diagrams.** ASCII layer/flow diagrams in the style of
  `docs/ARCHITECTURE.md` are welcome; no external tooling.
- **Speak the glossary.** Citizen, Spirit, Officer, Trunk Port — reuse the
  domain vocabulary; invent new terms only where a concept is genuinely new,
  and define them in one place.
- **The document must stand alone.** A reader with `AGENTS.md` and this
  document — and neither this prompt nor the chat — gets the whole design,
  including the decisions already made and the questions still open.

## Step 4 — ask before you finalise

Answer from the tree what the tree can answer, and cite where. Ask the owner
what only the owner can. Use `AskUserQuestion` if available; otherwise write
an **Open questions** section with your recommendation beside each question.
At minimum:

1. **Authored maps** — your Decision-3 recommendation: stamp, legacy mode, or
   retire?
2. **Scale targets** — concurrent players per world, colonies per player,
   expected density. The answer bounds region size, server topology and the
   persistence budget; design to a stated assumption if unanswered, and mark
   it.
3. **Terrain mutability** — flatten-under-buildings only, or player
   terraforming? (Changes the delta format, the netcode and the griefing
   surface.)
4. **Client generation vs server-shipped chunks** — your recommendation from
   area C, as a question, because it decides the cross-architecture
   reproducibility requirement and the cheating posture.
5. **Offline progression** — do colonies live, produce and die while their
   player is away?
6. **Dependency posture** — the tree links only against the OS with one
   header-only dependency; is a storage engine or serialisation library
   acceptable, or does persistence stay hand-rolled? (Build topology: ask,
   never assume.)
7. **One world or many** — a single shared world, per-group worlds, or both?
8. **PvP and claims** — may players harm each other's colonies, and what
   protects a claim? (Drives G's permission model and F's validation.)

## Step 5 — deliverables

1. `docs/OPEN_WORLD.md` (plus `docs/OPEN_WORLD_<AREA>.md` splits if needed):
   the analysis, the design areas A–J, the decision records, the disposition
   tables (Location members, 57 building types, 18 entity types), the open
   questions with recommendations, and the phasing argument.
2. A closing report in chat: the shape of the design in ten sentences, the
   recommendations made, the questions asked and what you assumed pending
   answers, and anything the analysis turned up that changes the picture —
   including anything worth adding to `AGENTS.md` *Known issues*.
3. Commit the documents (and nothing else) to your working branch with a
   clear message. Do not push to `main`.

Honesty rules apply throughout: report what you actually read; arithmetic is
not measurement; on Linux you cannot build or run the game — say so rather
than implying you checked.
