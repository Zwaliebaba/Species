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
via `tasks/rename-scaffolding.yaml`, CI-verified.

**No Darwinia-named identifier derived from the game remains.** 372 occurrences
across 27 spellings are left, and every one falls into a group that is frozen for
a stated reason:

| Group | Occurrences | Why it is frozen |
|---|---|---|
| Named in `GameData/` — `Darwinian`, `Darwinians`, `dialog_leavedarwinia` and the other language keys | 241 | Level files and language tables resolve these by string at runtime. Renaming breaks content loading, silently. |
| Entity-derived identifiers — `TypeDarwinian`, `FindDarwinian`, `numDarwinians`, `RenderDarwinians` | 101 | Code-only, so the filename test passes — but they name the same concept. See below. |
| Game-name **strings**, not identifiers — the `"DARWINIA"` title text, `"~/.darwinia"` and `"Application Support/Darwinia"` user-data paths, the `"Darwinia"` Win32 window class | 30 | Not a refactor. The title text is a branding decision, and changing the data paths orphans every existing save. |

> Counting note: `grep -rl Darwinia GameData/` matches files containing
> *Darwinian*, which inflates the first group. Use `grep -rlw` for whole-word
> matching when re-deriving these figures.

**Code-only is necessary but not sufficient.** `TypeDarwinian` appears nowhere in
`GameData/`, so the filename test passes — yet `Entity::GetTypeId` matches
level-file strings against a `typeNames[]` table whose entry is the literal
`"Darwinian"`:

```
MissionGardenLiberate.txt:   Darwinian   0   598.8   1202.4   30 …
Entity.cpp typeNames[]:      "Darwinian"
```

Renaming the enum without the string would leave a `TypeCitizen` that loads
`"Darwinian"` — technically working, conceptually half-converted. Anything
derived from an entity name moves with that entity, in one task, or not at all.

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

The same problem hits the layering allowlist, which is keyed on filename — see
`python3 tools/check_layering.py --rename OLD NEW`.

**Domain names — leave them alone.** `Darwinian` (140 occurrences) and anything
else naming an entity, building or program. These names are load-bearing in
content:

```
GameData/Shapes/Darwinian.shp        GameData/Sprites/Darwinian.bmp
GameData/Icons/IconDarwinian.bmp     GameData/Sounds/LaserHitDarwinian1..19.wav
GameData/Sounds/DarwinianThreat*.wav GameData/Levels/MissionGardenLiberate.txt
```

Level files name entity types as **strings** (`Darwinian 0 598.8 1202.4 30 …`),
and shapes, sprites and sounds are resolved by filename at runtime. A rename that
misses one fails silently at load rather than at compile time.

That rename was gated on the game running again, which it now does — so the
Garden smoke test can finally catch a missed reference, and the task is
unblocked rather than done. It remains one deliberate task covering code, assets
and content together, and nobody has written it. Until someone does: **do not
rename `Darwinian`, and do not rename anything whose name appears in
`GameData/`.**
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

> **Cross-architecture play is unproven.** The projects build both ARM64 and x64
> with MSVC defaults — no `<FloatingPointModel>` is set anywhere. Whether an
> ARM64 client and an x64 client stay in sync depends on contraction and libm
> behaviour that nobody here has verified. Assume they do not until someone
> tests it. If mixed-architecture play is ever a goal, pinning the float model
> and auditing the transcendentals becomes a project in its own right.

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

**Never delete a copyright or licence notice.** Three files carry third-party
terms — `NeuronClient/AutoVector.h`, `NeuronClient/MathUtils.cpp` and
`NeuronClient/TriTri.cpp` — and two of them require their notice be retained. The
notice travels with the code: if you move one of these files to another project,
the header goes with it, and if you reformat one, the header survives verbatim.
See [`LICENSE`](LICENSE). This applies to `AutoVector.h` in particular, which
migration stage 1 moves into `NeuronCore`.

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
| 3 | **Containers replaced** | **todo** | `LList` → `std::vector`/`std::list`. **`DArray` → a slot map, never `std::vector`** — its indices are network identity, see [Determinism](#determinism) | Once they are in one place they can be replaced one at a time. This is the stage most able to break lockstep silently; treat every `DArray` as a design question, not a substitution |
| 4 | **Strings** | **todo** | `char*`, `char[N]`, `strcpy`, `sprintf` → `std::string`, `string_view`, `std::format` | The largest source of latent buffer bugs |
| 5 | **Ownership** | **todo** | raw `new`/`delete`, `SAFE_DELETE`, `EmptyAndDelete` → `unique_ptr` and values | Depends on containers being standard, since ownership currently lives in `LList` |
| 6 | **Protocol types** | done | `Entity`, `Team`, `WorldObject` out of `NeuronCore`'s headers | Severs the last game dependency from the network layer |
| 7 | **Globals** | done | `g_app` reached from inside `NeuronCore` → injected dependencies | The final blocker on a standalone server binary |
| 8 | **Dead targets** | done | `TARGET_FULLGAME`, `TARGET_DEMOGAME`, `DARWINIA_*`, the unbuilt Linux/macOS branches in `NeuronCore.h` | Safe to do any time; deliberately last so it does not churn files mid-migration |

Stages 1, 2 and 8 are tree-wide moves. Stages 3–7 proceed file by file, and a
file at stage *n* is fully at stage *n* before it advances.

Status re-measured on 2026-08-02, and re-measure rather than trusting it: stage 3
has `LList` in 127 files, stage 4 has 324 `strcpy`/`sprintf` calls, stage 5 has 34
`SAFE_DELETE` uses. Stages 1, 2, 6 and 7 landed with
`tasks/neuroncore-layering.yaml`; stage 8 landed with `7ee8c00`. The only `g_app`
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
