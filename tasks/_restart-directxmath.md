# The DirectXMath migration is finished

Written 2026-08-05, when T27 closed. Read [`AGENTS.md`](../AGENTS.md) first —
it is still the orientation document.

**All 28 tasks of [`Archive/directxmath-migration.yaml`](Archive/directxmath-migration.yaml)
are done.** The tree is CI-green at `1af4979` (runs 543 and 544), and the owner
ran the Garden smoke test on that build and reported it successful. AGENTS.md
records it as the post-migration baseline.

This is not a restart note any more. It is kept for two things: what the tree
looks like now, and what the migration cost that the four still-open plans are
about to pay again.

---

## What the tree looks like now

There is no Neuron math type. `Vector2`, `Vector3`, `Matrix33`, `Matrix34` and
`Plane` are deleted from disk and from `NeuronCore.vcxproj`. Storage is
`DirectX::XMFLOAT2`, `XMFLOAT3`, `XMFLOAT3X3` and `XMFLOAT4X4`; arithmetic is
`XMLoad` / `XMVector*` / `XMStore` at the call site.
[`NeuronCore/NeuronMath.h`](../NeuronCore/NeuronMath.h) holds the conventions,
one function (`BasisFromFrontAndUp`) and two constants (`g_upVector`,
`g_zeroVector`) — and no class.

The standing rules live in
[`CODING_STANDARDS.md`](../CODING_STANDARDS.md#native-math), not here:
row-vector `v * M`, right-handed, no `*Est` in simulation code, no math type
naming a graphics API, and every converted member explicitly initialised.

**Parameter style, settled at the end of the plan so it does not get
relitigated per file:**

| The value is… | Take it as |
|---|---|
| storage a caller holds in memory — an entity's `m_pos`, a marker's world position | `DirectX::XMFLOAT3 const&` |
| a link in a computation the caller is already doing in registers | `DirectX::FXMVECTOR`, **and the function must be `XM_CALLCONV`** |

`FXMVECTOR` without `XM_CALLCONV` is the one combination to avoid: it asks for
a register type and then passes it by the default convention, which is the
cost of both and the benefit of neither. Everything in the tree that takes an
`FXMVECTOR` is `XM_CALLCONV` as of 2026-08-05.

Do **not** convert the storage-facing APIs to `FXMVECTOR` as a tidy-up. The
`*Access` interfaces are virtual, and the geometry and landscape routines are
called with members straight out of memory — moving the `XMLoadFloat3` from
ten callee bodies to two hundred call sites is a loss, and several of those
callees only read `.x` and `.z` and would spill the register immediately.

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
   result, and anything behind a pointer. Nothing else.
4. **Storage moves first; the signature follows it.** T20 deliberately ADDED
   eleven seam calls so that T28 could delete them along with the signature.
   The reverse order does not work.
5. **Whole-file reformat first, in its own commit.** The changed-lines format
   check makes an insertion drag its neighbours in.

And the one that is about people rather than code: **the smoke test found what
nothing else could, twice.** Seven local checks, green CI and 180 passing tests
never render a tree. `Tree::RenderBranch`'s zero-length normalise and
`ShapeMarker`'s uninitialised fourth column were both found by the owner
looking at the game. Keep the gate.

---

## `tools/check_math_types.py`, and the one gap left in it

It earned its keep to the end. In T25 it found **128** row accesses on locals
converted from `Matrix34` to `XMFLOAT4X4` — `.pos`, `.f`, `.u` and `.r` on a
type that numbers its rows `_11` to `_44`. Every one was a compile error
waiting to happen, in files nobody was reading.

**But it checks MEMBERS and cannot see METHODS**, so a call CHAINED onto a
converted result is invisible to it. Three got past it in T25, on the very
expressions where it had flagged the member access: `Mine.cpp`'s five
`RotateAroundF` calls, and `EntityLeg`'s `.pos.Mag()`. CI found the first; a
hand sweep found the second. So the tool is not the whole net — grep the
legacy method names too:

```bash
grep -rnE '\.(Mag|MagSquared|Normalise|SetLength|SetToIdentity|Set|Zero|GetData|RotateAround[RUFXYZ]?|FastRotateAround|Orient[RUF][RUF]|Transpose|Invert|ToNative)\s*\(' --include=*.cpp --include=*.h .
```

Most hits are `RGBAColour::GetData` and `SlotMap::GetData`; read the type, not
the name.

**Its governing doctrine is still the right one: under-reporting is
recoverable, crying wolf gets the tool switched off.** Every gap ever found in
it closed with a NARROWER claim about where a name's type is known, never a
broader regex — and the fix for the gap above is a separate grep rather than
teaching it to guess at methods.
