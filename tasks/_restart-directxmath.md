# Restarting the DirectXMath migration

Rewritten 2026-08-04 after T18. Read [`AGENTS.md`](../AGENTS.md) first — it is
still the orientation document. This file answers "where did the math migration
get to, and what do I do next".

The plan is [`directxmath-migration.yaml`](directxmath-migration.yaml) and it is
still the plan. **20 of its 28 tasks are done** (2026-08-05: T19 landed and
T21, the owner's Garden run, closed with it).** Everything below is either
recorded in a task's `notes` or reproducible from the commands quoted here.
(It was 27 until 2026-08-05, when the first of the two gaps below became T28.)

---

## Where it stopped

The engine layers and most of GameLogic are converted and CI-green. `NeuronCore`'s
math and geometry, `NeuronClient`'s renderers and sound, the wire types, and the
entities, buildings, creatures, world, landscape and routing all compute on
`DirectX::XMFLOAT2/3`, `XMFLOAT3X3`, `XMFLOAT4X4` and `XMVECTOR`.

| Landed | |
|---|---|
| T1 | conventions, layout `static_assert`s, the transitional seam |
| T2, T3 | wire bytes and raw-memory layout pinned before anything moved |
| T4–T8 | the geometry library — nine functions and the `Plane` class deleted, not migrated |
| T9 | the wire types |
| T10, T11 | the renderers and the sound subsystem |
| T13 | owner-run smoke test on the converted engine layers |
| T14 | the entity core and the humanoid entities |
| T15 | the creature entities |
| T16 | the structural buildings — **the keystone of the GameLogic wave** |
| T17 | the functional buildings |
| T18 | the world, landscape and routing |
| T24 | the tests |

**What is left in GameLogic is T19 and T20**; `Species/` is untouched, which is
T22–T23, and T22 is in progress. `--next` offers T22 and T23 today. T28 (added 2026-08-05) closes the
first of the two gaps below and sits between T20/T22 and the deletion.

**T19 IS SEVEN NINTHS DONE, and its `notes` are the handover.** ParticleSystem,
Explosion, Spirit, Flag, TaskManager, Airstrike and Spam are converted and
CI-green; the five files that had drifted from clang-format were reformatted
first, in their own commit. Weapons and Rocket remain, and the task records them
by EDIT POINT rather than type name — roughly 246 and 161 — because the type
count understates them about threefold. Weapons.cpp alone holds 56 of the
tree's remaining `AsLegacy` uses. The two are independent of each other.

---

## Do this first

```bash
python3 tools/check_math_types.py
python3 tools/check_task_dag.py --next tasks/directxmath-migration.yaml
```

Then read the `notes` on **T15, T16 and T18**. T16 has the keystone rule, T15
has the fallback decisions, and T18 has the two gaps in the plan itself.

---

## The eight ways this conversion breaks a file you did not touch

The first five were known after T10. T14 added 6 and 7, T19 added 8, and every
one of them has since cost at least one red round. `tools/check_math_types.py`
catches all but 3 and 8.

| | What | Why grep misses it |
|---|---|---|
| 1 | The type name | — this one is easy |
| 2 | A converted **member's** name | `vel.Mag()` contains no type name |
| 3 | A **transitive include** | the header stops supplying `Vector3` to files that got it through it |
| 4 | The **replaced type's field** names | `GetWorldMatrix(m).pos` — `XMFLOAT4X4` numbers its rows `_11.._44` |
| 5 | **Default construction** | `Vector3()` zeroed; `XMFLOAT3()` does not — and `Matrix34` had no fourth column where `XMFLOAT4X4` has one |
| 6 | A contended name that **is not a math type at all** | `ArmyAnt::m_orders` is an `int`, `m_mousePos` an `int[3]`, `m_right` a `float*` |
| 7 | **The address-of trap** | the seam converts by REFERENCE, which does nothing through a pointer |
| 8 | **A legacy GLOBAL** | `m_up = g_upVector` compiles; `XMLoadFloat3(&g_upVector)` does not — same trap, other direction |

Number 8 is 7 seen from the other side and was hit five times in one file during
T19. `g_upVector` and `g_zeroVector` are still `Vector3` and T25 owns retiring
them. Until then, use what the converted files use: `DirectX::g_XMIdentityR1`
for `g_upVector`, which is (0,1,0,0), and `DirectX::XMVectorZero()` for
`g_zeroVector`. Assignment through the seam is fine; taking the address is not.

Number 5 is the dangerous one: invisible to the compiler *and* to CI. It shipped
three times before anything noticed — `Shape::CalculateCentre` accumulating
fragment centres onto uninitialised memory, the sound system's cached positions
with nothing at all to catch it, and then `ShapeMarker::m_transform`.

**That third one is the shape to remember, because it is not the zeroing
difference at all.** `Matrix34` was twelve floats: r, u, f and pos, with no
fourth column, and `ToNative()` supplied `(0,0,0,1)` for the one it did not
have. `XMFLOAT4X4` has that column for real and nothing fills it in. The
shape-file parser writes nine floats — front, up and pos — so `_14`, `_24`,
`_34` and `_44` kept whatever was on the stack, and `XMMatrixMultiply` reads all
sixteen: `row3 = _41*r0 + _42*r1 + _43*r2 + _44*r3`, so a `_44` of about -1e9
times a building position of about 1200 put every marker at roughly -1.2e12.

It surfaced as two unrelated-looking reports — a research item that stopped
appearing (`Library::Advance` spawns it *at* a marker's world position) and a
`GenerateSyncValue` assert on a total of -2.5e12 — and both were this. **A
converted type that is WIDER than the one it replaced needs the same audit as
one that stopped zeroing: find every writer and check it fills the new fields.**
`tools/check_math_types.py` now checks matrix members too.

Number 7 is the one that shapes the remaining tasks. `&someVector3` is a
`Vector3*` and will not bind to an `XMFLOAT3*`, so **any API with an
out-pointer cannot convert until both ends convert together**. `AsLegacy` is the
escape hatch where the callee is staying legacy: `&AsLegacy(*_pos)` is a
`Vector3*` onto native storage.

---

## The structural rule, learned four times

**A task that owns only call sites and no storage cannot convert
independently**, and **a virtual signature cannot move without every overrider
in the same commit**.

- **T12** (the `*Access` headers) could not be done at all as scoped, and now
  depends on T18 and T22.
- **T20** (the in-game windows) declares no math member whatsoever, and now
  depends on its callees.
- **T16** turned out to be the keystone of the whole GameLogic wave and the plan
  did not say so: `Building` declares five virtuals carrying math types,
  overridden in **fifteen headers spread across three tasks**. That is why T16
  had to land before T15 and T17 whatever `parallel_safe` claimed.
- **T18** hit it again from the other side — see the gaps below.

**Before scheduling any remaining group as parallel, check it declares a member
and map its virtuals in both directions.** It is two greps and it saves a round.

---

## Two gaps in the plan itself, found by doing T18

Both blocked work the plan assumes is possible. **The first now has an owning
task; the second is still unowned but is already fixed in the tree.**

**1. MathUtils' geometry API had no converting task. It is now T28.** T7 and T8
rebuilt `RayTriIntersection` and `RayRayDist` on DirectXMath but deliberately
kept their `Vector3` signatures so callers compiled unchanged — that is written
into both tasks' acceptance. Nothing after them converted the signatures, and
`PointSegDist2D` and `SegRayIntersection2D` still take `Vector2 const&` and
`Vector2*`. **T25 then deletes both classes**, which it cannot do while they
are named in a signature.

This is what stops `Landscape`'s ray and sphere API moving: those functions pass
their out-pointer straight into MathUtils, and the call sites pass
`&someVector3` into `Landscape::RayHit`. Both ends are stuck on failure mode 7.

**T28 owns both APIs and their callers**, added 2026-08-05 on the owner's
instruction — the decision T25's notes deferred rather than took. It depends on
T19, T20 and T22, which are the three tasks whose files still hold legacy
storage *and* call these routines; every other caller already reaches them
through `AsLegacy`, so for those files T28 removes seam calls rather than
adding them. **T25 now depends on T28**, which puts the deletion in wave 10
rather than wave 8. The counts in T28's notes were re-measured rather than
inherited: the Landscape family has 20 caller files, not ten, and
`RaySphereIntersection` alone has 13.

**2. `SurfaceMap2D` was never mentioned in the plan.** `Landscape::m_normalMap`
was a `SurfaceMap2D<Vector3>`, so `Vector3` could not be deleted while it stood,
and the element type could not change because the generic `GetValue`
interpolates with `T * float` and `T + T`. Fixed during T18 —
`NeuronClient/2dSurfaceMap.h` gained an explicit `GetValue` specialisation for
`XMFLOAT3` — but it is a file outside that task's list *and* outside its
project, so it landed in its own commit. Recorded here because the plan should
have owned it.

---

## The seam, and what is still holding legacy types

`Vector3`, `Vector2` and `Matrix34` each convert implicitly to and from their
native counterpart. That is what lets a converted file compile against an
unconverted API. **T25 deletes all of it**, and until T25 lands the migration is
not finished however good the intermediate state looks.

`grep -rl AsLegacy` is the live worklist: **19 files** carry one today and
there are **113 uses** between them. Count USES, not matching lines — the two
differ because several lines carry more than one, and this file said 23/56 when
it meant lines. Weapons.cpp held 71 of them until T19 converted it; it now
holds exactly one, `Landscape::RayHit`'s out-pointer, which is T28's.

Signatures deliberately still legacy, all commented in place:

- `ShapeMarker::GetWorldMatrix` returns `Matrix34` — 143 call sites.
- `LocationAccess::GetSoundSource` and `Location`'s override of it — T12.
- `CameraAccess`'s `GetPos`/`GetFront`/`GetUp`/`GetRight`/`GetClickRay`/
  `Get2DScreenPos` and `UserInputAccess::GetMousePos3d` — pure virtuals, T12/T22.
- `Landscape::RayHit`, `RayHitCell`, `UnsafeRayHit`, `SphereHit`,
  `IsInLandscape` — **T28**, together with the MathUtils routines below, which
  is why one task owns both.
- MathUtils' `RayTriIntersection`, `RaySphereIntersection`, `RayRayDist`,
  `PointSegDist2D`, `SegRayIntersection2D` — **T28**.

---

## `NeuronClient/GlVertex.h`

`EmitVertex(FXMVECTOR)` bridges an `XMVECTOR` to a legacy OpenGL immediate-mode
vertex. The old code wrote `glVertex3fv(v.GetData())`, which was sound because a
`Vector3` *is* three packed floats; an `XMVECTOR` is four lanes in a register
with no address to take, so every call site round-trips through an `XMFLOAT3`.

**It is NOT transitional** and outlives T25 — it bridges DirectXMath to OpenGL,
not old math to new. T10 and T14 had grown five byte-identical file-static
copies before T16 folded them onto it. **33 files use it; 188 `glVertex3fv`
call sites remain** in unconverted files. Reach for it rather than writing a
sixth copy.

---

## `tools/check_math_types.py`

Written after five CI failures in a row in T10 and grown by every task since. It
now covers members, locals, **parameters**, `XMVECTOR`s, matrix rows, equality,
compound assignment, typed receivers and renames. Each rule was added for a real
bug and verified by re-introducing that bug.

**Its governing doctrine, which every change to it has had to respect:**
under-reporting is recoverable; crying wolf gets the tool switched off. A name
that means two things is **skipped and counted**, never guessed at — sixteen are
contended today, including `m_pos`, `m_front` and `m_up`.

**Every gap ever found in it closed the same way: a NARROWER claim about where a
name's type is known, not a broader regex.** A contended member is checked in
the one file whose header binds it, for bare uses only. An `XMFLOAT4X4`
contended with an `XMMATRIX` gives up only `.r`, because those are the only rows
`XMMATRIX` has. A typed receiver — `cmnt->m_front` where `cmnt` is a
`CameraMount*` and `CameraMount` is declared in the paired header — removes the
ambiguity outright. Two rules that widened instead, array members and the first
cut of the per-file rescue, were measured, found to accuse correct lines, and
refused.

It also catches things the compiler does not: a member accumulated into without
an initialiser, and a rename that left a use behind.

---

## One open question

**`LandscapeRenderer::GetLandscapeColour` reseeds the simulation RNG from
rendering code.** Predates this plan. Belongs in `tasks/Archive/determinism.yaml` once
somebody establishes what it costs.

---

## Finding the sites: three nets, none sufficient alone

T19 settled this after two red builds that all seven local checks had passed.
The type grep, the member grep and `check_math_types` have **different** blind
spots, and a file is only clean when all three are.

```bash
# 1. the type names — misses everything that does not spell one
grep -nE '\b(Vector3|Vector2|Matrix33|Matrix34|AsLegacy)\b' <file>

# 2. the converted MEMBERS — legacy methods and operators the native types lack
grep -nE '\.(Mag|MagSquared|Normalise|SetLength|SetToIdentity|Set|Zero|GetData|GetDataConst|RotateAround|FastRotateAround)\s*\(' <file>
grep -nE '\bm_[a-zA-Z]+\b\s*(\*=|\+=|-=|/=|\^)' <file>

# 3. the tool, which resolves types the greps cannot
python3 tools/check_math_types.py
```

Why all three. `Explosion::Advance` was missed by net 1 because
`m_pos += m_vel * g_advanceTime` names no type, and by net 3 because `m_pos`
and `m_vel` were **contended names it skips rather than guesses at**. Net 2
found `.Set(` in TaskManager only after `Set` was added to it. Net 3 found the
cross-file break in `Species/TaskManagerInterfaceIcons.cpp` that neither grep
could see, and it found it only because that conversion had *just* made
`m_centre` unambiguous.

**Net 3 gets stronger as the migration proceeds** — every conversion removes a
contended name. Files converted early were checked less thoroughly than files
converted late, which is worth knowing if something surfaces in T14–T18 work.

**One hazard no net catches.** Replacing every occurrence of an identical block
puts duplicate declarations in any function containing two of them —
`Spirit::Render` draws two quads differing only in size and got two `sizeVec`
declarations in one scope. Only the compiler sees it. A per-function scan for
repeated local names helps, but it false-positived nine times out of nine on
sibling `for`/`if` bodies in Spam and Airstrike, so read the braces on each hit.

---

## Working habits this migration settled on

- **Whole-file reformat first, in its own commit.** The changed-lines format
  check makes an insertion drag its neighbours in, and fixing those drags in
  theirs.
- **CI is the compiler.** No agent here can run MSVC, so every claim about
  building is a CI run number. Say which one.
- **Write the plan file after every task, not once at the end.**
- **Map a class's inheritance in BOTH directions before converting it.** T16 is
  the reason.
- **State a fallback decision at the site.** `Vector3::Normalise` answered zero
  with `(0,0,1)` and `SetLength` with `(len,0,0)`; the native routines do not.
  T1 decided not to reproduce them by default, and roughly twenty sites in T15
  alone were reachable enough to need the exception — each commented with why.
- **After merging `main`, commit the merge before running `check_format`.** It
  diffs against `merge-base(HEAD, origin/main)`, so until the merge is committed
  `main`'s own unformatted lines are reported as yours. Do not "fix" them.
