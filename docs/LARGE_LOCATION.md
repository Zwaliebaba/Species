# The 65,536 × 65,536 Location — analysis and design

**This document licenses no implementation.** It is the design deliverable of
`tasks/_large-location-prompt.md`, produced 2026-08-06 on a Linux agent that
cannot build or run the game. Every capacity figure in it is arithmetic, not
measurement, and is labelled so; every claim about current code carries a
pointer that was read, not remembered. Work toward it happens the way all work
here happens: a task DAG under `tasks/` first (`docs/TASK_DAG.md`), written
from this document after the owner has read it.

Facts were read at `900f769`. The tree moves; re-verify before coding.

This design is a **milestone of the int64×int64 open world**
(`tasks/_openworld-prompt.md`), not a detour around it. Where it builds a seam
the open world will also need — chunks, generation streams, sparse indexes —
it conforms to that prompt's vocabulary and defers what only that design can
decide. That prompt has not been run; where this document leans on it, it
leans on its questions, not on answers it does not have.

---

## Decisions already made

Dated, owned, and not reopened by this document. D1–D4 were taken when the
prompt was written; D5–D8 were put to the owner during the design session
(Step 5 of the prompt, asked before finalising); D9–D15 resolved the
document's seven open questions in a follow-up the same day; D16–D18 came
out of an expert review later on 2026-08-06, which also amended areas B, C,
E and F in place — the amendments are integrated below, not appended. **No
question in this design remains open.**

| # | Decision | Owner, date |
|---|---|---|
| D1 | The target is **65,536 × 65,536 world units at today's cell size**. The unit is the world unit today's maps use; terrain sample density stays in the range the authored maps already span. | Owner, 2026-08-06 |
| D2 | This is a **milestone of the open world**. Chunk and region boundaries introduced here must be usable by the int64 design. | Owner, 2026-08-06 |
| D3 | The **performance bar is measured against The Garden**, and streaming is allowed to hit it — in the editor as well as the game. See *The performance bar*. | Owner, 2026-08-06 |
| D4 | **The server does not become authoritative first.** Re-argued in this document (area F); the analysis found nothing that only an authoritative server can remove, but D8 sharpens the cost — see the decision record there. | Owner, 2026-08-06 |
| D5 | **The map is populated at today's density** — not an empty stage for the open world to fill later. Pro-rata that is ~245,000 entities and tens of thousands of buildings (arithmetic below). This pulls bounded-activity simulation into scope for this milestone. | Owner, 2026-08-06 |
| D6 | **Terrain becomes procedural**: heights are a pure function of `(worldSeed, chunkCoord)`. The diamond-square tile list is retired for the large map; authored maps keep it (D7). | Owner, 2026-08-06 |
| D7 | **The twelve authored maps are unchanged, alongside.** They keep their sizes and their load path; the large Location is additive. The Garden seven-step smoke test stays valid throughout this work. | Owner, 2026-08-06 |
| D8 | **The far plane stays 15,000; no horizon renderer.** Distant terrain is fog, as today. Resident set is bounded by view distance, not world size. | Owner, 2026-08-06 |
| D9 | **Work proceeds on arithmetic; the Garden baseline is captured later** — before M2, the first milestone with a new map to measure against. Until then the bar's numbers are estimates by declaration, not omission. | Owner, 2026-08-06 |
| D10 | **Population is biome-clumped** at the D5 total: nests and settlements keyed off the terrain octaves, empty plains between. Activation worst case is the largest clump, clamped by the generation layer. | Owner, 2026-08-06 |
| D11 | **Water and clouds recentre on the camera** at their constant vertex counts; wave phase keys to world position so recentring is invisible. Nothing beyond the fog line buys polygons. | Owner, 2026-08-06 |
| D12 | **The large map uses `cellSize` 16** — chunk-exact, 32×32 chunks of 128×128 cells, within the 10.66–21.17 range authored maps already use. This is the accepted reading of D1. | Owner, 2026-08-06 |
| D13 | **`2dSurfaceMap.h` moves down to `NeuronCore`**, beside `2dArray.h`. Downward, legal, lands in M1; what lets a headless server hold terrain later. | Owner, 2026-08-06 |
| D14 | **`SpiritReceiver`'s spawn scatter is scoped to the receiver's region**, accepting the version-skew-only `syncrand` sequence shift — the `determinism` T5 cost shape, to be recorded in `AGENTS.md` when it lands as T5's was. | Owner, 2026-08-06 |
| D15 | **No external dependency enters.** Noise is hand-rolled integer hashing; the sparse grid is standard-library; there is no store because the world regenerates. The tree keeps its one header-only dependency. | Owner, 2026-08-06 |
| D16 | **The dormant world is visibly empty this milestone.** Ledgered entities and buildings do not render; the camera over a region without the player's units sees terrain only. Accepted openly rather than discovered at the gate. What would reopen it: the owner seeing T16 and wanting the cosmetic render-proxy layer, which is lockstep-safe (camera-keyed but sync-inert) and was declined on cost, not on correctness. | Owner, 2026-08-06 |
| D17 | **The ledger stores full survivor state.** Dematerialise writes each survivor's position (fixed-point, ~0.1-unit quantisation), health and unit membership to its *current* chunk's ledger; rematerialisation restores what was left. Nest damage persists for the session; nothing resets on re-entry. | Owner, 2026-08-06 |
| D18 | **Traversal times are accepted**: ~6 minutes base pan corner to corner, ~37 seconds at the existing ×10 sprint. Fast travel is open-world era work and stays out of scope. | Owner, 2026-08-06 |

Decisions recorded in the tree that this design builds on: deterministic
lockstep is to be replaced *eventually* but governs everything here
(`docs/ARCHITECTURE.md` *Runtime model*); object identity becomes a stable
generated id when the protocol changes (owner, 2026-08-02); mixed-architecture
play is not supported (owner, 2026-08-03).

---

# Part 1 — Analysis

## 1.1 What exists, verified

The world model is one `Location` (`GameLogic/Location.h:36`), holding a
`Landscape` by value, five `FastSlotMap` containers of world objects, two
whole-world spatial grids, water, clouds and teams, torn down whole and
rebuilt from two text files on entry (`Location::Init`, `Location.cpp:105`).

Every fact the prompt's table carried was re-verified at `900f769`. The ones
the design turns on:

| Fact | Where |
|---|---|
| The Garden is 2,002² world units at `cellSize 10.66`; defaults are 2,000² at 12; the largest authored map is 5,372; authored cell sizes span 10.66–21.17 | `GameData/Levels/MapGarden.txt`, `LevelFile.h:147-156`, `GameData/Levels/Map*.txt` |
| Heightmap and normal map are two whole-world `SurfaceMap2D`s sized `worldSize / cellSize` | `Landscape.cpp:552-553` |
| `Array2D` dimensions and indices are `unsigned short` | `NeuronCore/2dArray.h:28-47` |
| `SurfaceMap2D` construction narrows `ceilf(worldSize/cellSize)` — a float — into those `unsigned short`s: **UB past 65,535 cells** | `NeuronClient/2dSurfaceMap.h:70, 98` |
| `GetValue` wraps out-of-range indices to **zero**, so the outermost cell interpolates against the opposite edge | `2dSurfaceMap.h:125-128`, specialisation `183-190` |
| `EntityGrid` allocates one flat cell array **per team** at 8×8 units, sized from the whole world | `Location.cpp:131`, `EntityGrid.cpp:184-193` |
| `ObstructionGrid` is a whole-world `SurfaceMap2D<ObstructionGridCell>` at 64×64, each cell a `std::vector<int>`, fully recalculated at construction | `Location.cpp:132`, `ObstructionGrid.cpp:17-26` |
| `LandscapeRenderer` builds **one** vertex array for the whole map, one strip, no LOD, no culling; two 36-byte verts per above-water cell | `LandscapeRenderer.cpp` `BuildVertArrayAndTriStrip`, `LandscapeRenderer.h`, `RgbColour.h:11`, `TextureUv.h:9` |
| A `LandscapeTile` generates a power-of-two-plus-one heightmap by diamond-square drawing from `speciesRandom` (the cosmetic LCG), then an **unconditional** whole-tile height-shift loop | `Landscape.cpp` `LandscapeTile::Generate` |
| `MergeTileIntoLandscape` loops `unsigned short` counters against an `int` cell count: **never terminates at ≥ 65,536 cells** | `Landscape.cpp:364-390` |
| `Water` derives its cell size as `detail × worldSize / 100`, so its grid is a constant ~200×200 cells and its **tessellation coarsens** with the world; its light map is a fixed 128² mask | `Water.cpp:57-88, 113-120, 304-315` |
| `Clouds::RenderFlat` covers fixed extents (−8,000 … +9,000), not the world | `Clouds.cpp:94-99` |
| Inside a location the far plane is 15,000; the lobby uses 10,000,000 | `Species/Renderer.cpp:350-356` |
| Camera pan speed is `worldSize / 30` per second | `Species/Camera.cpp:87-89` |
| The editor permits `worldSizeX/Z` up to 1e6 and `cellSize` down to 1.0; tile size up to 10,000; the scale button multiplies sizes without bound and re-runs `Landscape::Init` per click | `LandscapeWindow.cpp:407-409, 160, 290-330` |
| `LevelFile` parses world size and tile size with `atoi`, no validation | `LevelFile.cpp:503-507, 557` |
| `Location::Init` skips both grids and clouds entirely when `g_editing` | `Location.cpp:126-136` |
| `GenerateSyncValue` sums entity state in container index order — entity-bound, not area-bound | `Species/Main.cpp:233`, `docs/ARCHITECTURE.md` *Runtime model* |
| **The world size parameterises the lockstep RNG**: `SpiritReceiver` spawns unprocessed spirits at `syncfrand(worldSizeX)` | `SpiritReceiver.cpp:463-467, 553-557` |
| Entities clamp to `GetWorldSizeX/Z()` — the exact coordinate where `GetValue`'s wrap begins | `Entity.cpp:426-435`, `Airstrike.cpp:261-264`, `InsertionSquad.cpp:570-579`, `Lander.cpp:54-63`, `Snow.cpp:81-90`, `SpiritReceiver.cpp:915-920` |

The three reachable index defects and the `GetHighestValue` rows/columns typo
are recorded in `AGENTS.md` *Known issues* (2026-08-06) and owned by
`tasks/landscape-index-safety.yaml`. This design **depends on that plan
landing first** — in particular T4, without which partially-resident terrain
turns the wrap-to-zero into a live, client-dependent wrong answer.

### The whole-world sweep

Every file that reads `GetWorldSizeX/Z` (17 files; the count per file, read
2026-08-06): `Water.cpp` ×17, `Location.cpp` ×7, `SpiritReceiver.cpp` ×6,
`EntityGrid.cpp` ×6, `Airstrike.cpp` ×4, `Species/Camera.cpp` ×3,
`LocationEditor.cpp` ×2, `Weapons.cpp`, `Virii.cpp`, `Spirit.cpp`, `Snow.cpp`,
`ObstructionGrid.cpp`, `Lander.cpp`, `InsertionSquad.cpp`, `Entity.cpp` ×2
each, `Species/Main.cpp`, `GlobalWorld.cpp` ×1 each. They fall into four
classes, and the design answers each class once rather than each site:

1. **Sizing an allocation** (both grids, water's wave tables) — becomes
   per-chunk or sparse; area D.
2. **Clamping a position** (the six entity files) — unchanged in meaning;
   the clamp value grows. Safe at 65,536: it is exactly 2¹⁶, unit-exact in
   float (§1.3).
3. **Parameterising a `syncfrand` draw** (`SpiritReceiver`) — a behaviour
   *and* determinism question; area F.
4. **Scaling presentation** (camera speed, water cell size, editor overview)
   — each needs a deliberate answer, not an inherited formula; areas E and G.

## 1.2 The arithmetic

All figures assume MSVC x64 layout where `sizeof` is involved and are
**arithmetic, not measurement**. Two cell sizes are shown: 12
(`LandscapeDef`'s default) and 16 (the recommended chunk-aligned value, area
A — within the 10.66–21.17 range the authored maps already use, per D1).

**Terrain samples.**

| | Garden (10.66) | 65,536² @ 12 | 65,536² @ 16 |
|---|---|---|---|
| Samples per axis | 188 | 5,462 | 4,096 |
| Total samples | 35,344 | 29.8 M | 16.8 M |
| Heightmap (`float`) | 141 KB | 119 MB | 67 MB |
| Normal map (`XMFLOAT3`) | 424 KB | 358 MB | 201 MB |
| Ratio vs Garden (samples) | 1× | 844× | 475× |

**The `unsigned short` ceiling.** 65,535 cells per axis. At `cellSize` 12 the
world needs 5,462 — comfortable. The ceiling is a today-problem only because
the editor permits pairs that cross it (`worldSize` 1e6 at `cellSize` 12 is
83,334); `landscape-index-safety` T2 makes crossing it loud. **This design
keeps the whole-world heightmap under the ceiling and does not widen the index
type** — chunking makes whole-world addressing unnecessary rather than making
it bigger (area A).

**`EntityGrid`, allocated dense today** (`EntityGridCell` = two `int`, two
pointers, one `int` → 32 bytes padded — *assumption to verify*):

| | Garden | 65,536² dense | Ratio |
|---|---|---|---|
| Cells per axis (8×8) | 251 | 8,193 | 33× |
| Cells × 4 teams | 252,004 | 268.5 M | 1,065× |
| Memory | 8.1 MB | **8.6 GB** | 1,065× |

**`ObstructionGrid`**: 1,024² cells of an empty `std::vector<int>` (24 B
assumed) ≈ **25 MB** + `CalculateAll()` walking every building at
construction, vs 24 KB today.

**The renderer**, worst case (all land): 2 verts × 36 B per cell → **2.1 GB**
at `cellSize` 12 (1.2 GB at 16), ~59.7 M (33.5 M) triangles in one draw call,
plus the same again in the VBO. The below-water skip
(`LandscapeRenderer.cpp`, `BuildVertArrayAndTriStrip`) helps only in
proportion to ocean; a Garden-like land fraction keeps most of it.

**Population, per D5** (pro-rata on Garden's mission: 50 Citizens + 179 Virii
= 229 entities in 2,002²; area ratio 1,071.8):

| | Garden | 65,536² pro-rata |
|---|---|---|
| Entities | 229 | **~245,000** |
| Buildings (Garden places ~30) | ~30 | ~32,000 |
| `FastSlotMap` residency at ~0.5 KB/entity (assumption) | ~115 KB | ~123 MB |

Entity memory is survivable. Entity **advance** is not: `Location::Advance`
walks every live entity every tick in 10 slices, and 245,000 is ~1,070× the
per-tick entity work of the map the game runs today. No terrain chunking
touches that number. This is why D5 pulls bounded activity into scope — §F.

**Load time.** 844× (or 475×) the samples through generation, normals,
colour and UV building, plus ~1,070× the entity spawning. Unmeasurable from
here; the bar (Part 3) bounds it by *resident* area instead, which makes the
multiplier ~1 by construction.

## 1.3 Float precision

65,536 = 2¹⁶, well inside float's unit-exact 2²⁴. The ULP at x ≈ 65,536 is
2⁻⁷ ≈ 0.0078 world units, vs ≈ 0.00012 at Garden scale — position resolution
64× coarser at the far corner. Entity radii are ~1–10 units and `cellSize`
≥ 10.66, so neither collision nor terrain sampling is threatened.
`GenerateSyncValue` is unaffected *within* one build: every client computes
identically wherever the entity is; coarser is not divergent. What it does
mean: physics near the far corner quantises visibly harder than near the
origin (a Citizen's sub-0.01-unit motion rounds away). Accepted for this
milestone; the open world's region-local frames (its area A) are the real
fix, and nothing here forecloses them.

## 1.4 The editor

`g_editing` skips both grids and clouds (`Location.cpp:126-136`), so the
editor's costs are different: whole-map regeneration and re-render.

- `ScaleLandscapeButton` runs a full `Landscape::Init` per click
  (`LandscapeWindow.cpp:313-317`) — at 844× the samples, seconds per click.
- The tile list is the authoring surface. Garden covers 2,002² with 7 tiles;
  held at that density, 65,536² is ~7,500 rows of eleven numbers — not
  authorable. D6 retires it for the large map.
- Mouse picking is `Landscape::RayHit` walking heightmap cells
  (`Landscape.cpp`), fine when the resident window is bounded.
- The editor renders every tile and flatten area every frame
  (`LocationEditor.cpp:790-940`) — moot once the large map has none.

## 1.5 The determinism trap

The simulation is deterministic lockstep and **stays so through this
milestone** (D4). Every lazily-resident structure introduces the same hazard:
*a value that depends on whether a chunk happened to be loaded differs
between clients.* `GenerateSyncValue` sums entity positions and velocities in
container index order (`Main.cpp:233`), so anything reaching a position
reaches the sync value. Four concrete rules govern the design:

1. **Generation is a pure function of `(worldSeed, chunkCoord, layer)`** and
   draws from its own keyed stream — never `syncrand` (would desequence the
   simulation), never bare `speciesRandom` (client-dependent state; the six
   desyncs `tasks/Archive/determinism.yaml` T5 fixed are the record of what
   that costs). Today's `LandscapeTile::Generate` seeds and draws the LCG per
   tile — safe only because every client generates every tile at load; lazy
   generation ends that safety.
2. **Generation arithmetic is integer/fixed-point.** The landscape-shape
   observation of `directxmath-migration` T13 (same build, different-looking
   terrain until checksums proved it identical) is the cheap warning; float
   noise differing across compilers or flags is the expensive one. Within one
   architecture float would suffice today — integer noise removes the
   question, costs little for lattice noise, and is the choice the open
   world's area C will want anyway if clients ever generate locally.
3. **Residency must not gate simulation values.** A simulated entity must
   read the same ground height whether the local client had that chunk
   resident or not — so simulation-critical queries (`GroundHeight`,
   `IsWalkable`, grid queries) force synchronous generation of the chunk they
   touch, deterministically, on every client. Rendering may lag; simulation
   may not.
4. **Wake rules key on simulation state, never on presentation.** A camera
   position is client-local; if camera proximity woke a region, clients would
   desync immediately. Activity derives from things every client computes
   identically: where player-controlled units, orders, and active buildings
   are (§F).

Standing hazards this design inherits and names: the `GetValue` wrap-to-zero
(fix owned by `landscape-index-safety` T4 — a **prerequisite**), and
`SpiritReceiver`'s `syncfrand(worldSize)` draws, which make the *world size
itself* an input to the lockstep stream — any change to the size or to those
call sites shifts the sequence (§F).

## 1.6 Disposition of the world model

Member-by-member, the spine of the migration story. **Per-chunk** = owned by
a chunk record; **windowed** = exists only for the resident window;
**world** = one instance, size-independent; **retired** = gone for the large
map (authored maps keep everything, per D7).

**`Location` (`Location.h:36-120`)**

| Member | Disposition |
|---|---|
| `m_landscape` (by value) | Becomes the chunk-owning terrain object: per-chunk height/normal blocks behind the same query API (area B) |
| `m_entityGrid` | Sparse, hashed, population-proportional (area D) |
| `m_obstructionGrid` | Per-chunk, built at chunk activation (area D) |
| `m_levelFile` | World: parameters only (seed, size, teams, objectives) — no tiles, no `InstantUnits` for the large map |
| `m_clouds`, `m_water` | World: already view-anchored / constant-count; retuned not restructured (area E) |
| `m_teams` | World: `NUM_TEAMS` is protocol (`ProtocolLimits.h`), untouched here |
| `m_lights` | World: authored ambience, a handful |
| `m_buildings`, `m_spirits`, `m_lasers`, `m_effects` | World containers, **active residents only**; dormant population lives in chunk ledgers (area F) |
| Four `SliceWalker`s | Unchanged mechanics over the (bounded) active containers |
| `m_lastSliceProcessed`, `m_missionComplete`, `m_christmasTimer` | World, unchanged |

**`Landscape` (`Landscape.h:75-120`)**

| Member | Disposition |
|---|---|
| `m_heightMap`, `m_normalMap` | Replaced by per-chunk blocks; the `SurfaceMap2D` type survives as the per-chunk container, sized 129² — far under the 16-bit ceiling by construction |
| `m_outsideHeight` | World parameter (the "not yet generated" answer is *generate it*, rule 3 above; outside the world border it keeps today's meaning) |
| `m_renderer` | Per-chunk mesh + shared state (area E) |
| `m_worldSizeX/Z` (float) | World; exact at 65,536 (§1.3) |
| `MergeTileIntoLandscape`, tile machinery | Authored maps only (D6/D7); counter fix per `landscape-index-safety` T3 regardless |

**`LevelFile` (`LevelFile.h`)**

| Member | Disposition |
|---|---|
| `m_landscape` (`LandscapeDef`) | Gains the procedural block: `worldSeed`, generation parameters. Tiles/flatten lists empty for the large map |
| `m_buildings`, `m_instantUnits`, `m_runningPrograms` | Retired for the large map — population is generated (area F); authored maps unchanged |
| Camera mounts, routes, objectives, difficulty | Unchanged; a route's waypoints are float positions, exact to 2²⁴ |
| `Save` | Unchanged for authored maps; the large map saves parameters, not terrain (persistence itself stays **out of scope** — the large map regenerates on entry exactly as today's maps re-parse; runtime state is lost on exit today and that does not change here) |

---

# Part 2 — Design

## A. Index and coordinate types

**Problem.** One address space (float world coordinates) currently maps to
one allocation via 16-bit indices.

**Options.** (1) Widen `Array2D` to 32-bit indices and keep whole-world
containers; (2) keep 16-bit indices and make whole-world addressing
impossible by construction — nothing ever allocates a world-sized array.

**Recommendation: (2).** Widening buys an 8.6 GB `EntityGrid` the right to
exist; chunking removes the need. `Array2D`/`SurfaceMap2D` survive unchanged
as *per-chunk* containers (129² samples), with `landscape-index-safety` T2's
construction check as the enforcement that nothing regresses. World
coordinates stay `float` `XMFLOAT3` everywhere — exact to 2²⁴ (§1.3), no new
coordinate type this milestone. A **chunk coordinate** is `(int cx, int cz)`
with `cx = floor(x / chunkEdge)`; the conversion lives in one header beside
the chunk record, and it is the seam the open world's int64 design later
widens — int64 world → chunk id + local offset is the same shape
(`_openworld-prompt.md` area A), so nothing here is thrown away.

**Chunk-aligned cell size — decided, D12.** 65,536/12 = 5,461.3 samples —
ragged against any power-of-two chunk. **The large map uses `cellSize` 16**:
4,096 samples per axis = 32×32 chunks of exactly 128×128 cells (129² samples
with the shared border row), chunk edge 2,048 world units, integers
throughout. 16 sits inside the 10.66–21.17 range the authored maps already
use (`MapGenerator.txt` uses 20); the owner accepted this as the reading of
D1's "today's cell size" on 2026-08-06. (The declined alternative, kept for
the record: `cellSize` 12.8 → 5,120 samples, 40×40 chunks of 128.)

**Consequences.** No signature in `GameLogic` changes type; the envelope the
types can express is enforced at construction; the editor's `cellSize`/
`worldSize` controls get validated against the same rule (area G).

## B. Chunking and residency

**Definitions.** A **chunk** is the generation, storage and rendering
granule: 128×128 cells, 2,048×2,048 world units, 32×32 = 1,024 chunks. A
**region** is the simulation-interest granule: a 3×3 chunk neighbourhood
(6,144 units across — comfortably larger than any weapon range or
`m_roamRange` in the tree). Chunks have residency; regions have *activity*
(§F). Two axes, deliberately separate, because rendering follows the camera
and simulation follows the units, and rule 4 of §1.5 forbids conflating them.

**Per-chunk memory** (arithmetic, `cellSize` 16): heights 129²×4 B = 65 KB;
normals 129²×12 B = 195 KB; obstruction cells 32²×~40 B ≈ 41 KB; mesh ≈
128²×2×36 B = 1.2 MB worst-case all-land. **≈ 1.5 MB per fully resident
chunk.**

**Residency window** (D8): far plane 15,000 → chunks intersecting a
30,000-unit square around the camera ≈ 16² = **256 chunks ≈ 380 MB**
worst-case all-land, before the simulation adds its active regions
(bounded, §F). Under the Garden land fraction it is under half that —
labelled arithmetic either way, and the bar (Part 3) is what it answers to.

**Lifecycle.** `generate → resident → active (region) → inactive → evict`.
Generation is deterministic (§C) so eviction is free — no writeback, because
nothing in a chunk mutates outside the pure function this milestone: the
large map has no tiles and no *authored* flatten areas, and flattening under
**generated** buildings is itself a step of the pure function (§C), so a
regenerated chunk comes back flattened identically. Player building
placement is not in scope — it is the open world's area G. Population state
lives in the ledger (§F), not the chunk. **Residency is a policy object,
not a constant** — the camera window on the client, the active-region set
for simulation, both expressed against the same interface, because the open
world will hand this seam a third policy (interest management, its area F).
An **everything-resident policy is selectable by a named preference** — the
debugging kill switch that answers "streaming bug or logic bug?" with one
flip.

**The coarse height envelope.** Rule 3 of §1.5 (simulation queries force
synchronous generation) is correct and, alone, unbounded: an `Airstrike`
crossing the map or a long `IsVisible` ray can demand a diagonal of
never-resident chunks inside one `Advance`, on every client. So the design
adds one always-resident structure: **min/max height per chunk**, computed
closed-form from the coarse octaves at load — 32×32 chunks × two floats ≈
8 KB (a 257² coarse field is the fallback if per-chunk bounds prove too
loose, ≈ 260 KB; both are arithmetic). Conservative ray and line-of-sight
tests resolve against the envelope first and force full generation only
where the envelope admits a possible hit. This caps the worst hitch class
by construction.

**Layering — decided, D13.** The chunk record needs heights on a headless
server eventually; `SurfaceMap2D` lives in `NeuronClient` today
(`NeuronClient/2dSurfaceMap.h`). **`2dSurfaceMap.h` moves down to
`NeuronCore`** beside `2dArray.h` — it includes only `2dArray.h` and
`NeuronMath.h` (`2dSurfaceMap.h:1-10`), both already in `NeuronCore`, so the
move is downward, legal, and small; the chunk record itself lives in
`GameLogic`. This was the one project-file/topology-adjacent change in the
design; the owner approved it on 2026-08-06 per the ask-first rule in
`AGENTS.md`, and it lands in M1.

**What stays whole-world:** teams, lights, water, clouds, sky, fog — the
environment layer, none of which scales with area (§1.1, water/clouds rows).

## C. Terrain generation (D6)

**Problem.** Diamond-square needs whole-tile context, draws from the shared
cosmetic LCG in seeded bursts, and does not tile. None of that survives
per-chunk generation under §1.5's rules.

**Recommendation.** Lattice (value/gradient) noise, **integer/fixed-point**,
evaluated per sample: `height(x,z) = Σ octaves of noise(worldSeed, layer,
lattice points)` where every lattice-point hash is a pure function of
`(worldSeed, layer, latticeX, latticeZ)`. Lattice noise is *naturally local*
— a sample needs only its surrounding lattice points, so chunk borders are
continuous by construction, with no overlap windows and no stitching. Coarse
octaves (lattice spacing ≥ chunk edge) give continents; fine octaves give
local relief. This is the open world's area-C candidate built at 1/1,000
scale; the function signature `(seed, chunkCoord, layer)` is the seam D2
requires.

- **Streams:** one keyed hash stream per layer, keyed by position — never
  `syncrand`, never shared LCG state (§1.5 rules 1–2).
- **Cost** (arithmetic): ~5 octaves × a few tens of integer ops × 129² ≈
  ~10⁶–10⁷ ops per chunk — sub-millisecond-scale per chunk on modern
  hardware, but that is exactly the kind of figure the bar requires the
  owner to measure, not this document to assert.
- **Guide grids and authored flatten areas:** authored-map machinery,
  untouched (D7). The large map launches with no authored stamps; the
  tile-as-stamp idea stays open for the open-world design (its Decision 3),
  and nothing here blocks it — a stamp would be one more generation layer.
- **Flattening under generated buildings IS a generation step.** D5
  populates buildings, and a building on raw noise floats or clips — today
  every authored building sits on a `LandscapeFlattenArea` applied at load
  (`Landscape.cpp` `FlattenArea`). The resolution keeps purity: the
  population layer (§F) picks building sites first — slope-constrained, a
  pure function of `(seed, chunkCoord)` — and the terrain function then
  flattens under those sites as its final step. Same inputs, same
  flattening, on every client and every regeneration; the immutability
  claim in area B survives because the flatten derives from the pure
  layers, not from runtime state.
- **Normals** are generated per chunk from its own samples plus the
  one-sample border, same central-difference as `GenerateNormals`
  (`Landscape.cpp:447-460`), so a chunk's normals never depend on residency
  of its neighbours.

**Testability** — the guard against §1.5 and the part of this design most
worth unit tests (`docs/TESTING.md`): generation purity is a plain assert —
same `(seed, chunkCoord)` → identical block, any generation order, any
subset generated. That test is cheap, runs on CI's x64, and pins the one
property everything else stands on. Recommend it lands with the first
generation commit, not after.

## D. Spatial indexes

**Problem.** 8.6 GB dense `EntityGrid`; 25 MB eager `ObstructionGrid`
(§1.2). Query patterns, read before recommending: `GetNeighbours` and
friends gather ids within a radius (≤ ~200 units in every caller found) into
a scratch array; `GetBestEnemy` scans that; callers are entity AI inside
`Advance` — so queries are radius-local and ordering-sensitive
(`EntityGrid.cpp:184-660`).

**Options.** (1) Dense per active chunk — 256×256 cells × 32 B × 4 teams ≈
8.4 MB *per chunk*, hundreds resident → gigabytes again; fails arithmetic.
(2) Hashed sparse cells: cell exists only while occupied. (3) Hierarchy —
more structure than a radius query needs.

**Recommendation: (2).** A hash map from `(cellX, cellZ, team)` → cell
record, population-proportional: ~245k entities → at most ~245k occupied
cells ≈ **~12 MB at 48 B/cell** (arithmetic; assumption stated), vs 8.6 GB.
Two determinism obligations, per §1.5: iteration for a radius query walks
the *cell rectangle* in fixed x-then-z order exactly as the dense grid does
(`EntityGrid.cpp:280-340` shape), so neighbour *ordering* — which feeds
target selection, which feeds positions — is unchanged by hashing; and the
scratch-array growth (`EnsureMaxNeighbours`) stays as-is. Per-team structure
survives (the `m_cells[NUM_TEAMS]` shape becomes per-team maps), so when
the open world grows the team count (its area F), the structure scales by
adding maps, not by reshaping.

**Equivalence test:** same insertions → identical query results *and result
order* against the dense grid on a small world. That is the
characterisation this conversion must ship with, per the standard
(`CODING_STANDARDS.md` conversion checklist step 2).

**`ObstructionGrid`**: per-chunk 32×32 cells built at chunk activation from
that chunk's buildings — `CalculateAll` becomes `CalculateChunk`. Building
footprints near borders register in every chunk they touch (footprints are
small; `CalculateBuildingArea` already computes the touched cell rectangle,
`ObstructionGrid.cpp:29-60`).

## E. Rendering (D8)

**Problem.** One strip, one draw, no culling; 1.2–2.1 GB of verts at target
size (§1.2).

**Recommendation.** Per-chunk meshes, built at chunk residency, frustum-culled
per chunk against the 15,000 far plane; **no LOD** this milestone. Draw-call
count is not the risk — **triangle fill is, and fog is its real bound, not
the far plane.** The arithmetic that decides whether no-LOD survives: The
Garden's whole map (~4M u²) is about one chunk and ~70k terrain triangles
total; a ~60° frustum swept to the full 15,000 far plane covers ~118M u² ≈
28 land chunks ≈ **~0.9M triangles** — ~13× Garden's whole load. What makes
no-LOD true is that D8 keeps fog "as today": **the large map's fog end is
pinned Garden-like (~3,000–4,000 units effective terrain visibility)**, at
which the frustum holds ~2 chunks ≈ Garden's own triangle count. The
implementing task records the fog constant it finds in
`Species/Renderer.cpp` (unread from this environment — verify there) and
carries this arithmetic in its acceptance; if the owner later wants fog
pushed toward the far plane, LOD re-enters scope and that is a new
decision, not a tuning knob. Chunk meshes are backed by a **pooled buffer
set** — VBOs are recycled on evict/build rather than created and destroyed
per chunk, because buffer churn during fast camera travel is a classic
hitch source. The strip builder is reused per chunk verbatim (below-water skip
included); `m_landscapeColour` sampling and UV generation take chunk-local
offsets. Water — decided, D11: constant-count grid already (§1.1); its
200×200 grid recentres on the camera instead of the world, wave phase keyed
to world position so the recentring is invisible; polygon budget unchanged,
tessellation at Garden density near the viewer, and the visual change
distant water would have shown is behind fog (D8). Clouds: already
fixed-extent; anchored to camera the same way. Camera pan speed
(`Camera.cpp:87-89`): stop deriving from world size — 65,536/30 ≈ 2,185
units/s is unusable near ground; clamp to the largest authored-map speed and
make faster travel an explicit control, not a formula surprise.

**Deliberately not redesigned:** the renderer's internals (GL state, display
lists vs VBO paths), the sound system, Eclipse, entity behaviour state
machines, `TargetCursor`/input. Scope has edges; these are them.

## F. Simulation at scale (D5) — population, dormancy, lockstep

**Problem.** D5 populates the map at today's density: ~245,000 entities
(§1.2). Advancing them all is ~1,070× today's per-tick entity work;
authoring them as `InstantUnits` is ~7,500 Gardens' worth of lines; and
deterministic lockstep means every client pays all of it. This is the area
D5 moved from the open world's plate onto this milestone's.

**Design.** Population is **generated, ledgered, and mostly dormant**:

- **Generated:** a population layer of the §C function — per chunk,
  `(worldSeed, chunkCoord, 'population')` yields deterministic spawn groups
  (species, counts, positions), biome-keyed by the terrain octaves.
  Replaces `InstantUnits` for the large map. Shape decided, D10:
  **biome-clumped** at the D5 total — nests and settlements where the
  octaves say so, empty plains between, and the worst-case activation cost
  is the largest clump, which the generation layer clamps.
- **Buildings are generated by the same layer, and first.** D5's population
  includes buildings (nests are building organs — AntHill, Triffid,
  SporeGenerator — and settlements have structures). The layer picks
  building sites slope-constrained, assigns each a deterministic id keyed
  `(chunk, ordinal)` — building ids are load-bearing, since pylons, fences
  and tracks link by int id — and the terrain function flattens under the
  sites as its final step (§C). Activation materialises buildings through
  the normal construction paths and registers them in the per-chunk
  obstruction grid; dormant buildings are ledger data like everything else,
  and per D16 they do not render while dormant.
- **Ledgered:** dormant population is *data* in a per-chunk ledger, not
  entities in slot maps. It costs no Advance, no grid cells, no sync-sum
  entries. **Schema decided, D17 — full survivor state**: per record,
  species/type, position in fixed point at ~0.1-unit quantisation, health,
  and unit/group membership; an entity's chunk membership is its position
  at dematerialise time. Nest damage persists for the session. The record
  format carries a **version byte and a byte-stability test** in the
  `ByteStream` style the suite already uses — persistence is out of scope,
  but the open world's delta store (its area D) will serialise exactly this
  record, and versioning it now costs nothing.
- **Visibly dormant is visibly empty — D16, accepted openly.** No render
  proxies this milestone: the camera over a dormant region shows terrain
  only. The lockstep-safe proxy design (camera-keyed, sync-inert, drawn
  from the ledger) is recorded in D16 as the thing to build if the owner
  reverses after seeing T16.
- **Active:** a region (3×3 chunks, §B) **activates on simulation-visible
  causes only** (§1.5 rule 4): a player-controlled unit inside it, an order
  targeting it, an active building in it. Activation materialises the
  ledger into real entities via the normal spawn paths; deactivation writes
  survivors back as ledger data (full state, D17). Every client computes
  the same causes at the same tick, so the same regions activate everywhere
  — lockstep holds. **Hysteresis is part of the mechanism, not a tuning
  afterthought**: the activation radius exceeds the deactivation radius, a
  region must be quiet for a minimum-dwell tick count before it may sleep,
  and budget eviction breaks ties in region-coordinate order — all three
  deterministic, all three there to stop a unit patrolling a boundary from
  thrashing materialise/dematerialise cycles.
- **Bounded:** an explicit active-entity budget. **Proposed: ≤ 4,000 active
  entities** (~17× Garden's 229) — a *proposed number for the owner to
  measure against*, not a measurement; the mechanism (per-region budgets,
  nearest-cause-first) matters more than the constant.
- **Identity:** materialise/dematerialise reallocates slot indices, which
  are wire identity today (`WorldObjectId`, serialised verbatim). Under
  lockstep this is safe *because* it is deterministic — every client
  materialises the same entities in the same order at the same tick, so
  indices agree, exactly as spawning agrees today. The recorded stable-id
  decision (owner, 2026-08-02) is when identity decouples from slots; this
  design does not need it early, but the ledger keys entities by
  `(chunk, ordinal)` precisely so stable ids can be laid over it without a
  format change.
- **Sync:** `GenerateSyncValue` sums the *active* containers as today —
  bounded again — plus a cheap ledger checksum folded in per modified
  chunk, so dormant divergence (impossible by construction, but the check
  exists to catch bugs) is still caught.
- **`SpiritReceiver`'s `syncfrand(worldSize)`** (§1.1) — decided, D14: the
  spawn is scoped to the receiver's region. The draws advance the lockstep
  stream, so this shifts the `syncrand` sequence — the same accepted,
  recorded cost as `determinism` T5 (version-skew only; protocol v2 already
  refuses old builds). On the authored maps the region spans most of the
  map, so behaviour there barely moves. To be recorded in `AGENTS.md` when
  it lands, as T5's shift was.

**Decision record — D4 re-argued, as the prompt requires.** Analysis
confirms: terrain, renderer, editor and both grids cost the same in any
process, and chunking fixes them wherever they run. What D5 adds is
*agent-proportional* load, which is the load lockstep genuinely cannot
scale past — every client simulates every active entity. The design above
keeps that bounded (~4,000 active), which fits comfortably inside lockstep;
so D4 stands for this milestone. **The consequence to state plainly:**
245,000 *simultaneously active* entities is not reachable under lockstep on
any client hardware — that ceiling lifts only with the authoritative
server, and this design's ledger/activation seam is exactly the interest-
management seam that server will reuse (open world areas E/F). Nothing
found requires reordering the milestones. What would reopen D4: the owner
wanting world-scale battles (>> the active budget) before the open world.

**Pathfinding and AI:** unchanged mechanics inside active regions (routes,
obstruction queries are region-local — every `m_roamRange`/waypoint read
found in the sweep is ≤ region scale); cross-map routing does not exist
this milestone because nothing dormant moves. `AITarget`/`AISpawnPoint`
stay authored-map machinery.

## G. The editor

**Problem.** The editor is a tile-list author; the large map has no tiles.
Whole-map `Landscape::Init` per parameter tweak; value controls whose limits
already exceed what the types survive (§1.1).

**Recommendation.** For the large map the `LandscapeWindow` edits *the
function*: seed, octave parameters, biome thresholds, population densities —
a dozen scalars — with regeneration of only the **resident window** (256
chunks, not 1,024, and generation is per-chunk cheap per §C) on change.
Navigation: the existing camera plus a coarse overview rendered from the
top octave (continent layer) — cheap, chunk-independent. The editor edits
the whole world *by construction* (parameters are global); nothing global
is done per-sample any more, which is what kills `ScaleLandscapeButton`'s
cost model — for the large map that button and the per-tile windows simply
do not apply (they remain for authored maps, D7). Validation: the
`worldSize`/`cellSize` controls get the same envelope check as construction
(`landscape-index-safety` T2), closing the prompt's cell-size-envelope
question (its open question 7) at the container boundary. `LevelFile::Save` for the large map writes the parameter block —
kilobytes, text, exactly the current format's shape.

## H. The migration path

A phasing argument, not a task DAG. Every milestone ends **runnable and
smoke-testable** — `AGENTS.md`'s standing lesson is that green builds say
nothing, and two plans already sit behind an un-run Garden gate; this
phasing adds no third.

| # | Milestone | Proves | Deliberately fakes / defers | Smoke test at its end |
|---|---|---|---|---|
| M0 | `landscape-index-safety` lands (plan exists, 0/4) | The container limits are loud; edge lookups sane | Everything else | Garden 7 steps (regression only) |
| M1 | Terrain storage chunked *behind today's sizes*: heightmap/normals as chunk blocks, per-chunk renderer, `SurfaceMap2D` moved to `NeuronCore`; **all twelve authored maps unchanged in behaviour** | The chunk seam and per-chunk rendering, against known content | Generation (tiles still merge into chunk blocks); residency (all chunks resident) | Garden 7 steps — landscape visually identical; renderer chunk count > 1 |
| M2 | The generation function + the large map loads **empty**: chunked residency live, camera window streaming, editor parameter mode | Purity, borders, streaming, the bar's memory/load numbers | Population (none yet); water/cloud recentring may land here or M3 | New: enter the 65,536² map, fly the camera for minutes, `GroundHeight` everywhere, no assert; Garden 7 steps unchanged. **The D9 Garden baseline is captured at this gate**, and the bar's numbers become falsifiable here |
| M3 | Population: generation layer, ledger, region activation, sparse `EntityGrid`, per-chunk `ObstructionGrid` | D5 at the bar; lockstep holds with materialisation on | Stable ids (slot identity retained, §F); persistence (none, as today) | New: enter, find generated Citizens/Virii at deterministic spots, watch 30 s of behaviour, sync assert quiet; Garden 7 steps unchanged |
| M4 | Editor at scale: overview, resident-window regen, envelope validation | D3's editor half | — | Owner authors a seed/biome change and plays it |

M1 is the load-bearing risk-retirement step: it converts the renderer and
storage *under the existing smoke test*, where a mistake is visible against
known terrain — the cheapest possible place to be wrong. Every milestone
keeps the authored maps' path intact (D7), so the Garden baseline never
stops being the control. What the eventual plans must preserve to keep this
cheap: the `LocationAccess` query interface as the world-query seam
(`NeuronClient/LocationAccess.h:47`), `SurfaceMap2D`'s narrow API (it is
the per-chunk container), and no new location-scale float signatures.

---

# Part 3 — The performance bar

Owner-set (D3), restated as the constraint the whole design answers to:

> Measured against The Garden on the same machine: **frame time must not
> regress for the same on-screen area**, **resident memory must not grow
> with world size**, and **time from level select to playable must be
> bounded by resident area, not world area**. All three hold in the editor
> as well as in the game.

Proposed numbers — each a figure to be *defended or corrected by
measurement*, since nothing here can measure (proposed against the
arithmetic above; the mechanism stands even where a constant moves):

| Budget | Proposed | Rationale (arithmetic) |
|---|---|---|
| Frame time | ≤ Garden baseline ±10% at equal visible area | Same triangle density in-window; ≤ ~256 chunk draws (E); active entities ≤ ~17× Garden (F) with slice machinery unchanged |
| Resident memory over Garden | ≤ +600 MB | ~380 MB worst-case terrain window (B) + ~123 MB full entity residency bound + indexes (D) |
| Level select → playable | ≤ 2× Garden's load | Only spawn-window chunks and initially-active regions generate at load; the rest streams |
| Editor parameter change → visible | ≤ 2 s | Resident-window regeneration only (G) |

**How each is measured, and by whom.** Nothing in CI can measure any of
them — CI builds x64 Debug and runs the unit suite; it does not launch the
client (`AGENTS.md`). All four are owner measurements on Windows. **The
Garden baseline — frame time at the standard camera, peak working set, and
stopwatch load on a current build — does not exist yet, and the owner
decided (D9) that work proceeds on arithmetic with the baseline captured
before M2**, the first milestone that produces a new map to measure against.
Until it exists, every number in this table is an estimate by declaration,
and M2's gate includes capturing it. **The gates produce numbers only if
the build can show them**: an instrumentation pass — resident chunk count,
generation milliseconds per frame, active entity count, triangles
submitted, on the existing `Profiler` or a debug overlay — lands before the
M2 gate, so every owner run reads data instead of estimating by feel.

**"No regression" is not "no cost".** Streaming introduces artefacts a
frame-time average hides, named so they are looked for: chunk pop at the
fog boundary (bounded by generating one ring beyond the far plane);
generation hitches on fast camera travel (bounded by budgeted per-frame
generation — chunks/frame is a tunable, and the camera speed clamp in E
caps demand); region-activation spikes when a large ledger materialises
(bounded by the per-region budget, F).

**Statically checkable, proposed as a tool:** *no allocation sized from
world dimensions.* A `tools/` check that flags `new`/`resize`/`Initialise`
whose size expression reaches `GetWorldSizeX/Z` or `LandscapeDef`
dimensions — the exact shape `check_containers.py` set: resolve by name,
skip ambiguity rather than guess, under-report rather than cry wolf
(`AGENTS.md` explains why every gap in those tools closed by narrowing).
The 17-file sweep in §1.1 is its acceptance list on day one.

**Unit-testable now** (per `docs/TESTING.md`; each named in H's
milestones): generation purity and border continuity (C); sparse-vs-dense
grid equivalence including result order (D); chunk coordinate conversion
(A); envelope validation (G); ledger materialise/dematerialise round-trip
and byte stability (F). Two harder tests earn their cost: a **two-run
determinism soak** — `LinkStubs.cpp` is empty, so `GameLogic` links into a
test DLL; construct a `Location` on a tiny procedural def, run N ticks of
`Advance` with a scripted cause list *twice in one process*, and compare
sync values tick by tick, which catches activation nondeterminism without a
second machine — and **shadow-mode dual verification** during the two
storage conversions: in Debug, keep the old whole-world structure alive
alongside the new one and assert equality on *every* query for a full
Garden session, which checks every access pattern the game actually makes
rather than the ones a test author thought of. Not unit-testable, said
plainly: frame time, streaming feel, hitches — owner-run territory, which
is what H's per-milestone smoke tests are for.

# Comparison table (Step 4)

Garden → naïve 65,536² → this design. Arithmetic, `cellSize` 16, worst-case
land fraction; "resident" = camera window (B) + active regions (F).

| Structure | Garden | Naïve 65,536² | Under this design |
|---|---|---|---|
| Heightmap + normals | 565 KB | 477 MB (@12) / 268 MB (@16) | ~67 MB resident (256 × 260 KB) |
| Landscape mesh | ~2.5 MB | 1.2–2.1 GB, 1 draw | ~300 MB resident, ~256 culled draws |
| `EntityGrid` | 8.1 MB | **8.6 GB** | ~12 MB (population-proportional) |
| `ObstructionGrid` | 24 KB | 25 MB + full recalc | ~41 KB × resident chunk, built on activation |
| Entities advanced / tick | 229 | ~245,000 | ≤ ~4,000 (budget, F) |
| Load-time terrain work | 35 K samples | 29.8 M samples | ~4.3 M (window) |
| Editor regen per change | whole map | whole map ×844 | resident window |

# Open questions — all resolved

The document went to the owner with seven open questions on 2026-08-06 and
all seven were answered the same day; an expert review later that day
raised three more, also answered. Each is now a decision record in the
table at the top; this table is kept as the record of what was asked and
what was chosen.

| # | Question | Answer | Decision |
|---|---|---|---|
| Q1 | **Garden baseline** — when are the three numbers captured? | Proceed on arithmetic; capture before M2 | D9 |
| Q2 | **Population shape** at the D5 total | Biome-clumped | D10 |
| Q3 | **Water/cloud fidelity** — accept camera-recentred constant-count grids? | Accept | D11 |
| Q4 | **`cellSize` 16** as the reading of D1? | Yes | D12 |
| Q5 | **Move `2dSurfaceMap.h` to `NeuronCore`?** | Yes, in M1 | D13 |
| Q6 | **Scope `SpiritReceiver`'s spawn**, accepting the `syncrand` shift? | Yes; record like `determinism` T5 | D14 |
| Q7 | **Dependency posture** — nothing external? | Confirmed, nothing | D15 |
| Q8 | **Dormant world look** — render proxies, or visibly empty? | Empty this milestone; the sync-inert proxy design recorded for reversal | D16 |
| Q9 | **Ledger fidelity** — what survives dormancy? | Full survivor state, ~0.1-unit fixed-point positions | D17 |
| Q10 | **Traversal time** — accept ~37 s sprint crossing? | Accepted; fast travel is open-world work | D18 |

---

*Analysis before design; argument before conclusion; the numbers are
arithmetic until D9's baseline capture makes them falsifiable at M2. A
reader with `AGENTS.md` and this document has the whole design and the
eighteen decisions it stands on; no question remains open. The
implementation DAG written from Part 2's milestones is
[`tasks/large-location.yaml`](../tasks/large-location.yaml) — nineteen
tasks in eight waves, the four milestone gates as owner-run nodes, M0
reached through `blocked_by` edges into
[`tasks/landscape-index-safety.yaml`](../tasks/landscape-index-safety.yaml).*
