# Coding standards

How to write code that belongs in this repository.

Two styles coexist in the tree. The **Neuron style** described here is the target
for all new code and everything being migrated. The **legacy style** is inherited
Darwinia code, which is being converted on a plan — not left alone, and not
rewritten wholesale.

`.clang-format` encodes everything a formatter can express. This document covers
what it cannot: naming, ownership, what to use instead of what, and how the
migration proceeds.

---

## Deciding which style applies

| The file you are editing | What to write |
|---|---|
| New file | Neuron style, no exceptions. |
| Anything under `Tests/` | Neuron style. Test code is new code. |
| Already converted (`NeuronCore/FileSys.*`, `Debug.h`, `NeuronHelper.h`, `Server.*`) | Neuron style. |
| Legacy, and you are fixing a bug | Match the file. Fix the bug and nothing else. |
| Legacy, and converting it *is* the task | Neuron style, whole file, own commit. |

The rule that matters: **never leave a file half-converted as a side effect of
something else.** A file is either legacy or Neuron. A file that is both is worse
than either.

---

## Naming

| Kind | Convention | Example |
|---|---|---|
| Type | `PascalCase` | `ServerToClientLetter` |
| Function, method | `PascalCase` | `GetHomeDirectory()` |
| Member variable | `m_camelCase` | `m_sequenceId` |
| Static member | `sm_camelCase` | `sm_homeDir` |
| Global | `g_camelCase` | `g_app`, `g_gameTime` |
| Parameter | `_camelCase` | `_fileName`, `_desiredTeamId` |
| Local | `camelCase` | `clientId` |
| Constant, enumerator | `PascalCase` | `HelloClient`, `TeamAssign` |
| Macro | `SCREAMING_SNAKE` | `DEBUG_ASSERT`, `SAFE_DELETE` |
| Namespace | `PascalCase` | `Neuron` |
| File | `PascalCase.cpp` / `.h` | `NetSocketListener.cpp` |

The leading underscore on parameters is deliberate and consistent across the
converted code. It is legal C++ — the reserved forms are `_Uppercase` and
`__anything`, and identifiers starting `_lowercase` at global scope. Do not use
either of those.

Engine code lives in `namespace Neuron`. Game code does not — `Species`,
`GameLogic` and most of `NeuronClient` are still at global scope, and moving them
is a migration task, not something to do opportunistically.

### Renaming away from Darwinia

**The UI scaffolding rename is done.** `DarwiniaWindow` → `SpeciesWindow` (213),
`DarwiniaButton` → `SpeciesButton` (178), `DarwiniaModeButton` (6), the
`DARWINIA_*` macros (30), and the `About*` pair that sat on top of them — landed
via `tasks/Archive/rename-scaffolding.yaml`, CI-verified.

**The entity rename is done too.** `Darwinian` → `Citizen` landed via
`tasks/Archive/rename-darwinian.yaml` T3: the class and its file, `TypeCitizen` and every
identifier derived from it, 37 `GameData/` filenames, the level-file entity
strings, the `Sounds.txt` groups and the entity-derived language keys, all in one
commit. What remains named after the old game is **branding only**:

| Group | Occurrences | Why it is frozen |
|---|---|---|
| Game-name **strings**, not identifiers — the `"DARWINIA"` title text, the `"Darwinia"` Win32 window class, the store and website URLs | 24 in code | Not a refactor. It is a branding decision and the owner's to make. |
| The four branding language keys — `dialog_leavedarwinia`, `dialog_buydarwinia`, `about_darwinia`, `darwinia_vistaedition` | 5 in English.txt | Same decision; they name the game, not an entity. |
| Localised prose in the five non-English language files — `Darwinianer`, `Darwiniani`, `darwinianos` and the rest | 523 lines | Deliberately deferred by the owner. The KEYS were renamed so lookups resolve; machine-translating prose nobody has reviewed would trade an invisible staleness for a visible text bug. |

> Counting note: `grep -rl Darwinia GameData/` matched files containing
> *Darwinian* and inflated these figures while the entity was still called that.
> That particular trap is gone, but use `grep -rlw` for whole-word matching when
> re-deriving any of these.

**Code-only is necessary but not sufficient**, and the rename that just landed is
the worked example. `TypeDarwinian` appeared nowhere in `GameData/`, so the
filename test passed — yet `Entity::GetTypeId` matches level-file strings against
a `typeNames[]` table whose entry was the literal `"Darwinian"`:

```
MissionGardenLiberate.txt:   Darwinian   0   598.8   1202.4   30 …
Entity.cpp typeNames[]:      "Darwinian"
```

Renaming the enum without the string would have left a `TypeCitizen` that loads
`"Darwinian"` — technically working, conceptually half-converted. Anything
derived from an entity name moves with that entity, in one task, or not at all.
That is why T3 was a single commit across code, assets and content rather than a
sequence of tidy ones.

So the test is two questions, both of which must pass:

```bash
grep -rl "<TheName>" GameData/          # 1. is the name itself in content?
                                        # 2. is it derived from a name that is?
```

**Declare the rename in the commit.** A rename touches every line mentioning the
old name without authoring any of them, so the changed-lines format check would
otherwise demand you reformat fragments of files you did not really edit — the
half-converted state forbidden above. Add a trailer to the commit performing it:

```
Rename: DarwiniaWindow=SpeciesWindow
```

`tools/check_format.py` reads those trailers and skips lines that reverse to a
line already present in the base revision. Lines you genuinely wrote are still
checked, so this cannot be used to sneak unformatted code past CI. If the file
itself was renamed, `git mv` it so git records the rename and the check can
follow it.

The layering allowlist used to have the same problem — it was keyed on filename
and a rename orphaned its entries. It no longer exists: `check_layering.py` is
strict, so there is nothing left for a rename to disturb.

**Domain names — leave them alone.** `Citizen` and anything else naming an
entity, building or program. The name changed; the rule did not, and these names
are still load-bearing in content:

```
GameData/Shapes/Citizen.shp          GameData/Sprites/Citizen.bmp
GameData/Icons/IconCitizen.bmp       GameData/Sounds/LaserHitCitizen1..19.wav
GameData/Sounds/CitizenThreat*.wav   GameData/Levels/MissionGardenLiberate.txt
```

Level files name entity types as **strings** (`Citizen 0 598.8 1202.4 30 …`), and
shapes, sprites and sounds are resolved by filename at runtime. A rename that
misses one fails silently at load rather than at compile time.

`Darwinian` → `Citizen` was done exactly that way: one commit, gated on the game
running so the Garden smoke test could catch a missed reference, and verified by
the owner running it. **Renaming `Citizen`, or anything else whose name appears
in `GameData/`, needs the same treatment** — a plan, one commit, and an
owner-run smoke test. It is not something to do opportunistically.
Check before you assume:

```bash
grep -rl "<TheName>" GameData/     # any output means it is content-coupled
```

---

## Layout

Set by `.clang-format`; run it rather than counting spaces. The shape it produces:

```cpp
namespace Neuron
{
  class BinaryFile : public FileSys
  {
    public:
      [[nodiscard]] static byte_buffer_t ReadFile(std::wstring_view _fileName);

    protected:
      inline static std::wstring sm_homeDir;
  };
}
```

- Two-space indent, spaces never tabs, 150-column limit.
- Allman braces: opening brace on its own line.
- Access specifiers indented one level inside the class; members one level inside
  those.
- Namespace contents indented.
- `Type* name`, not `Type *name`.
- One-line bodies stay on one line; control flow never does.
- Single-statement `if`/`for` bodies do not get braces — but the statement goes on
  its own line.

```cpp
if (strcmp(thisIP, _ip) == 0)
  return i;
```

---

## Language

C++20, MSVC v145, `/permissive-` (`ConformanceMode`). Use the standard library.

**Ownership.** Prefer values. Where a pointer is needed, it is `std::unique_ptr`
or `std::shared_ptr`; a raw pointer is a non-owning observer and never gets
`delete`d. The `SAFE_DELETE` / `SAFE_FREE` macros in `NeuronCore.h` are legacy —
do not use them in new code.

**Strings.** `std::string` / `std::wstring` to own, `std::string_view` /
`std::wstring_view` to borrow. Not `char*`. Not fixed `char[N]` buffers.

Which of the two, for a parameter or a return, is decided by ONE question:
does anything downstream need a null terminator? `string_view` does not carry
one — `.data()` is not guaranteed terminated — and this tree hands strings to
`fopen`, `strcmp`, `strchr`, `_findfirst` and the OpenGL and resource lookups
constantly.

| The function… | Takes / returns |
|---|---|
| reads the characters — compares, copies, parses | `std::string_view` |
| forwards them to a C API that needs a terminator | `char const*` or `std::string const&` |
| returns something that must outlive the call | `std::string`, by value |
| returns a borrow of storage it owns | `std::string_view`, once that lifetime is provable |

Two mistakes this prevents, both of which happened here. Taking `string_view`
and then building a `std::string` inside the function to get a terminator is
worse than taking `char const*` — it hides an allocation the caller cannot see.
And RETURNING a `string_view` to a shared buffer preserves the dangling-pointer
bug it looks like it fixes: `ConvertIntToIP` handed back a pointer into a
function-local `static char[16]`, and only returning an owning `std::string`
made a second call safe.

A `char const*` parameter that only ever gets compared is a `string_view` that
has not been converted yet, not a considered choice.

**Formatting.** `std::format` and `std::vformat`. Not `sprintf`, `snprintf` or
`printf`-family varargs.

**Containers.** `std::vector` by default; `std::unordered_map` / `std::map` /
`std::set` as the shape demands. Not `LList`, `BTree`, `FastDArray`.

`DArray` is the exception and is **not** a `std::vector` in disguise — see
[Determinism](#determinism) before replacing one.

**Iteration.** Range-`for` and `<ranges>` over index loops. Reach for
`std::ranges::` algorithms before writing a loop — but never a parallel execution
policy, and never an unordered traversal, in simulation code.

**Const and constexpr.** `const` by default on anything not mutated. `constexpr`
for anything computable at compile time. `[[nodiscard]]` on any function whose
return value is the point of calling it.

**Casts.** `static_cast` and friends. Never a C-style cast in new code.

**`nullptr`,** never `NULL` or `0`.

**`#pragma once`,** never include guards.

**Errors.** Throw a type derived from `Neuron::BaseException`. `ASSERT` /
`ASSERT_TEXT` for invariants that hold in every build; `DEBUG_ASSERT` for those
only worth checking in Debug. `Neuron::Fatal` for unrecoverable states.

**Enums.** `enum class`, with `ENUM_HELPER` from `NeuronHelper.h` when you need
iteration or bitwise operators over it.

Eight enums are scoped today; four are not, and they are not left for lack of
effort. `ControlType`, `InputType` and `InputCondition` each have an `int`
typedef used interchangeably with them across the driver API, and `Camera::Mode`
is stored in an `int m_mode` — scoping any of them is an API change, tracked as
`tasks/language-hygiene.yaml` T9.

**Before you scope an enum, survey for integer interop by SHAPE, not by variable
name.** Four red builds came from doing the latter. All four forms, with the
grep that finds each:

| Shape | Example | Find it with |
|---|---|---|
| Used as a subscript | `errors[state]` | `grep -nE '\[\s*\w*state\w*\s*\]'` |
| Boolean context | `if (_key && _mood)` | read every use; a 0-valued enumerator makes this mean "not NONE" |
| An `int` variable holding it | `static int old = Enum::Value;` | `grep -rnE '\b(int\|bool\|unsigned\|short\|char)\s+\w*\s*=\s*EnumName::'` |
| Reached through the enclosing class | `Owner::Enumerator` | `grep -rnE '\w+::(Enumerator1\|Enumerator2)' \| grep -v 'EnumName::'` |

The last one is the easiest to miss: a qualification pass keyed on "enumerator
not preceded by `::`" skips those by construction.

---

## Determinism

**The simulation must produce bit-identical results on every client.** This is
not an aspiration — it is checked at runtime, and violating it is the easiest way
to break this game while leaving every build green.

Multiplayer is deterministic lockstep. The server sequences player *intent*;
every client then advances its own copy of the world and is expected to arrive at
the same state. `Species/Main.cpp` `GenerateSyncValue()` sums every unit's,
entity's, laser's and effect's `m_pos` and `m_vel`, folds the result to one byte,
and sends it up each frame. `NeuronCore/Server.cpp` compares it against the other
clients:

```cpp
DEBUG_ASSERT(lastKnownSync == sync);   // Server.cpp — a desync lands here
```

That assert is the whole safety net at runtime. It fires far from whatever caused
it, in a Debug build, possibly minutes later. Read this section before touching
anything the simulation advances.

The other half of the net is the test suite, and it is the half you can actually
use while working. A test that **pins the values** — the `speciesRandom()`
sequence from a known seed, the exact bytes a `ByteStream` macro writes — fails
in the commit that broke it, named, in under a millisecond. Determinism is the
one property in this codebase where pinning a constant is the right thing to do
rather than over-specification: the constant *is* the contract between two
machines. See [`docs/TESTING.md`](docs/TESTING.md).

### What the sync value actually depends on

`GenerateSyncValue()` accumulates floats **in container index order**. So the
result is sensitive to all of:

- the **order** entities are visited in,
- the **number** of entities and the slots they occupy,
- the **exact floating-point value** of every position and velocity,
- and therefore every arithmetic operation that produced them.

### `DArray` is a slot map, not a vector

Its own header states the contract: *"an entry's index never changes"*. It keeps
a `shadow` array marking which slots are live, so `PutData` reuses free slots and
removal leaves a hole rather than shifting anything down.

**Those indices are network identity.** `WorldObjectId` (`GameLogic/WorldObject.h`)
stores `m_unitId` and `m_index` — a raw `DArray` slot — and the whole struct goes
onto the wire through `WRITE_WORLDOBJECTID`. `NetworkUpdate` carries
`m_unitId`, `m_entityId` and `m_buildingId` as bare indices too.

Replacing a `DArray` with `std::vector` therefore breaks two things at once:
erase shifts every later index, silently repointing object references *and*
changing the sync traversal order. If you need to replace one, write or adopt a
**slot map** with stable indices and an occupancy mask — matching `DArray`'s
semantics, not `std::vector`'s.

`std::unordered_map`, `std::unordered_set` and anything else with unspecified
iteration order **must not hold simulation state**. Order can differ between
builds of the same source. They are fine for asset caches, editor state and UI.

Three transformations make hashed containers legitimate in simulation code,
because what desyncs is *observing the order*, not the container existing:

1. **Lookup-only.** A table that is never traversed is deterministic —
   `find`, `insert` and `erase` results do not depend on bucket order. Use
   `Neuron::LookupTable` (`NeuronCore/LookupTable.h`), a wrapper that
   exposes no iteration, so "never traversed" is enforced by the compiler
   rather than by review.
2. **Insertion-ordered iteration.** Pair the hash table with a vector of
   keys in insertion order and traverse the vector, looking up the table.
   Insertions happen in sequenced order, so every client iterates
   identically. Erase must preserve the vector's order — tombstone or
   linear remove; swap-with-last reorders the traversal and desyncs.
3. **Sort before iterating.** For infrequent traversals, snapshot the keys,
   sort them, iterate the snapshot. Never on a hot path.

What stays forbidden is relying on two machines' hash tables happening to
iterate alike: the order is unspecified by contract, so a toolset update can
desync clients silently, and cross-architecture behaviour is already
unproven. At simulation sizes — hundreds of entities — a sorted
`std::vector` or `std::map` often wins outright anyway; reach for these
patterns when profiling says so, not by default.

### Rules for simulation code

Simulation code is `GameLogic/`, plus the world, entity, team and physics code in
`Species/` — anything reachable from `Location::Advance`.

- **Do not change iteration order.** Not by switching container, not by
  reversing a loop, not by "tidying" a traversal.
- **Do not change the order or grouping of floating-point arithmetic.**
  `(a + b) + c` and `a + (b + c)` are different numbers. Refactoring a sum into
  an accumulator, or hoisting a term out of a loop, changes results.
- **Do not use `std::execution` parallel policies.** Non-deterministic reduction
  order.
- **Do not introduce a new random source.** There is exactly one:
  `speciesRandom()` in `NeuronCore/Random.cpp`, a linear congruential
  generator over a single global `holdrand`. It is deterministic only because
  every client makes the *same sequence of calls*. Adding a call, removing one,
  or making one conditional shifts the stream for everything downstream.
  `rand()`, `std::mt19937` and `std::random_device` are all forbidden here.
- **Never call `speciesRandom()` from rendering, UI, sound or the editor.**
  Those run at frame rate rather than tick rate, so they would consume the shared
  stream at a client-dependent rate. Use a separate generator for anything
  cosmetic.
- **Be careful with `sinf`, `cosf`, `powf`, `acosf`, `asinf`, `expf`.** The
  simulation calls them heavily — over 270 sites in `GameLogic/` and `Species/`.
  IEEE-754 pins down `+ - * /` and `sqrt`; it does **not** pin down the
  transcendentals, whose results can differ between CRT versions and between
  architectures. Do not swap one for another form (`powf(x, 0.5f)` for `sqrtf(x)`,
  a lookup table for a call, or vice versa) inside the simulation.

### Native math

`tasks/Archive/directxmath-migration.yaml` replaced the inherited `Vector3`,
`Matrix34` and friends with `DirectX::XMFLOAT3`, `XMFLOAT4X4` and `XMVECTOR`,
and then deleted them. There is no legacy math type to fall back to and no
conversion seam; these rules are the whole of how math is written here. The first four are properties of the native types; the last three were
learned by converting GameLogic and cost a red CI round each. The fourth cost
something worse — it reached a player, twice over, from one uninitialised field:

- **The `*Est` family is banned in simulation code.** `XMVector3NormalizeEst`,
  `XMVectorReciprocalEst` and the rest are explicitly permitted to differ
  between implementations, which makes them a desync between two clients on the
  *same* build. Rendering-only code may use them.
- **Multiplication is row-vector, `v * M`,** and the coordinate system is
  right-handed — use DirectXMath's `*RH` variants, never the `LH` ones. Both are
  stated at length in `NeuronCore/NeuronMath.h`, which is the file to read
  before converting a call site.
- **`XMFLOAT3` does not zero itself.** `Vector3`'s default constructor did.
  Anything that accumulates into a converted member, or reads it before its
  first write, changed behaviour silently — give every converted member an
  explicit initialiser. `tools/check_math_types.py` reports the ones that lack
  one. This and the rule below it are the failure modes on its list that neither
  the compiler nor CI can see, which is why the checker exists.
- **`XMFLOAT4X4` is WIDER than `Matrix34` was, and the extra column is real.**
  `Matrix34` held twelve floats and its `ToNative()` supplied `(0,0,0,1)` for
  the fourth column it did not have. Nothing supplies it now. A writer that
  fills the old twelve — the shape-file marker parser was one — leaves `_14`,
  `_24`, `_34` and `_44` holding stack garbage, and `XMMatrixMultiply` reads all
  sixteen, so a junk `_44` multiplies the parent translation into the result.
  That shipped as markers at roughly -1e12. **When a converted type is wider
  than the one it replaced, audit every writer, not just every reader.**
- **The zero-length fallbacks are gone, and each site decides.**
  `Vector3::Normalise` answered a zero-length input with `(0,0,1)` and
  `SetLength` with `(len,0,0)`; `XMVector3Normalize` answers with zero or QNaN.
  The default is **not** to reproduce them — that was decided in T1 — but where
  the zero case is genuinely reachable and the QNaN would stick, reproduce it
  and say why at the site. Steering code that crosses `front` with a direction
  to a target is the recurring example: facing exactly away makes the cross
  zero, and the old fallback is what broke the deadlock. A one-line comment at
  the site is the standard here, because the next reader cannot tell a
  considered choice from an oversight.
- **A virtual signature cannot move without every overrider in the same
  commit.** An override must match its base exactly; no implicit conversion is
  consulted, and a mismatch silently stops overriding rather than failing. Map a
  class's virtuals in both directions before changing it. This outlived the
  migration that learned it — it is a fact about C++, not about the seam.
- **A parameter takes `XMFLOAT3 const&` when it is STORAGE and `FXMVECTOR` when
  it is a link in a COMPUTATION** — and an `FXMVECTOR` parameter obliges the
  function to be `XM_CALLCONV`. `FXMVECTOR` without `XM_CALLCONV` asks for a
  register type and then passes it by the default convention, which costs the
  alignment constraint and buys none of the speed. Most of the tree's APIs take
  a value a caller holds in memory — an entity's `m_pos`, a marker's world
  position — and those stay `XMFLOAT3 const&`. Converting one to `FXMVECTOR` as
  a tidy-up moves the `XMLoadFloat3` from the callee to every call site, and
  the `*Access` interfaces are virtual, so the choice is not local to one
  function. `NeuronMath.h` has the long version.
- **`g_upVector` and `g_zeroVector` are native constants** in `NeuronMath.h`,
  `(0,1,0)` and `(0,0,0)` as `XMFLOAT3` storage. They are for the sites that
  pass one straight into a parameter. Where an `XMVECTOR` is wanted, reach for
  `DirectX::g_XMIdentityR1` and `DirectX::XMVectorZero()` instead of loading a
  constant from memory.

The two rules that used to sit here about the conversion SEAM — that it crossed
values and references but never pointers, and that `AsLegacy` was the escape
hatch through an out-pointer — are gone with the seam itself. If you are reading
an old commit message that mentions them, `Vector3` and `AsLegacy` were deleted
by `tasks/Archive/directxmath-migration.yaml` T25.

**The migration deliberately changes what the simulation computes** — lane
arithmetic does not reproduce the current scalar arithmetic bit for bit, so a
build carrying part of it desyncs against one that does not. That is sanctioned
on the same terms as `determinism.yaml` T1. What it does **not** change, and
what is still forbidden to change, is the RNG call sequence, iteration order,
container identity and the wire format.

> **Mixed-architecture play is not supported.** DirectXMath dispatches to SSE
> on x64 and to ARM-NEON on ARM64, the two do not produce bit-identical
> results, and deterministic lockstep requires that they do. The owner accepted
> that on 2026-08-03 rather than force the scalar path — no
> `_XM_NO_INTRINSICS_`, no `<FloatingPointModel>`. An ARM64 client and an x64
> client in one session will desync. **Within one architecture the simulation
> stays deterministic**, which is what the sync assert tests and what this
> section is about.

### If you must change simulation behaviour

Sometimes you have to — a bug fix changes results by definition. That is fine,
provided **every client changes the same way**, which means the change ships to
everyone at once. What is not fine is a change whose result depends on the
compiler, the platform, the container implementation, or the frame rate.

---

## Includes

Each project's `pch.h` pulls in its layer header (`NeuronCore.h` /
`NeuronClient.h` / `NeuronServer.h`), which brings in the standard library and
the Windows headers. **Every `.cpp` includes `pch.h` first** — the compiler
requires it to precede any code. A leading comment block is fine; another
`#include` is not.

After that, group with a blank line between groups and order alphabetically
*within* each group:

```cpp
#include "pch.h"

#include "NetLib.h"           // this project
#include "NetSocket.h"

#include "Preferences.h"      // lower layers
#include "Profiler.h"

#include "Server.h"           // this file's own header
```

Includes are written as bare basenames — `#include "App.h"`, not
`#include "../Species/App.h"` — and resolved through each project's
`AdditionalIncludeDirectories`. This is why header basenames must be unique
across the whole tree.

`.clang-format` does **not** sort includes. Some legacy files have load-bearing
include order, and a formatter reordering them silently would be worse than the
inconsistency. Order them by hand.

**Include only downward.** See the layering table in
[`AGENTS.md`](AGENTS.md#layering). `tools/check_layering.py` enforces it.

---

## Adding and removing files

MSBuild does not glob. A `.cpp` on disk that is not in the `.vcxproj` is not
compiled, and nothing tells you — the symbols simply go missing at link time.

Every added or removed source file needs **four** edits:

1. `<ClCompile Include="Foo.cpp" />` in `<Project>.vcxproj`
2. `<ClInclude Include="Foo.h" />` in `<Project>.vcxproj`
3. The matching entries in `<Project>.vcxproj.filters`, each with a `<Filter>`
4. `python3 tools/check_project_files.py` to confirm

`.inc` files count as `ClInclude`. They are not compiled, but they need to be
listed so they appear in the IDE and so the check stays meaningful.

The same four apply to files under `Tests/`, where the consequence is worse: a
test file missing from its `.vcxproj` leaves the suite reporting green for a test
nobody is running. The check covers every `Tests/<Name>Tests` directory.

---

## Comments

Comment the decision, not the mechanism. The code says what it does; a comment
earns its place by saying why it is that way and what would break otherwise.

The converted code sets the bar — this, from `GameData.targets`:

```
Globbed inside the target rather than at project scope: MSBuild expands
project-level wildcards once at evaluation time, so files added to GameData
after the project was loaded would be missed until a reload.
```

That tells the next reader why the obvious simplification is wrong. A comment
saying `// copy the files` would not.

Delete commented-out code. It is in git. The tree still carries large commented
blocks from Darwinia; removing them is fair game when you are converting a file.

**Never delete a copyright or licence notice.** One file carries third-party
terms — `NeuronCore/MathUtils.cpp`, whose BSD 3-clause grant requires its notice
be retained. The notice travels with the code: if you move that file to another
project, the header goes with it, and if you reformat it, the header survives
verbatim. See [`LICENSE`](LICENSE).

`AutoVector.h` and `TriTri.cpp` were two more until containers-replaced/T16 and
directxmath-migration/T4 deleted them. Note the distinction that made both
legal: deleting a whole file — notice included — is not shipping someone's code
without their terms. Stripping the header while keeping the code would have
been.

---

## Modernisation migration

Legacy code is converted **deliberately and on a plan**, not opportunistically
and not all at once. Each conversion is its own task in a DAG under `tasks/`
(see [`docs/TASK_DAG.md`](docs/TASK_DAG.md)) and its own commit.

### Order

Later stages assume earlier ones have landed for the code they touch. Do not skip
ahead within a file.

| # | Stage | Status | What changes | Why here |
|---|---|---|---|---|
| 1 | **Containers down** | done | `LList`, `DArray`, `BTree`, `FastDArray` move from `NeuronClient` into `NeuronCore` | Nothing else can be layered correctly while the foundation reaches upward for its own containers |
| 2 | **Maths down** | done | `Vector3`, `Matrix33/34`, `MathUtils` move into `NeuronCore` | The wire protocol serialises these; a headless server needs them without a renderer |
| 3 | **Containers replaced** | done | `LList` → `std::vector`. **`DArray` → `Neuron::SlotMap`, never `std::vector`** — its indices are network identity, see [Determinism](#determinism). All ten legacy container headers are deleted | The stage most able to break lockstep silently. `SlotMap` ships in two flavours because the legacy templates assigned different indices after a removal: `FastSlotMap` pops a freelist (was `FastDArray`), plain `SlotMap` scans lowest-first (was `DArray`). Picking the wrong one changes network identity for the same spawn sequence |
| 4 | **Strings** | in progress | `char*`, `char[N]`, `strcpy`, `sprintf` → `std::string`, `string_view`, `std::format` — which of the first two is decided by the table under [Strings](#strings) | The largest source of latent buffer bugs. Seven found so far were live defects rather than risks: three unbounded writes, a null dereference, a double-evaluated shared static, a `delete` on a `strdup` pointer, and two `strrchr(...) + 1` reads of address 1 |
| 5 | **Ownership** | **in progress** | raw `new`/`delete`, `SAFE_DELETE`, `EmptyAndDelete` → `unique_ptr` and values | Containers are standard now, so this is unblocked. The transitional `Neuron::EmptyAndDelete` and `Neuron::CopyInto` in `VectorUtils.h` exist for this stage to remove |
| 6 | **Protocol types** | done | `Entity`, `Team`, `WorldObject` out of `NeuronCore`'s headers | Severs the last game dependency from the network layer |
| 7 | **Globals** | done | `g_app` reached from inside `NeuronCore` → injected dependencies | The final blocker on a standalone server binary |
| 8 | **Dead targets** | done | `TARGET_FULLGAME`, `TARGET_DEMOGAME`, `DARWINIA_*`, the unbuilt Linux/macOS branches in `NeuronCore.h` | Safe to do any time; deliberately last so it does not churn files mid-migration |

Stages 1, 2 and 8 are tree-wide moves. Stages 3–7 proceed file by file, and a
file at stage *n* is fully at stage *n* before it advances.

Status re-measured on 2026-08-05, and re-measure rather than trusting it: stage 3
is finished and its headers are deleted; stage 4 has **112** `strcpy`-family calls
across 42 files, down from 367 at the start of the restart and from 161 before
`strings-modernised` T5; stage 5 has **32**
`SAFE_DELETE`/`SAFE_FREE` occurrences and `EmptyAndDelete` in 13 files. Stages 1, 2, 6 and 7 landed with
`tasks/Archive/neuroncore-layering.yaml`; stage 8 landed with `7ee8c00`. The only `g_app`
left anywhere under `NeuronCore/` is a comment explaining what replaced it.

### Converting a file

1. Read it first. Darwinia code has non-obvious invariants, especially around
   `LList` ownership semantics and the fixed-size buffers in the netcode. If the
   domain terms are unfamiliar, [`docs/GLOSSARY.md`](docs/GLOSSARY.md) defines
   them — converting code you cannot read is how invariants get lost.
   If the file is simulation code, re-read [Determinism](#determinism) — the
   conversion must not change iteration order, arithmetic grouping, or the
   sequence of `speciesRandom()` calls.
2. **Characterise it before you touch it.** A conversion commit claims no
   behaviour change; a test written against the legacy code and still passing
   after is the only thing that turns that claim into evidence. Write those
   tests first, in their own commit, so the diff shows them passing on both
   sides. If the behaviour is too entangled to characterise, that is the
   finding — record it in the task's `notes` and say so, rather than converting
   blind. [`docs/TESTING.md`](docs/TESTING.md).
3. Convert the whole file. Half-converted is not a state a file may rest in.
4. `python3 tools/check_format.py --all <file>` — whole-file formatting is
   correct here, and only here.
5. Build Debug, and Release too if the conversion touches anything
   optimisation-sensitive. Run the suite; the characterisation tests from step 2
   are the point of it.
6. Nothing but the conversion in the commit. No bug fixes, no behaviour changes,
   no renames beyond what the conversion requires.

If you find a bug while converting, note it in the task's `notes` and fix it
separately. A conversion commit that also changes behaviour cannot be reviewed
and cannot be reverted cleanly.
