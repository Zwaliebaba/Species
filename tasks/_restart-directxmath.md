# The DirectXMath migration is code-complete

Rewritten 2026-08-05, after T25 deleted the wrapper headers. Read
[`AGENTS.md`](../AGENTS.md) first — it is still the orientation document. This
file answers "is the math migration finished, and what is left".

**27 of the plan's 28 tasks are done.** The one that is not is
[`T27`](directxmath-migration.yaml), and it is not code: the owner runs the
Garden smoke test on the wrapper-free build. **Nothing else in the plan is
open, and no agent can close T27.**

---

## What the tree looks like now

There is no Neuron math type. `Vector2`, `Vector3`, `Matrix33`, `Matrix34` and
`Plane` are deleted from disk and from `NeuronCore.vcxproj`. Storage is
`DirectX::XMFLOAT2`, `XMFLOAT3`, `XMFLOAT3X3` and `XMFLOAT4X4`; arithmetic is
`XMLoad` / `XMVector*` / `XMStore` at the call site.
[`NeuronCore/NeuronMath.h`](../NeuronCore/NeuronMath.h) holds the conventions,
one function (`BasisFromFrontAndUp`) and two constants (`g_upVector`,
`g_zeroVector`) — and no class.

```bash
python3 tools/check_math_types.py
grep -rn AsLegacy --include=*.cpp --include=*.h NeuronCore NeuronClient GameLogic Species Tests
```

The second returns nothing. The seam is gone.

---

## T27: what the owner is being asked to check

The plan's own argument for why a green build is not enough:

> T25 deletes the seam, which changes how every converted call site resolves
> its types. A clean build is not evidence that it still runs.

That is the honest version, and it understates one thing. **Three of this
plan's worst bugs were invisible to the compiler AND to CI, and all three were
found by a human looking at the screen:**

- `Shape::CalculateCentre` accumulating fragment centres onto uninitialised
  memory, because `XMFLOAT3` does not zero itself and `Vector3` did.
- `ShapeMarker::m_transform` leaving `_44` as stack garbage, which put every
  shape marker at roughly -1.2e12 and stopped a research item appearing.
- `Tree::RenderBranch` crossing two identical vectors, getting exactly zero,
  and normalising it — every tree in the game rendered as an invisible
  zero-width line.

So the seven Garden steps are the test. Watch particularly for **anything that
renders in the wrong place, at the wrong size, or not at all** — that is the
shape all three took.

Record the run in `AGENTS.md` with its commit hash, as the previous two were,
and note that it is the post-migration baseline: from here the sync value is
whatever native math produces, and any future divergence is measured against
that build rather than against anything before it.

---

## The conventions, and why they outlive the plan

These are now in [`CODING_STANDARDS.md`](../CODING_STANDARDS.md#native-math)
rather than here, because they are standing rules rather than migration notes.
The short version:

| | |
|---|---|
| Multiplication | **row-vector, `v * M`** — rows are right / up / front / position |
| Handedness | **right-handed**, `*RH` variants only, and it stays that way |
| `*Est` intrinsics | **banned in simulation code** — permitted to differ between implementations, which is a desync on one build |
| Zero-length normalise | **native behaviour by default**; reproduce the old `(0,0,1)` only where the zero case is reachable, and say why at the site |
| Graphics APIs | **no math type knows which one it feeds** — the transpose and the depth range live in renderer code |

Two facts about the legacy classes are worth keeping even though the classes
are gone, because they are the reason two conversions in this plan were wrong
before they were right, and an old commit or a screenshot comparison may still
raise them:

- `Matrix33::RotateAroundX/Y/Z(a)` applied as `v * mat` rotated `v` by **+a**;
  `FastRotateAround(n, a)` and `RotateAround(n, a)` rotated the matrix's three
  basis ROWS by +a, which transposes into rotating a vector by **-a**.
- `Matrix33(yaw, dive, roll)` composed **Y * X * Z**, not the Z * X * Y that
  `XMMatrixRotationRollPitchYaw` computes and that the constructor's call order
  reads like.

---

## Mixed-architecture play is not supported

Decided, not unproven. DirectXMath dispatches to SSE on x64 and ARM-NEON on
ARM64, the two are not bit-identical, and deterministic lockstep requires that
they are. Owner decision, 2026-08-03: accept it rather than force the scalar
path. **Within one architecture the simulation stays deterministic**, which is
what the sync assert tests and what the migration was required to preserve.

---

## What this plan taught, for the plans that come after it

`strings-modernised`, `ownership`, `language-hygiene` and `namespace-migration`
are still open and touch the same files. Five things cost this plan a round
each, and none of them is specific to math:

1. **A task that owns only CALL SITES and no STORAGE cannot convert
   independently.** T12, T20 and T10's `GetWorldMatrix` each learned this
   separately. Check for a declared member before scheduling a group.
2. **A virtual signature cannot move without every overrider in the same
   commit.** No implicit conversion is consulted, and a mismatch stops
   overriding silently rather than failing loudly. Map a class's virtuals in
   BOTH directions before touching it. T16 and T22 were both keystone commits
   for this reason and neither plan-file entry said so in advance.
3. **A CONVERSION SEAM is bidirectional for values and one-directional for
   pointers**, and T22's map got that backwards — it predicted twelve repair
   files and needed five. What actually breaks is arithmetic on a converted
   result and anything behind a pointer. Nothing else.
4. **Storage moves first; the signature follows it.** T20 deliberately ADDED
   eleven seam calls so that T28 could delete them along with the signature.
   The reverse order does not work.
5. **Whole-file reformat first, in its own commit.** The changed-lines format
   check makes an insertion drag its neighbours in.

And one that is about tools rather than tasks:
`tools/check_math_types.py` found **128** row accesses on the locals T25
converted from `Matrix34` to `XMFLOAT4X4` — `.pos`, `.f`, `.u` and `.r` on a
type that numbers its rows `_11` to `_44`. Every one was a real compile error
waiting to happen, in files no human was reading. **Its governing doctrine is
still the right one: under-reporting is recoverable, crying wolf gets the tool
switched off.** Every gap ever found in it closed with a NARROWER claim about
where a name's type is known, never a broader regex.
