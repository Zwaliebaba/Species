# Restarting the DirectXMath migration

Written 2026-08-04 at `e91734c`, the commit the work stopped on. Read
[`AGENTS.md`](../AGENTS.md) first — it is still the orientation document. This
file answers "where did the math migration get to, and what do I do next".

The plan is [`directxmath-migration.yaml`](directxmath-migration.yaml) and it is
still the plan. **12 of its 27 tasks are done.** Everything below it is either
recorded in a task's `notes` or reproducible from the commands quoted here.

---

## Where it stopped

The engine layers are converted and CI-green. `NeuronCore`'s math and geometry,
`NeuronClient`'s renderers and sound, and the wire types all compute on
`DirectX::XMFLOAT2/3`, `XMFLOAT3X3`, `XMFLOAT4X4` and `XMVECTOR`.

| Landed | |
|---|---|
| T1 | conventions, layout `static_assert`s, the transitional seam |
| T2, T3 | wire bytes and raw-memory layout pinned before anything moved |
| T4, T5, T6, T7, T8 | the geometry library — nine functions and the `Plane` class deleted, not migrated |
| T9 | the wire types |
| T10, T11 | the renderers and the sound subsystem |
| T13 | owner-run smoke test — **partial, see below** |

**The whole of GameLogic and `Species` is untouched.** That is T14–T20 and
T22–T23, and it is the bulk of the work: `Entity::m_pos` and `m_vel` alone are
2,294 occurrences across 109 files.

---

## Do this first

**Run the six checks, then read T10's and T20's `notes` in the plan.** Between
them they carry everything that cost this migration a wasted round.

```bash
python3 tools/check_math_types.py     # written for exactly this migration
python3 tools/check_task_dag.py --next tasks/directxmath-migration.yaml
```

---

## The five ways this conversion breaks a file you did not touch

Four of these cost a red CI round in T10 alone, and T14–T20 convert hundreds of
members rather than nine. `tools/check_math_types.py` catches 1, 2, 4 and 5.

| | What | Why grep misses it |
|---|---|---|
| 1 | The type name | — this one is easy |
| 2 | A converted **member's** name | `vel.Mag()` contains no type name |
| 3 | A **transitive include** | the header stops supplying `Vector3` to files that got it through it |
| 4 | The **replaced type's field** names | `GetWorldMatrix(m).pos` — `XMFLOAT4X4` numbers its rows `_11.._44` |
| 5 | **Default construction** | `Vector3()` zeroed; `XMFLOAT3()` does not |

Number 5 is the dangerous one: invisible to the compiler *and* to CI. It shipped
twice before anything noticed — `Shape::CalculateCentre` accumulating fragment
centres onto uninitialised memory, and the sound system's cached positions with
nothing at all to catch it. Number 3 is the one the checker cannot see; it needs
a compiler.

---

## The structural rule, learned three times

**A task that owns only call sites and no storage cannot convert
independently.** T12, T10's `GetWorldMatrix` and T20 each cost a discovery round
to learn this the same way.

- **T12** (the `*Access` headers) could not be done *at all* as scoped: a
  virtual override must match its base exactly, so an interface signature cannot
  move ahead of its implementors, and no implicit conversion is consulted. It
  now depends on T18 and T22.
- **T20** (the in-game windows) declares no math member whatsoever — every
  `Vector3` in it is a local feeding `CameraAccess::GetClickRay`,
  `Landscape::RayHit`, `Building::GetPortPosition` or `Location::SpawnEntities`.
  It now depends on its callees.

**Before scheduling any remaining group as parallel, check it declares a
member.** It is one grep and it saves a round.

---

## The seam, and the two places still holding legacy types

`Vector3`, `Vector2` and `Matrix34` each convert implicitly to and from their
native counterpart. That is what lets a converted file compile against an
unconverted API, and it is why T10 did not have to edit the 89 call sites that
pass a `Matrix34` into `Shape`'s methods.

**T25 deletes all of it**, and until T25 lands the migration is not finished
however good the intermediate state looks. Two signatures are deliberately still
legacy and both are commented in place:

- `ShapeMarker::GetWorldMatrix` returns `Matrix34` — 43 sites across fourteen
  GameLogic files read `.pos` or `.f` off it.
- Two `Vector3` locals in `SoundInstance.cpp` feed
  `LocationAccess::GetSoundSource`.

Four external files carry commented seam repairs that convert *back* on purpose:
`Species/Renderer.cpp`, `GameLogic/RadarDish.cpp`, `GameLogic/Explosion.cpp` and
`Tests/NeuronCoreTests/NetworkUpdateTests.cpp`. `RadarDish` especially —
it is simulation code where `Matrix34::RotateAround`'s exact semantics matter.

---

## One open question

~~**1. The Garden landscape may have changed shape.**~~ **Answered 2026-08-04:
the height-map checksums are equal, so the terrain is bit-identical and the
difference was in rendering or in the eye.** `57386fb`, the temporary checksum
dump, is reverted. The condition it attached to the GameLogic wave is
discharged.

**2. `LandscapeRenderer::GetLandscapeColour` reseeds the simulation RNG from
rendering code.** Unrelated to the above, predates this plan, unrecorded until
now. Belongs in `tasks/determinism.yaml` once somebody establishes what it
costs.

---

## Working habits this migration settled on

- **Whole-file reformat first, in its own commit.** The changed-lines format
  check makes an insertion drag its neighbours in, and fixing those drags in
  theirs. Four commits on this branch exist only to do this. It is the same
  remedy `language-hygiene` T3 and T4 used.
- **CI is the compiler.** No agent here can run MSVC, so every claim about
  building is a CI run number. Say which one.
- **Write the plan file after every task, not once at the end.** A script that
  built the whole file in memory and wrote at the end lost four status updates
  when it asserted out — and the plan still validated, so no check caught it.
  `--next` reporting "no task is ready" was the only signal.
