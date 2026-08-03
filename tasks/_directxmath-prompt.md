# Prompt: plan the DirectXMath migration

Copy everything below the rule into a fresh agent session on this repository
(branch of your choice). It produces a plan file under `tasks/`, not code
changes. The figures embedded in it were measured on 2026-08-03 at `4eeafa6`;
the prompt instructs the agent to re-measure rather than trust them.

The four decisions in *Decisions already made* were taken by the project owner
before this prompt was written. They are not open. An agent that re-opens them
is wasting the session.

---

You are working in the **Species** repository, a ~113k-line C++20 Windows game
(six MSBuild projects) partway through being modernised from the inherited
Darwinia source into Neuron-style C++20 with enforced layer boundaries.

## Mission

Produce a **complete plan for migrating the hand-rolled math library to native
DirectXMath**, expressed as a task-DAG YAML file under `tasks/`, conforming to
the schema in `docs/TASK_DAG.md` and validating clean under
`python3 tools/check_task_dag.py`.

The end state: `NeuronCore/Vector3.h`, `Vector2.h`, `Matrix33.h`, `Matrix34.h`
and `Plane.h` are **deleted**, and the tree computes on `DirectX::XMFLOAT2/3`,
`XMFLOAT3X3`, `XMFLOAT4X3`, `XMVECTOR` and `XMMATRIX` directly. Not a typedef
over the wrapper. Not a wrapper over DirectXMath. The wrapper classes and their
operators are gone, and call sites carry the load/compute/store discipline
DirectXMath is designed around.

This session is **planning only**. You write, validate and commit the plan file.
You do not convert any code and you do not start executing tasks. The plan must
be executable incrementally, by multiple agents over an extended period — that
is what the DAG structure is for.

## Decisions already made

Do not re-ask these. Record them in the plan `summary` so the executing agents
inherit them.

1. **Determinism: a one-off divergence is accepted.** DirectXMath's SIMD lane
   arithmetic does not reproduce the current scalar arithmetic bit-for-bit, so
   this migration *will* change what the simulation computes. That is
   sanctioned here, exactly as `tasks/determinism.yaml` T1 was: the rule being
   suspended is "the new build matches the old build", **not** "every client in
   a session agrees". Clients must still be bit-identical to each other on the
   same build, and mixed-version play is expected to desync. The migration
   therefore covers the whole tree, `GameLogic` and `Location::Advance`
   included.
2. **Target style: `XMVECTOR`/`XMFLOAT3` everywhere.** Storage is the
   `XMFLOAT*` types; arithmetic is explicit `XMLoad*` → `XMVector*` → `XMStore*`
   at the call site. No wrapper class, no operator-overload shim layer, no
   `using Vector3 = DirectX::XMFLOAT3;` surviving into the final state. A
   transitional alias inside a single task is fine if it keeps the tree
   compiling between tasks; a task at the end of the plan must delete it.
3. **Deliverable: the plan first.** No code this session.
4. **The geometry library moves to DirectXMath/DirectXCollision** where an
   equivalent exists — `XMPlane*`, `BoundingSphere`, `BoundingBox`,
   `TriangleTests` — rather than being rewritten by hand on the new types. Where
   no equivalent exists, the algorithm is rewritten on native types.

## The constraint that survives

Determinism *within a build*. `Species/Main.cpp` `GenerateSyncValue()` sums
every unit's, entity's, laser's and effect's `m_pos` and `m_vel` in container
index order, folds to a byte, and the server asserts every client agrees. After
this migration the sum is a different number than it was — that is expected and
sanctioned. What is not acceptable is a change that makes two clients on the
*same* build disagree. The two ways this plan can do that:

- **Cross-architecture.** ARM64 is the primary development platform and x64 is
  what CI builds. DirectXMath dispatches to ARM-NEON on one and SSE/SSE2/AVX on
  the other, and `AGENTS.md` already records cross-architecture play as
  unproven with 335 `sinf`/`cosf`/`sqrtf`-family calls in the tree. This
  migration makes that risk concrete instead of theoretical. The plan must
  address it explicitly — `_XM_NO_INTRINSICS_` for a portable scalar path,
  a pinned `<FloatingPointModel>`, or a documented decision to accept it — and
  anything that touches project settings is a build-topology question to ask
  before writing (see Step 4).
- **Estimate-based intrinsics.** `XMVector3NormalizeEst`, `XMVectorReciprocalEst`
  and friends are permitted to differ between implementations. Ban the `*Est`
  family outright in simulation code and say so in the plan's summary.

Read `CODING_STANDARDS.md#determinism` in full before writing a single task.

## Step 0 — read before planning

In this order, completely:

1. `AGENTS.md` — current priority, scope, known issues, decisions already taken
   and declined.
2. `CODING_STANDARDS.md` — target style, migration stages, determinism.
3. `docs/TASK_DAG.md` — the schema you are writing to.
4. `docs/TESTING.md` — what earns a test, characterization-first conversion.
5. `docs/ARCHITECTURE.md` — layers, runtime model.
6. `tasks/_modernization-prompt.md` and `tasks/_restart.md` — how plans in this
   repo are shaped and where the modernisation currently stands. Four plans are
   open; your plan has to say how it sequences against them, because it touches
   files they also touch.
7. `tasks/neuroncore-layering.yaml` and `tasks/containers-replaced.yaml` — the
   exemplars. Match their standard: counts cited with the command that produced
   them, observable acceptance criteria, honest notes.

## Step 1 — survey and re-measure

Counts drift. **Re-measure everything you cite.** Figures as of 2026-08-03
(`4eeafa6`), Linux, with `P="NeuronCore NeuronClient NeuronServer GameLogic
Species Server Tests"`:

| Axis | Measured | Command |
|---|---|---|
| `Vector3` | 3,133 occurrences, 207 files (GameLogic 140, NeuronClient 29, Species 19, NeuronCore 16, Tests 3) | `grep -row Vector3 --include=*.cpp --include=*.h $P \| wc -l` |
| `Matrix34` | 489 occurrences, 79 files | same, `-w Matrix34` |
| `Vector2` | 220 occurrences, 22 files | |
| `Matrix33` | 107 occurrences, 10 files | |
| `Plane` | 20 occurrences, 4 files | |
| `glVertex3fv`/`glNormal3fv` fed from `GetData()` | 46 files | `grep -rlEw 'glVertex3fv\|glNormal3fv' --include=*.cpp $P` |
| `ConvertToOpenGLFormat` call sites | 3 (`GameLogic/Tree.cpp`, `NeuronClient/Shape.cpp` ×2) | `grep -rnw ConvertToOpenGLFormat --include=*.cpp $P` |
| `glMultMatrixf` | 3 | |
| Math library size | 2,718 lines across 16 files | `wc -l NeuronCore/{Vector3,Vector2,Matrix33,Matrix34,Plane,MathUtils}.{h,cpp}` |
| Trig/sqrt/pow calls | 335 | `grep -rEow '\b(sinf\|cosf\|tanf\|acosf\|asinf\|atan2f\|sqrtf\|powf)\b' $P \| wc -l` |
| Tests | 124 across 4 projects, none covering math | `grep -rc TEST_METHOD Tests/*/*.cpp` |

Before writing tasks, read at minimum: `NeuronCore/Vector3.h`,
`NeuronCore/Matrix34.h` and `.cpp`, `NeuronCore/MathUtils.h` and `.cpp`,
`NeuronCore/NetworkUpdate.cpp`'s `ReadByteStream`/`WriteByteStream`,
`Species/Main.cpp`'s `GenerateSyncValue()`, `GameLogic/LandscapeRenderer.cpp`
around line 232, and one representative entity (`GameLogic/Citizen.cpp`) so the
tasks are shaped by what the code actually does.

## Step 2 — the mapping the plan must define

The plan's first task is a **conventions document or header comment that fixes
every one of these decisions once**, before any sweep starts. Getting this wrong
mid-sweep means redoing files. Cover at least:

**Types.** `Vector3` → `XMFLOAT3` storage; `Vector2` → `XMFLOAT2`; `Matrix33` →
`XMFLOAT3X3`; `Matrix34` → `XMFLOAT4X3` or `XMFLOAT4X4` (pick one and justify —
`Matrix34` is four `Vector3`s: `r`, `u`, `f`, `pos`); `Plane` → a plane
`XMVECTOR` built by `XMPlaneFromPoints`/`XMPlaneFromPointNormal`. The globals
`g_upVector`, `g_zeroVector` and `g_identityMatrix34` need native equivalents
(`g_XMIdentityR1`, `XMMatrixIdentity()`) or replacements.

**Operators, and the traps in them.**

| Current | Native | Trap |
|---|---|---|
| `a ^ b` | `XMVector3Cross` | `^` is cross product here, not xor. Every `^` in a math expression is a call site. |
| `a * b` (Vector3) | `XMVector3Dot` | Returns an `XMVECTOR` with the result splatted across all four lanes, **not a float**. Every dot-product site needs `XMVectorGetX` or to stay in vector form. This is the single most common conversion error. |
| `a * f` (scale) | `XMVectorScale` | |
| `Normalise()` | `XMVector3Normalize` | **Behaviour differs on zero-length input.** The current one returns `(0,0,1)`; `XMVector3Normalize` returns zero or QNaN. Whether to preserve the fallback is a decision the plan must take deliberately, per call site if necessary — silently inheriting the DirectXMath behaviour will produce NaNs in the simulation. |
| `Mag()` / `MagSquared()` | `XMVector3Length` / `XMVector3LengthSq` | Same splatted-return trap. |
| `SetLength`, `HorizontalAndNormalise` | compose | `HorizontalAndNormalise` divides by `sqrtf(x*x+z*z)` with no zero guard — preserve or fix deliberately, and note it. |
| `RotateAroundX/Y/Z`, `RotateAround`, `FastRotateAround` | `XMMatrixRotation*` / `XMVector3Rotate` (quaternion) | `FastRotateAround` assumes a normalised axis; the quaternion path does too. |
| `operator ==` | `XMVector3NearEqual` | The current one is `NearlyEquals` at 1e-6 per component. Pin the epsilon; do not let it drift to `XMVector3Equal`. |
| `Matrix34 * Vector3` | `XMVector3Transform` | **Convention change.** The current matrices are column-vector (`M*v`, with `r`/`u`/`f` as columns); DirectXMath is row-major, row-vector (`v*M`). Decide the convention once, state it, and make every transform site obey it. Getting this wrong compiles cleanly and renders garbage. |

**The OpenGL seam.** `Matrix33::m_openGLFormat` and `Matrix34::m_openGLFormat`
are `static float[16]` — a shared mutable buffer returned by pointer, which is
neither reentrant nor thread-safe and is a bug waiting to be found. Replace with
a returned-by-value `XMFLOAT4X4`. OpenGL's `glMultMatrixf` wants column-major,
DirectXMath stores row-major: the transpose has to happen somewhere and the plan
must say where. `GetData()`/`GetDataConst()` feeding `glVertex3fv` survive as
`&v.x` because `XMFLOAT3` has identical layout — **verify that claim with a
`static_assert` rather than assuming it**, and note that `sizeof(Vector3) == 12`
is load-bearing in `GameLogic/LandscapeRenderer.cpp` (vertex buffer offsets) and
`GameLogic/TrunkPort.cpp` (a `memset` over a height map).

**The wire.** `NetworkUpdate::ReadByteStream` reads world position field by
field (`.x`, `.y`, `.z` via `READ_FLOAT`), so `XMFLOAT3` preserves the format —
but `NetworkUpdate` holds a `Vector3 m_direction` member and the packet layout
is a contract between machines. A byte-pinning test comes **before** the type
changes, as its own task, per `docs/TESTING.md`.

**Alignment.** `XMVECTOR` and `XMMATRIX` are 16-byte aligned and must not become
members of heap-allocated game objects casually — that is exactly why storage is
`XMFLOAT*`. Make "no `XMVECTOR` data members without an explicit alignment
decision" a rule in the plan. Function signatures taking vectors should use
`XM_CALLCONV` with `FXMVECTOR`/`GXMVECTOR`/`CXMVECTOR` in the documented order;
say so once rather than leaving each agent to guess.

**The geometry library** (`NeuronCore/MathUtils.cpp`, and
`NeuronClient/TriTri.cpp`). Produce the mapping table as part of the plan;
first-pass candidates:

| Current | Native |
|---|---|
| `RayTriIntersection` | `DirectX::TriangleTests::Intersects` |
| `TriTriIntersection` (`TriTri.cpp`) | `DirectX::TriangleTests::Intersects(tri, tri)` |
| `RaySphereIntersection`, `SphereSphereIntersection`, `SphereTriangleIntersection` | `DirectX::BoundingSphere::Intersects` |
| `RayPlaneIntersection` | `XMPlaneIntersectLine` |
| `GetPlaneMatrix`, `ConvertWorldSpaceIntoPlaneSpace`, `ConvertPlaneSpaceIntoWorldSpace` | compose from `XMPlane*` / `XMMatrix*` |
| `IsPointInTriangle`, `PointSegDist2D`, `SegRayIntersection2D`, `CalcTriArea`, `RayRayDist`, `RaySegDist` | no equivalent — rewrite on native types |

Two cautions here. The DirectXCollision routines are *different algorithms*, not
reimplementations, so out-parameters, edge-case behaviour and what counts as a
hit will differ — each replacement earns a test that states the new contract.
And `NeuronClient/TriTri.cpp` carries a third-party notice with **unresolved
terms** (Tomas Möller, 1997 — see `LICENSE`); deleting the file changes the
`LICENSE` table, so make it its own task and ask before doing it.

**Stays put.** `frand`/`sfrand`/`syncrand`/`syncfrand`, the Mersenne Twister in
`MathUtils.cpp` (BSD notice, must travel verbatim), `Log2`, `RampUpAndDown`, and
the `sign`/`signf`/`ClampInPlace`/`Round` macros — none of those are
DirectXMath's business. Converting the macros to functions is `language-hygiene`
work, not this plan; if you fold it in, say why.

**Build topology.** DirectXMath is a header-only part of the Windows SDK
(`<DirectXMath.h>`, `<DirectXCollision.h>`) — no new library to link and no
external dependency to vendor, and `NeuronCore/pch.h` already sets `NOMINMAX`
and the DirectX-oriented `NODRAWTEXT`/`NOBITMAP` defines. But `AGENTS.md` states
the project has "no third-party dependencies — it links only against the OS",
and `docs/BUILD.md` describes the toolchain; both need a doc task. Anything
beyond an `#include` — an SDK version floor, `<FloatingPointModel>`,
`_XM_NO_INTRINSICS_`, a new property in `Directory.Build.props` — is a
build-topology change and gets asked about first.

## Step 3 — how to shape the DAG

- **Bottom-up.** `NeuronCore` types and their tests → `NeuronCore` geometry →
  `NeuronClient` → `GameLogic` subsystem by subsystem → `Species`. The tree must
  compile at every task boundary; state the seam strategy explicitly (leaf-first
  with a conversion helper at the boundary, or a transitional alias deleted at
  the end) rather than leaving each agent to invent one.
- **One reviewable diff per task.** 3,133 `Vector3` sites is not one task and
  not ten. Group by subsystem so waves are wide; declare `files` on every task
  so collisions are visible; tree-wide sweeps are `parallel_safe: false`.
- **Characterization works differently here.** You cannot pin values across a
  change that deliberately alters the values. What you *can* pin, before
  touching anything: the wire bytes, `sizeof`/layout of the storage types,
  vertex-buffer offsets, and the packet round-trip. Do that first, as separate
  tasks. Then per-subsystem, the test that matters is "the new code agrees with
  the new code on both architectures", which is a smoke-test question, not a
  unit-test one.
- **The Garden smoke test gates the waves.** `AGENTS.md` *What working looks
  like* has the seven steps and the exact spawn counts (50 Citizens, 179 Virii).
  Only the owner can run it. Structure the plan so smoke-test gates fall at
  subsystem boundaries rather than after every task, and mark those tasks so it
  is obvious which ones cannot be closed by an agent alone.
- **Acceptance must be observable and countable** — "`grep -crw Vector3
  GameLogic/Citizen.cpp` returns 0", "`NeuronCore/Vector3.h` does not exist",
  never "is cleaner".
- **`verify` lists real commands**: all six `tools/` checks always, plus
  `msbuild` + `vstest` for anything that compiles. Agents on Linux can run only
  the Python checks and must say so.
- **Every task starts `status: todo`.**
- Standing rules apply: the four-edit `.vcxproj` rule for any file add or
  delete; `pch.h` first in every `.cpp`; licence notices travel verbatim; no
  upward includes and no reinstated allowlist; formatting is judged on changed
  lines, and a whole-file reformat is legal only as part of that file's
  deliberate conversion.
- **The last task deletes the wrapper headers** and proves it: `Vector3.h`,
  `Vector2.h`, `Matrix33.h`, `Matrix34.h`, `Plane.h` gone from disk and from
  `NeuronCore.vcxproj`, and `grep -rw 'Vector3\|Vector2\|Matrix33\|Matrix34'`
  over the tree returns nothing outside comments. Until that task lands the
  migration is not done, however good the intermediate state looks.

Sequence against the four open modernisation plans (`strings-modernised`,
`ownership`, `language-hygiene`, `namespace-migration`). They touch the same
files. Use `blocked_by` edges across plans where a file is contended, the way
`tasks/_restart.md` describes, and say in your summary which plan wins a
conflict.

## Step 4 — ask before you finalise

Answer from the tree what you can, and cite where. Ask via `AskUserQuestion` if
available; otherwise put an **Open questions** section in the plan `summary` and
mark dependent tasks `blocked`. At minimum resolve:

1. **Cross-architecture posture** — `_XM_NO_INTRINSICS_` for a portable scalar
   path, a pinned `<FloatingPointModel>`, or accept-and-document? This is a
   build-topology change either way and it decides whether ARM64 and x64 clients
   can play together at all.
2. **Matrix storage and convention** — `XMFLOAT4X3` vs `XMFLOAT4X4`, and
   row-vector vs column-vector. Recommend one; it is not reversible cheaply.
3. **`Normalise()`'s zero-length fallback** — preserve the `(0,0,1)` behaviour
   everywhere, preserve it only where a call site depends on it, or drop it?
4. **`TriTri.cpp` deletion** — the third-party terms are unresolved, so removing
   it is arguably an improvement to the licence position, but it is the owner's
   call and it edits `LICENSE`.
5. **Smoke-test cadence** — how often is the owner willing to run the Garden
   test? It is the only real verification this migration has, and the answer
   sets the wave size.
6. **Execution start** — once the plan validates, do agents begin wave 1, or
   does the plan await review?

## Step 5 — deliverables

1. `tasks/directxmath-migration.yaml` — valid under
   `python3 tools/check_task_dag.py`, every task `todo`, summary carrying your
   re-measured counts and the commands that produced them, plus the four
   decisions above and whatever Step 4 resolved.
2. A closing report: task count, `--waves` output, the open questions and what
   you assumed pending answers, and anything the survey turned up that changes
   the shape of the work.
3. Commit the plan file (and nothing else) to your working branch with a clear
   message. Do not push to `main`.

Honesty rules apply throughout: report what you actually ran; a compile is not a
test; on Linux you cannot build or run the game — say so rather than implying
you checked.
