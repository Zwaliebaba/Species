# Prompt: analyse and design the 65,536 × 65,536 Location

Copy everything below the rule into a fresh agent session on this repository
(branch of your choice). It produces **design documents under `docs/`** — not
code changes, and not task-DAG files. A planning prompt in the style of
`_modernization-prompt.md` follows *after* the owner has read the design; do
not write it in the same session.

The facts embedded below were read on 2026-08-06 at `086e239`. The prompt
instructs the agent to re-verify each one rather than trust it. The decisions in
*Decisions already made* were taken by the project owner on 2026-08-06. They are
not open. An agent that re-opens them is wasting the session.

This prompt is a **sibling of [`_openworld-prompt.md`](_openworld-prompt.md)**, not a
replacement for it. That prompt designs the int64×int64 procedurally generated
open world; this one designs the first milestone on the way there — one
Location, finite, 65,536 units on a side, that a person can load and play at
today's frame rate. Read both before starting.

---

You are working in the **Species** repository, a ~113k-line C++23 Windows game
(six MSBuild projects, one header-only dependency) whose planned modernisation
finished on 2026-08-05. `AGENTS.md` states the long-term goal: *a large-scale
realtime multiplayer world in which player colonies work, live and survive
persistently.* The largest map that exists today is 5,372 world units across.
This session designs the step from there to 65,536.

## Mission

Produce a **detailed analysis of every part of the tree whose cost scales with
Location area, and a design for raising the Location to 65,536 × 65,536 world
units** — playable, editable, and at no worse frame rate, memory footprint or
load time than The Garden delivers today for the same visible area.

The deliverable is `docs/LARGE_LOCATION.md` (split into
`docs/LARGE_LOCATION_<AREA>.md` files if one file becomes unwieldy — the main
file then carries the map). Analysis first, design second, in the same document
set: the design must be argued *from* the analysis, not beside it.

This session is **design only**. You write, commit and report documents. You do
not change code and you do not write task plans. `AGENTS.md` requires that
anything larger than a single-file change be a validated DAG under `tasks/`
before code is written, and this change is unavoidably tree-wide; the plan is
written from your document, in a later session, once the owner has read it.

## Decisions already made

Record these in the document so readers inherit them. Do not re-ask.

1. **The target is 65,536 × 65,536 world units at today's cell size.** The unit
   is the world unit the current maps use — The Garden is 2,002 of them across
   at `cellSize 10.66`, and `LandscapeDef` defaults to 2,000 at 12. `cellSize`
   does not change: terrain sample density stays what it is, and the world gets
   ~32× longer per axis than The Garden and ~1,073× larger in area.
   (Owner, 2026-08-06.)
2. **This is a milestone of the open world, not a detour around it.** Whatever
   chunk and region boundaries this design introduces must be usable by the
   int64×int64 design in [`_openworld-prompt.md`](_openworld-prompt.md), whose areas
   B (world model) and F (server topology, interest management) define the same
   seams at full scale. Where the two designs could diverge, say which one you
   are conforming to and why. (Owner, 2026-08-06.)
3. **The performance bar is measured against The Garden, and streaming is
   allowed to hit it.** See *The performance bar* below — it is the governing
   constraint of this design, not a section of it. Chunked residency, level of
   detail, per-chunk grids and per-chunk generation are all explicitly in
   scope, in the game **and in the editor**. (Owner, 2026-08-06.)
4. **The server does not become authoritative first.** This was asked and
   answered on 2026-08-06. Almost nothing 65,536² breaks is about who owns the
   simulation: the heightmap, the normal map, the landscape renderer, terrain
   generation and the whole editor are client-side and per-machine, and the two
   structures that are simulation-side — `EntityGrid` and `ObstructionGrid` —
   cost the same on a server as on a client unless they are made sparse first.
   Chunking is the fix; server authority only decides who holds which chunks.
   Entity counts do not grow because the map did — The Garden holds ~230
   entities in 2,002² and a 65,536² map is mostly empty — so the load that
   explodes here is area-proportional, not agent-proportional, and is not the
   load deterministic lockstep cannot carry.
   - **You must still re-argue this in the document, in a decision record**,
     because it is the decision most likely to be wrong for a reason the
     analysis turns up. If your analysis finds an area-scaling cost that only
     an authoritative server can remove, say so plainly and put it to the owner
     (Step 5) rather than quietly designing around it.

Decisions already recorded in the tree that this design builds on rather than
reopens:

- **Deterministic lockstep will not scale and is to be replaced.**
  `docs/ARCHITECTURE.md` *Runtime model* says so in terms. Replacing it is the
  open-world design's job. **This design must keep working under lockstep**, so
  every structure you make chunked or streamed must either stay bit-identical
  across clients or be provably outside `GenerateSyncValue`'s reach — see
  *The determinism trap* below, which is the sharpest hazard in this work.
- **Mixed-architecture play is not supported** (owner, 2026-08-03). Within one
  architecture the simulation must stay deterministic, and that property is what
  the sync assert in `Server.cpp` tests.
- **Object identity becomes a stable generated id when the protocol changes**
  (owner, 2026-08-02). If this design needs identity that survives a chunk
  eviction, it starts from that decision rather than inventing another.

## The constraints that survive

- **No code, no task plans.** The document must say so on its first page.
- **The determinism rules govern everything you are analysing.** Read
  `CODING_STANDARDS.md#determinism` before claiming anything may change.
- **Frozen names stay frozen.** New systems get new names; existing
  Darwinia-derived names follow `CODING_STANDARDS.md#renaming` and are not
  renamed by a design document.
- **Layering is enforced with no escape hatch.** `tools/check_layering.py`
  rejects any upward include tree-wide. The heightmap container
  (`SurfaceMap2D`) lives in `NeuronClient` today and the world model lives in
  `GameLogic`; if your design needs the heightmap below the renderer, say which
  layer it moves to and why that direction is downward.
- **Honesty.** You are almost certainly on Linux and cannot build or run the
  game. Every capacity figure in this document is arithmetic, not measurement —
  label it so. `sizeof` figures are assumptions about the MSVC x64 ABI unless
  you have verified them; say which.

## Step 0 — read before designing

In this order, completely:

1. `AGENTS.md` — priority, scope, known issues, decisions taken and declined.
2. `docs/ARCHITECTURE.md` — the layers, and especially *Runtime model*.
3. `CODING_STANDARDS.md` — determinism above all.
4. `tasks/_openworld-prompt.md` — the design this one is a milestone of.
5. `docs/GLOSSARY.md` — the domain vocabulary; this design must speak it.
6. The area-scaling code, in source: `GameLogic/Landscape.h/.cpp`,
   `GameLogic/LandscapeRenderer.h/.cpp`, `GameLogic/LevelFile.h/.cpp`,
   `GameLogic/Location.h/.cpp`, `GameLogic/EntityGrid.h/.cpp`,
   `GameLogic/ObstructionGrid.h/.cpp`, `GameLogic/Water.h/.cpp`,
   `GameLogic/Clouds.cpp`, `GameLogic/RoutingSystem.h`,
   `GameLogic/LandscapeWindow.cpp`, `NeuronCore/2dArray.h`,
   `NeuronClient/2dSurfaceMap.h`.
7. The editor, in source: `Species/LocationEditor.h/.cpp`,
   `GameLogic/EditorWindow.cpp`, and every `*Window.cpp` in `GameLogic` that
   edits landscape, tiles, buildings or instant units.
8. One authored map end to end: `GameData/Levels/MapGarden.txt` +
   `MissionGardenLiberate.txt`, against `LevelFile::ParseMapFile`.
9. `docs/TESTING.md` and `docs/TASK_DAG.md` — the standards the eventual
   planning prompt will hold this design to.

## Step 1 — analyse what scales with area

The analysis must let a reader who has never opened `Landscape.cpp` understand
exactly what breaks and by how much. Facts read at `086e239`, with pointers —
**re-verify each by reading; the tree moves, and two of these are arithmetic
that assumes a `sizeof` you should confirm**:

| Fact | Where |
|---|---|
| The Garden is 2,002 × 2,002 world units at `cellSize 10.66`; `LandscapeDef` defaults to 2,000 × 2,000 at 12; the largest authored map in the tree is 5,372 | `GameData/Levels/MapGarden.txt`, `LevelFile.h` `LandscapeDef`, `GameData/Levels/Map*.txt` |
| The heightmap and normal map are two whole-world `SurfaceMap2D`s allocated from `worldSize / cellSize` | `Landscape.cpp:552-553` |
| `Array2D` stores its dimensions as **`unsigned short`** and every accessor takes `unsigned short` indices | `NeuronCore/2dArray.h:28-47` |
| `SurfaceMap2D::GetValue` truncates the interpolation indices to `unsigned short` and wraps out-of-range indices **to zero** rather than clamping | `NeuronClient/2dSurfaceMap.h:117-120, 175-178` |
| `EntityGrid` allocates one flat `EntityGridCell[numCellsX * numCellsZ]` **per team**, at 8 × 8 world units | `Location.cpp:131`, `EntityGrid.cpp:184-193` |
| `ObstructionGrid` allocates a whole-world `SurfaceMap2D<ObstructionGridCell>` at 64 × 64, where each cell holds a `std::vector<int>`, and `CalculateAll()` runs at construction | `Location.cpp:132`, `ObstructionGrid.cpp:17-26`, `ObstructionGrid.h` |
| `LandscapeRenderer` builds **one** vertex array for the whole map as a single triangle strip — no LOD, no culling, no chunking — emitting up to two `LandVertex` per heightmap cell, and only skipping quads entirely below water | `LandscapeRenderer.cpp` `BuildVertArrayAndTriStrip` |
| `LandVertex` is `XMFLOAT3` + `XMFLOAT3` + `RGBAColour` + `TextureUV` = 12 + 12 + 4 + 8 = **36 bytes** | `LandscapeRenderer.h`, `RgbColour.h`, `TextureUv.h` |
| A `LandscapeTile` generates its own heightmap at a **power of two plus one** ≥ its cell span, at `cellSize` 1.0, by diamond-square — then applies a height shift in an **unconditional** loop over every sample | `Landscape.cpp` `LandscapeTile::Generate` |
| `MergeTileIntoLandscape` walks the tile with **`unsigned short`** loop counters and computes the tile origin as an `unsigned short` cell index | `Landscape.cpp` `MergeTileIntoLandscape` |
| Terrain is authored as a **list of tiles in a text file**; The Garden uses 7 tiles of size 564–884 to cover 2,002² | `GameData/Levels/MapGarden.txt` `LandscapeTiles_StartDefinition`, `LevelFile::ParseLandscapeTiles` |
| `Landscape::GenerateNormals` and `GetHighestValue` walk every sample | `Landscape.cpp`, `2dSurfaceMap.h` |
| `Water`'s cell size is derived as `detail * worldSize / 100`, so its polygon count is constant but its tessellation coarsens with the world; its light map is a fixed 128 × 128 mask over the whole world | `Water.cpp:57-88, 113-120` |
| Inside a location the far plane is **15,000** — the 65,536² world diagonal is 92,682 | `Species/Renderer.cpp:355` |
| The editor's world-size controls already permit up to **1e6**, and `cellSize` down to **1.0** | `LandscapeWindow.cpp:407-409` |
| `ScaleLandscapeButton` multiplies world size, every tile, every building and every instant unit by 1.05 and then calls `Landscape::Init` — a full regeneration per click | `LandscapeWindow.cpp:290-330` |
| `Location::Init` skips `EntityGrid`, `ObstructionGrid` and `Clouds` entirely when `g_editing` | `Location.cpp:126-136` |
| `GenerateSyncValue` sums entity positions and velocities in container index order; it is entity-count bound, not area bound | `Species/Main.cpp:233`, `docs/ARCHITECTURE.md` *Runtime model* |
| Entities clamp themselves to `GetWorldSizeX/Z()` in several places | `Entity.cpp:426-435`, `Airstrike.cpp:261-264`, `InsertionSquad.cpp:570-579`, `Lander.cpp:54-63` |

### Arithmetic the analysis must carry, as arithmetic, clearly labelled

Reproduce and check these; they are the numbers the whole design answers to.
Each assumes `cellSize` 12 (the `LandscapeDef` default) unless stated, and MSVC
x64 layout where a `sizeof` is involved.

**Terrain samples.** 65,536 / 12 = 5,461.33, and `SurfaceMap2D` takes
`ceilf`, so 5,462 samples per axis = 29,833,444 samples. The Garden is
2,002 / 10.66 = 187.8 → 188 per axis = 35,344 samples. **844× the samples.**

| | Garden | 65,536² | Ratio |
|---|---|---|---|
| Height samples | 35,344 | 29,833,444 | 844× |
| Heightmap (`float`) | 141 KB | **119 MB** | 844× |
| Normal map (`XMFLOAT3`) | 424 KB | **358 MB** | 844× |

At The Garden's own `cellSize` of 10.66 it is 6,149 per axis = 37.8M samples,
151 MB + 454 MB. State which cell size your design assumes and why.

**The `unsigned short` ceiling is real and reachable.** `Array2D` can address
65,535 cells per axis. At `cellSize` 12 the world needs 5,462 — comfortable.
But the editor permits `cellSize` down to 1.0 (`LandscapeWindow.cpp:407`), and
at 1.0 the world needs **65,536** cells, which is exactly one past the ceiling
and converts to `unsigned short` as **zero**. Work out precisely which
`(worldSize, cellSize)` pairs the current types can express, and say whether the
design widens the index type, clamps the editor, or both.

**`EntityGrid` is the single largest number in this design.** `int(65536 / 8) +
1` = 8,193 cells per axis = 67,125,249 cells. `EntityGridCell` holds two `int`s,
two pointers and an `int` — 32 bytes with padding, *verify this*. That is
**2.15 GB per team**, and it is allocated `NUM_TEAMS` = 4 times:

| | Garden | 65,536² | Ratio |
|---|---|---|---|
| Cells per axis | 251 | 8,193 | 33× |
| Total cells | 63,001 | 67,125,249 | 1,065× |
| All four teams | 8.1 MB | **8.6 GB** | 1,065× |

**`ObstructionGrid`**: 65,536 / 64 = 1,024 per axis = 1,048,576 cells, each an
empty `std::vector<int>` (24 bytes on MSVC — verify) = **25 MB**, against 24 KB
for The Garden.

**The renderer.** Worst case two 36-byte verts per heightmap cell:
2 × 29,833,444 × 36 = **2.15 GB** of vertex data in one strip, ~59.7M triangles
in one draw call, plus the same again in the VBO. Quads entirely below water are
skipped, so an ocean-heavy map emits far fewer — quantify that mitigation
against a Garden-like land fraction rather than assuming it saves you.

**Terrain generation.** A single tile spanning the world at `cellSize` 12 needs
5,461 cells, so `GetPowerOfTwo(5460)` = 13 and the tile heightmap is
8,193 × 8,193 = 67.1M floats = **268 MB**, generated by ~67M diamond-square
midpoint computations each drawing from `speciesRandom`, then walked again by
the unconditional height-shift loop. Meanwhile `MergeTileIntoLandscape`'s
`unsigned short` loop counter caps a single tile at 65,535 cells — at
`cellSize` 1.0 a world-spanning tile is 65,536 cells and **the loop never
terminates**. Confirm that reading.

**Authoring at this size is the argument for generation.** The Garden covers
2,002² with 7 tiles. Holding that tile density constant, 65,536² needs
7 × (65,536 / 2,002)² ≈ **7,500 tile lines** in the map text file, each
generating its own power-of-two heightmap and being merged. State plainly
whether the tile list survives at this scale or whether terrain becomes a
function of position — and if it becomes a function, conform to
`_openworld-prompt.md` area C rather than inventing a second scheme.

**Load time.** 844× the samples through generation, normal generation, colour
and UV array building. You cannot measure this on Linux. Express it as a
multiplier on whatever the owner measures for The Garden, and say what the
multiplier would have to fall to for the load budget to be met.

**Float precision.** 65,536 is 2¹⁶ exactly, well inside a `float`'s
unit-exact range of 2²⁴, so the coordinate range itself is safe. But the ULP at
x ≈ 65,536 is 2⁻⁷ ≈ 0.0078 units against 2⁻¹³ ≈ 0.00012 at Garden scale — a
64× coarser position resolution at the far corner. Entity radii are 1–10 units
so this is not a collision problem; say whether it is a `GenerateSyncValue`
problem, and show the working.

### Beyond the table

- Produce a **member-by-member disposition of `Location`, `Landscape` and
  `LevelFile`**: for each field, does it stay whole-world, become per-chunk,
  become per-view, or get retired? This table is the spine of the migration
  story and the thing the eventual task DAG is written from.
- **Sweep for every whole-world allocation and every whole-world loop**, not
  just the ones tabulated. Grep for `GetWorldSizeX`, `GetWorldSizeZ`,
  `GetNumColumns`, `GetNumRows` and every `new T[` sized from them, and read
  each hit. The recurring finding recorded in `tasks/_next-batch.md` is that a
  file list written from where a thing is *declared* misses most of where it is
  *named* — eight of nine such lists measured against this tree were wrong.
- **Trace every constant that assumes a small map**: the 15,000 far plane, fog,
  `Water`'s 128 × 128 light mask and `detail * worldSize / 100` cell size,
  `Clouds`, `Landscape::SphereHit`'s `_radius < 200.0f` assertion, camera
  limits, `AITarget`/`AISpawnPoint` placement, `m_spawnPoint`/`m_roamRange` on
  entities, routing waypoints, and the entity world-clamp sites. Each needs an
  answer; the ones you miss become surprises later.
- **Give the editor equal weight to the game.** `g_editing` already skips the
  two grids, so the editor's problems are different ones: whole-map
  regeneration on a scale click, per-frame rendering of every tile and flatten
  area, mouse picking through `Landscape::RayHit`, and the tile list as a
  usable authoring surface at ~7,500 entries. An editor that cannot open the
  map is a failed deliverable, not a follow-up.

### The determinism trap

Give this its own section. It is the way this work is most likely to ship a bug
no build catches.

The simulation is deterministic lockstep and stays so through this change. Every
structure you make lazily resident introduces the same hazard: **a value that
depends on whether a chunk happened to be loaded is a value that differs between
clients.** `GenerateSyncValue` sums entity positions and velocities in container
index order, so anything reaching a position reaches the sync value.

Concretely, the design must state for each streamed thing whether it is
simulation state or presentation state, and what makes that true:

- If terrain generation becomes per-chunk, two clients that generate the same
  chunk in a different order must get identical heights. Today
  `LandscapeTile::Generate` calls `speciesSeedRandom(m_randomSeed)` per tile and
  then draws from `speciesRandom` — the **cosmetic LCG**, not the lockstep
  Mersenne Twister. That is safe only because every client generates every tile
  at load. Under lazy generation it stops being safe unless generation is a pure
  function of `(seed, chunkCoord)` and nothing else. `AGENTS.md` records six
  real desyncs from simulation state drawn off the cosmetic stream
  (`tasks/Archive/determinism.yaml` T5) — do not add a seventh.
- If `EntityGrid` becomes sparse, cell allocation order must not affect
  neighbour query *ordering*, because AI target selection reads it and target
  selection reaches positions.
- `SurfaceMap2D::GetValue` wraps out-of-range lookups to index zero rather than
  clamping. Under whole-world residency that path is nearly unreachable; under
  partial residency it is a live, silent, client-dependent wrong answer.
- Record `AGENTS.md`'s landscape-shape observation from
  `directxmath-migration` T13 as evidence that float-based terrain generation is
  fragile across builds, let alone across residency policies.

## Step 2 — the design areas

Cover all of these. For each: the problem in one paragraph, the options
considered, a recommendation with consequences, and what it demands of the areas
it touches. Where an area turns on an unanswered owner question, say so and
point at Step 5.

**A. Index and coordinate types.** What `Array2D`/`SurfaceMap2D` become: widen
the index type, or make whole-world addressing impossible by construction so the
question does not arise. The `(worldSize, cellSize)` envelope the types can
express, and what enforces it — a validated `LandscapeDef`, an editor clamp, a
`DEBUG_ASSERT`, or a type that cannot represent the bad state. State how a
chunk-local index relates to a world index and which one appears in which
signature. This is where you conform to `_openworld-prompt.md` area A, at a
scale where 32-bit floats still work: say explicitly which of that design's
coordinate machinery you are adopting early and which you are deferring, and why
deferring it does not paint the open world into a corner.

**B. Chunking and residency.** Define the chunk — generation and storage
granularity — and the region, if the design needs a separate simulation and
interest granularity, and justify both sizes with the arithmetic above (memory
per resident chunk, entities per region, grid storage per chunk, chunks needed
to cover the far plane). The lifecycle: generate → load → activate → deactivate
→ evict. Which of today's subsystems becomes per-chunk, which stays
whole-world, and which becomes an environment layer that does not scale at all.
Where the heightmap container must live for the design to work, given it is in
`NeuronClient` today and `check_layering.py` has no escape hatch. **Residency
must be a policy, not a constant** — the open world will hand this design a
different policy, and a design that hard-codes "all chunks resident" has built
the wrong seam even if it performs.

**C. Terrain generation.** Whether the tile list survives, becomes a stamp
format over generated terrain, or is retired. The chunk-border continuity
problem: diamond-square needs whole-tile context and does not tile, so name the
candidate answers and recommend one. Generation as a pure function of
`(seed, chunkCoord)`, keyed streams per layer, and never the simulation stream.
How generation cost is amortised so load time is bounded by resident area rather
than world area. What happens to the guide grid and to `LandscapeFlattenArea`.
Conform to `_openworld-prompt.md` area C or state the divergence.

**D. Spatial indexes.** `EntityGrid` and `ObstructionGrid` at 8.6 GB and 25 MB
of whole-world allocation. The options — per-chunk grids, hashed sparse cells,
a hierarchy, or a different structure entirely — with the neighbour-query cost
of each measured against the query patterns that actually exist (read
`GetNeighbours`, `GetBestEnemy`, `AreEnemiesPresent` and their callers before
recommending). **`EntityGrid` allocates per team and the team count is a
protocol constant** (`NUM_TEAMS` 4); say what your structure does when that
constant grows, because `_openworld-prompt.md` area F expects it to. Query
ordering must stay deterministic — see *The determinism trap*.

**E. Rendering.** One vertex array and one draw call become what: per-chunk
meshes, LOD levels, seam stitching, frustum culling against the 15,000 far
plane, and a far-field or horizon answer for terrain beyond it. What the water
and cloud layers do when the world is 32× wider than the view distance — note
that `Water`'s cell size already scales with world size, so its polygon count is
constant and its *tessellation quality* is what regresses, which is a visual
decision to put to the owner rather than a performance one. Name what this
design deliberately does **not** redesign — the renderer's internals, the sound
system, Eclipse, entity behaviour — so the scope has edges.

**F. Simulation and gameplay at this size.** Entity counts do not grow with the
map, so state plainly what the simulation cost actually is and whether it
changes at all. What the slice machinery does when most of the world holds
nothing. Whether entities outside resident chunks exist, and what that means for
the world-clamp sites in `Entity.cpp` and friends. Routing and pathfinding
across a 65,536-unit map with per-location routes. What a 65,536² map is *for*
in gameplay terms, and whether anything in the design assumes an answer — if it
does, that is a Step 5 question, not an assumption to bury.

**G. The editor.** Equal weight to the game, per the performance bar. Opening,
navigating and saving a 65,536² map; what replaces a 7,500-line tile list as an
authoring surface; what `ScaleLandscapeButton` does when a full `Landscape::Init`
costs seconds; whether the editor edits the whole world or a resident window of
it, and what that does to operations that are inherently global. The level file
format at this size: whether it stays text, and what `LevelFile::Save` writes.

**H. The migration path.** Not a task DAG — a phasing argument: the ordered list
of separable milestones from today to a playable 65,536² Location, each with
what it proves, what it deliberately fakes, and **what can be smoke-tested at
the end of it**. `AGENTS.md` is emphatic that a green build and a green suite
say nothing about whether the game runs, and two plans are currently sitting
behind an un-run Garden gate; a phasing that produces no runnable intermediate
is a phasing that will accumulate the same debt. State which milestones are
independently shippable and which are not.

## The performance bar

This is the constraint the whole design answers to, and the owner set it
explicitly. Restate it in the document in these terms:

> Measured against The Garden on the same machine: **frame time must not
> regress for the same on-screen area**, **resident memory must not grow with
> world size**, and **time from level select to playable must be bounded by
> resident area, not by world area**. All three hold in the editor as well as
> in the game.

What that demands of you:

- **Propose the actual numbers.** Turn each of the three into a figure with a
  unit — a frame-time budget, a memory cap, a seconds-to-playable budget — and
  defend each from the arithmetic. A bar with no number cannot be failed.
- **Say how each is measured, and by whom.** Nothing in CI can measure any of
  them: CI builds x64 Debug and runs the unit suite, it does not launch the
  client. Frame time and load time are owner-measured on Windows. Say which
  measurements you are asking the owner for, what build to take them on, and
  what the Garden baseline needs to be measured *first* so there is something to
  compare against — that baseline does not exist today and someone has to
  capture it before any of this is falsifiable.
- **Distinguish "no regression" from "no cost".** Streaming has a cost: chunk
  pop-in, LOD transitions, a hitch when a chunk generates. Those are visible
  even when frame time is flat. Name each one this design introduces and say
  what bounds it, rather than letting a frame-time average hide it.
- **Say what is measurable statically and propose a check for it.** Memory
  growth with world size is the one property a tool could plausibly assert
  without running the game — an allocation whose size is derived from
  `GetWorldSizeX()` is greppable, and `tools/` is where this repository puts
  exactly that kind of check. If you propose one, propose it in the shape the
  existing tools have: resolve by name, skip what is ambiguous rather than
  guessing, and under-report rather than cry wolf. Read
  `tools/check_containers.py` and `tools/check_math_types.py` first; `AGENTS.md`
  explains why every gap in them closed by making a *narrower* claim.
- **Test what the suite can actually hold.** `docs/TESTING.md` is the standard,
  and 281 tests exist across four projects. Say which parts of this design are
  unit-testable — index arithmetic, chunk coordinate conversion, residency
  policy, generation purity for a given `(seed, chunkCoord)`, sparse-grid query
  equivalence against the dense grid — and which are not. Generation purity in
  particular is testable and is the guard against the determinism trap;
  recommend it explicitly.

## Step 3 — how to shape the documents

- **Analysis before design, argument before conclusion.** Every design claim
  about current code carries a `file:line`-style pointer a reader can follow.
- **Decision records.** Each significant choice gets: the options, the
  recommendation, the consequences, and what would reopen it. Match the tone of
  the decisions recorded in `AGENTS.md` — dated, owned, revisitable.
- **Numbers first.** Bytes per chunk, chunks resident, triangles per frame,
  samples generated per second of load, cells per query. Arithmetic with stated
  assumptions beats adjectives. Label estimates as estimates and `sizeof`
  assumptions as assumptions.
- **Plain diagrams.** ASCII layer and flow diagrams in the style of
  `docs/ARCHITECTURE.md`; no external tooling.
- **Speak the glossary.** Citizen, Spirit, Officer, Trunk Port — reuse the
  domain vocabulary; invent new terms only where a concept is genuinely new, and
  define them in one place.
- **The document must stand alone.** A reader with `AGENTS.md` and this document
  — and neither this prompt nor the chat — gets the whole design, including the
  decisions already made and the questions still open.

## Step 4 — the comparison the owner will want

Include one table that a reader can check at a glance: for each area-scaling
structure, its Garden cost, its naïve 65,536² cost, and its cost under your
design. That table is the argument that the performance bar is met, and it is
the first thing anyone will look at.

## Step 5 — ask before you finalise

Answer from the tree what the tree can answer, and cite where. Ask the owner
what only the owner can. Use `AskUserQuestion` if available; otherwise write an
**Open questions** section with your recommendation beside each question. At
minimum:

1. **The Garden baseline** — frame time, resident memory and load time, measured
   on the owner's machine. Nothing in this design is falsifiable until these
   three numbers exist. Say exactly what you need and on what build.
2. **What the map is for.** A 65,536² world at today's entity density is
   overwhelmingly empty. Is it empty by design (an open world to be filled by
   players, per `_openworld-prompt.md`), or does something populate it? The
   answer changes whether region-based simulation is needed at all here.
3. **Authored maps at this size** — do the existing twelve maps stay at their
   current sizes alongside a large one, get scaled up, or get retired? This is
   the same question as `_openworld-prompt.md`'s Decision 3, one milestone
   earlier; recommend consistently with whatever that design concluded, and say
   if it has not been run yet.
4. **Terrain authoring** — does the tile list survive at ~7,500 entries, or is
   terrain generated? Your area C recommendation, put as a question, because it
   decides how much of the editor changes.
5. **Water and cloud fidelity** — `Water`'s tessellation coarsens by 32× at this
   size for free. Accept the visual regression, or spend polygons to hold
   quality? Ask with numbers.
6. **Visual range** — the far plane is 15,000 against a 92,682-unit diagonal, so
   the player can never see across the map. Is that intended, or does a
   far-field/horizon renderer belong in scope?
7. **Cell size envelope** — should `cellSize` stay editable down to 1.0 (which
   the current types cannot express at this world size), or be clamped to a
   validated range?
8. **Dependency posture** — the tree links only against the OS with one
   header-only dependency. If your design wants anything beyond that, ask; build
   topology is never assumed.

## Step 6 — deliverables

1. `docs/LARGE_LOCATION.md` (plus `docs/LARGE_LOCATION_<AREA>.md` splits if
   needed): the analysis, the design areas A–H, the performance bar with
   proposed numbers, the determinism section, the decision records, the
   disposition tables, the comparison table from Step 4, the open questions with
   recommendations, and the phasing argument.
2. A closing report in chat: the shape of the design in ten sentences, the
   recommendations made, the questions asked and what you assumed pending
   answers, and anything the analysis turned up that changes the picture —
   including anything worth adding to `AGENTS.md` *Known issues*. The
   `unsigned short` ceiling, the `MergeTileIntoLandscape` loop counter and the
   `GetValue` wrap-to-zero are already recorded there (2026-08-06), and
   `tasks/landscape-index-safety.yaml` owns the guard-rail fixes — check that
   plan's state before analysing, because whichever of its tasks have landed
   change what the code you are reading does at the limits. The index-widening
   decision (area A) is explicitly NOT decided by that plan; it is deferred to
   this design.
3. Commit the documents (and nothing else) to your working branch with a clear
   message. Do not push to `main`.

Honesty rules apply throughout: report what you actually read; arithmetic is not
measurement; on Linux you cannot build or run the game — say so rather than
implying you checked.
