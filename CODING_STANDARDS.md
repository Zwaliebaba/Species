# Coding standards

How to write code that belongs in this repository.

Two styles coexist in the tree. The **Neuron style** described here is the target
for all new code and everything being converted. The **legacy style** is
inherited Darwinia code, which is converted deliberately, on a plan, one file at
a time.

`.clang-format` encodes everything a formatter can express. This document covers
what it cannot: naming, ownership, what to use instead of what, and the
constraints the simulation puts on all of it.

**This document states standards, not status.** What is converted, what is
planned and what is measured live in `tasks/` and [`AGENTS.md`](AGENTS.md); if
you find a count or a task id here, it has leaked and should be removed.

---

## Deciding which style applies

| The file you are editing | What to write |
|---|---|
| New file | Neuron style, no exceptions. |
| Anything under `Tests/` | Neuron style. Test code is new code. |
| Already converted | Neuron style. Match what is there. |
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
| Macro | `SCREAMING_SNAKE` | `DEBUG_ASSERT`, `ENUM_HELPER` |
| Namespace | `PascalCase` | `Neuron` |
| File | `PascalCase.cpp` / `.h` | `NetSocketListener.cpp` |

The leading underscore on parameters is deliberate and consistent across the
converted code. It is legal C++ — the reserved forms are `_Uppercase` and
`__anything`, and identifiers starting `_lowercase` at global scope. Do not use
either of those.

**Engine code lives in `namespace Neuron`; game code lives in
`namespace Species`.** All of `NeuronClient` and `NeuronServer` are in the
first, all of `GameLogic` and the `Species` executable in the second, and
`NeuronCore` is in it only in part — its converted helpers are, its networking
and protocol types are not. `docs/ARCHITECTURE.md#namespaces` has the split and
the three ways a namespace change breaks a build; read it before adding a
forward declaration that crosses a layer.

You do not have to qualify an engine name: `NeuronCore.h` ends with
`using namespace Neuron;` and every pch includes it. **Do not add the
equivalent for `Species`** — its only users are inside it, and a test DLL that
needs one puts it in a `.cpp`, never a header.

### Renaming

**A name that appears in `GameData/` is content, not just code.** Level files
name entity types as strings (`Citizen 0 598.8 1202.4 30 …`), and shapes,
sprites and sounds are resolved by filename at runtime. Renaming the identifier
without the content leaves code that compiles and fails silently at load.

Two questions, both of which must pass before you rename anything:

```bash
grep -rlw "<TheName>" GameData/     # 1. is the name itself in content?
                                    # 2. is anything derived from it in content?
```

The second catches what the first misses. An entity enumerator can appear
nowhere in `GameData/` while the string table that resolves level-file entries
holds the old spelling — so the filename check passes and the game loads a name
that no longer exists. **Anything derived from a name moves with that name, in
one commit, across code, assets and content**, or not at all.

A rename of a content-coupled name needs a plan, a single commit, and an
owner-run smoke test. It is not something to do opportunistically.

**Declare the rename in the commit.** A rename touches every line mentioning the
old name without authoring any of them, so the changed-lines format check would
otherwise demand you reformat fragments of files you did not really edit — the
half-converted state forbidden above. Add a trailer to the commit performing it:

```
Rename: OldName=NewName
```

`tools/check_format.py` reads those trailers and skips lines that reverse to a
line already present in the base revision. Lines you genuinely wrote are still
checked, so this cannot be used to sneak unformatted code past CI. If the file
itself was renamed, `git mv` it so git records the rename and the check can
follow it.

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

C++23, MSVC v145, `/permissive-` (`ConformanceMode`). Use the standard library.

**Ownership.** Prefer values. Where a pointer is needed, it is `std::unique_ptr`
or `std::shared_ptr`; a raw pointer is a non-owning observer and never gets
`delete`d. `SAFE_DELETE`, `SAFE_FREE` and `EmptyAndDelete` no longer EXIST —
migration stage 5 deleted them along with their last callers. `SAFE_DELETE_ARRAY`
survives in `NeuronCore.h` with two callers in `NeuronClient/Shape.cpp` and no
owning task; do not add a third.

**Strings.** `std::string` / `std::wstring` to own, `std::string_view` /
`std::wstring_view` to borrow. Not `char*`. Not fixed `char[N]` buffers.

`NewStr` no longer EXISTS either. It was `strcpy(new char[strlen(src) + 1], src)`
— an owning raw copy with no bound and no owner — and it went with its last
caller. A `std::string` member *is* the copy; do not reintroduce it.

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

Two mistakes this prevents, both of which have happened here. Taking
`string_view` and then building a `std::string` inside the function to get a
terminator is worse than taking `char const*` — it hides an allocation the
caller cannot see. And RETURNING a `string_view` to a shared buffer preserves
the dangling-pointer bug it looks like it fixes: a function handing back a
pointer into its own `static char[16]` is not fixed by narrowing the type, only
by returning owning storage.

A `char const*` parameter that only ever gets compared is a `string_view` that
has not been converted yet, not a considered choice.

**`==` IS NOT A DROP-IN FOR `stricmp`.** `std::string`'s comparison is
case-SENSITIVE, so converting a member to `std::string` and sweeping its
comparisons to `==` silently changes every site that deliberately compared
case-insensitively — and it changes them only for the inputs whose case
differs, which is the worst possible failure shape. Read each comparison before
converting it. Where the old behaviour was `stricmp`, the replacement is
`Neuron::StrEqualsIgnoreCase` (`StringUtils.h`). Seventeen sites in the UI needed
it when the Eclipse widget names became `std::string`; Eclipse's own lookup used
`strcmp` and is exactly unchanged.

**An accessor that can fail returns `char const*`, not `std::string`.** Check
what a lookup's failure value is before converting its return type: a `nullptr`
that callers test turns into an empty string that they do not, and the compiler
says nothing.

**Formatting.** `std::format` and `std::vformat`. Not `sprintf`, `snprintf` or
`printf`-family varargs.

**Never pass runtime data as a format string.** A `char const*, ...` entry point
that formats a caption, a translated phrase or a level-file field through the C
library is undefined behaviour the moment that data contains a `%`. Take
`std::format_string<Args...>` where the format is a literal, and a plain
`std::string_view` overload where it is not.

**Give the formatting template at least one argument, or the two overloads are
ambiguous.** `std::format_string<Args...>` with an empty pack and
`std::string_view` are both reachable from a string literal by one user-defined
conversion, so a zero-argument call cannot choose. Spell the template
`format_string<T, Args...> _fmt, T&& _arg, Args&&... _args` and the plain
overload takes every call that passes only text:

```cpp
void        Draw(float _x, std::string_view _text);
template <typename T, typename... Args>
void        Draw(float _x, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args);
```

**Converting printf specs is not search-and-replace.** `%f` is six decimals and
`{}` is shortest round-trip, so a bare `%f` changes output silently; it must
become `{:f}`. `%10s` right-justifies and `{:10}` LEFT-justifies a string, so a
padded `%s` becomes `{:>10}`. A literal brace has to be doubled. And a literal
with no arguments after it stops being a format string entirely under the split
above — check it for a `%` before you move it.

**Containers.** `std::vector` by default; `std::unordered_map` / `std::map` /
`std::set` as the shape demands.

`Neuron::SlotMap` is the exception and is **not** a `std::vector` in disguise —
see [Determinism](#determinism) before using or replacing one.

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

Some inherited enums are still unscoped, and scoping one is an API change rather
than a tidy-up: where an `int` typedef is used interchangeably with the
enumerators across an API, or the value is stored in an `int` member, the change
reaches every caller.

**Before you scope an enum, survey for integer interop by SHAPE, not by variable
name.** Surveying by name produces builds that fail four different ways. All
four forms, with the grep that finds each:

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
and sends it up each frame. `NeuronServer/Server.cpp` compares it against the
other clients:

```cpp
DEBUG_ASSERT(lastKnownSync == sync);   // Server.cpp — a desync lands here
```

That assert is the whole safety net at runtime. It fires far from whatever caused
it, in a Debug build, possibly minutes later. Read this section before touching
anything the simulation advances.

The other half of the net is the test suite, and it is the half you can actually
use while working. A test that **pins the values** — an RNG sequence from a known
seed, the exact bytes a `ByteStream` macro writes — fails in the commit that
broke it, named, in under a millisecond. Determinism is the one property in this
codebase where pinning a constant is the right thing to do rather than
over-specification: the constant *is* the contract between two machines. See
[`docs/TESTING.md`](docs/TESTING.md).

### What the sync value actually depends on

`GenerateSyncValue()` accumulates floats **in container index order**. So the
result is sensitive to all of:

- the **order** entities are visited in,
- the **number** of entities and the slots they occupy,
- the **exact floating-point value** of every position and velocity,
- and therefore every arithmetic operation that produced them.

### `Neuron::SlotMap` is a slot map, not a vector

Simulation containers are `Neuron::SlotMap`, whose contract is that **an entry's
index never changes**. An occupancy mask marks which slots are live, so
inserting reuses a free slot and removing leaves a hole rather than shifting
anything down.

**Those indices are network identity.** `WorldObjectId`
(`GameLogic/WorldObject.h`) stores `m_unitId` and `m_index` — a raw slot — and
the whole struct goes onto the wire through `WRITE_WORLDOBJECTID`.
`NetworkUpdate` carries `m_unitId`, `m_entityId` and `m_buildingId` as bare
indices too.

Swapping a `SlotMap` for a `std::vector` therefore breaks two things at once:
erase shifts every later index, silently repointing object references *and*
changing the sync traversal order. **Do not.**

**The two flavours are not interchangeable either.** `SlotMap` scans
lowest-first for a free slot; `FastSlotMap` pops a freelist. They assign
*different indices* for the same spawn-and-remove sequence, which means picking
the wrong one changes network identity while everything still compiles and runs.
The flavour is part of the type; match what the container already is.

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
desync clients silently. At simulation sizes — hundreds of entities — a sorted
`std::vector` or `std::map` often wins outright anyway; reach for these patterns
when profiling says so, not by default.

### The two random streams

**There are two generators and picking the wrong one is a desync.**

| | Where | Use it for |
|---|---|---|
| `syncrand()`, `syncfrand()`, `syncsfrand()` | `NeuronCore/MathUtils.cpp` — Mersenne Twister | **Simulation state.** Anything a position, velocity, timer or decision depends on. |
| `speciesRandom()`, `frand()`, `sfrand()` | `NeuronCore/Random.cpp` — LCG over `holdrand` | **Cosmetics only.** Particles, render jitter, muzzle-flash sizes, UI placement. |

`syncrand()` is the synchronised one. It is never explicitly seeded, so every
client starts from the same state; it stays in step only because every client
makes the *same sequence of calls*. Adding a call, removing one, or making one
conditional on anything client-local shifts the stream for everything
downstream.

`speciesRandom()` is **not** synchronised and cannot be made so: terrain
generation, tree generation and the landscape colour lookup reseed it wholesale
through `speciesSeedRandom`, and sound, the Eclipse UI and the frame loop consume
it at a client-dependent rate. Treat its state as arbitrary at any moment.

- **Never draw simulation state from `frand`/`sfrand`/`speciesRandom`.** This is
  the failure that actually happens, and it is easy to write because the two
  families differ by four characters. The shape to watch for is indirect: a
  timer seeded from `frand` that later gates a `syncfrand` draw makes two
  clients draw from the *synchronised* stream on different ticks, and they
  diverge permanently with every build green.
- **Never call `syncrand()` from rendering, UI, sound or the editor.** Those run
  at frame rate rather than tick rate, so they would consume the synchronised
  stream at a client-dependent rate.
- `rand()`, `std::mt19937` and `std::random_device` are forbidden in simulation
  code.

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
- **Be careful with `sinf`, `cosf`, `powf`, `acosf`, `asinf`, `expf`.** The
  simulation calls them heavily. IEEE-754 pins down `+ - * /` and `sqrt`; it does
  **not** pin down the transcendentals, whose results can differ between CRT
  versions and between architectures. Do not swap one for another form
  (`powf(x, 0.5f)` for `sqrtf(x)`, a lookup table for a call, or vice versa)
  inside the simulation.

### Native math

Math storage is DirectXMath's own — `DirectX::XMFLOAT2/3`, `XMFLOAT3X3`,
`XMFLOAT4X4` — with `XMVECTOR` for computation. There is no Neuron vector or
matrix type and no conversion seam; these rules are the whole of how math is
written here. `NeuronCore/NeuronMath.h` holds the conventions.

- **The `*Est` family is banned in simulation code.** `XMVector3NormalizeEst`,
  `XMVectorReciprocalEst` and the rest are explicitly permitted to differ
  between implementations, which makes them a desync between two clients on the
  *same* build. Rendering-only code may use them.
- **Multiplication is row-vector, `v * M`,** and the coordinate system is
  right-handed — use DirectXMath's `*RH` variants, never the `LH` ones. Both are
  stated at length in `NeuronMath.h`, which is the file to read before
  converting a call site.
- **`XMFLOAT3` does not zero itself.** Anything that accumulates into a member,
  or reads it before its first write, is reading stack garbage — give every
  member an explicit initialiser. `tools/check_math_types.py` reports the ones
  that lack one. This and the rule below are failure modes neither the compiler
  nor CI can see, which is why that checker exists.
- **When a type is wider than the one it replaced, audit every writer, not just
  every reader.** `XMFLOAT4X4` holds sixteen floats where the inherited 3x4 type
  held twelve and synthesised `(0,0,0,1)` for the fourth column. Nothing
  synthesises it now, so a writer that fills only twelve leaves `_14`, `_24`,
  `_34` and `_44` as stack garbage — and `XMMatrixMultiply` reads all sixteen,
  so a junk `_44` multiplies the parent translation into the result. That
  shipped as markers at roughly -1e12.
- **`XMVector3Normalize` has no zero-length fallback, and each site decides.**
  It answers a zero-length input with zero or QNaN. The default is **not** to
  invent a fallback — but where the zero case is genuinely reachable and the
  result would stick, reproduce one and say why at the site. Steering code that
  crosses `front` with a direction to a target is the recurring example: facing
  exactly away makes the cross zero. A one-line comment at the site is the
  standard, because the next reader cannot tell a considered choice from an
  oversight.
- **A virtual signature cannot move without every overrider in the same
  commit.** An override must match its base exactly; no implicit conversion is
  consulted, and a mismatch silently stops overriding rather than failing. Map a
  class's virtuals in both directions before changing it.
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

> **Mixed-architecture play is not supported.** DirectXMath dispatches to SSE
> on x64 and to ARM-NEON on ARM64, the two do not produce bit-identical
> results, and deterministic lockstep requires that they do. **Within one
> architecture the simulation stays deterministic**, which is what the sync
> assert tests and what this section is about.

### If you must change simulation behaviour

Sometimes you have to — a bug fix changes results by definition. That is fine,
provided **every client changes the same way**, which means the change ships to
everyone at once, alone, in its own commit. What is not fine is a change whose
result depends on the compiler, the platform, the container implementation, or
the frame rate.

A deliberate behaviour change is not a refactor and must never ride inside one.
Nothing that runs in CI can see a shifted RNG sequence, so such a change is
verified by an owner-run smoke test, not by a green build.

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
[`AGENTS.md`](AGENTS.md#layering). `tools/check_layering.py` enforces it, and it
has no escape hatch: if your change needs an upward include, the design is
wrong. Move the shared declaration down into a layer both sides can see, or
invert the dependency behind an interface.

The same check catches a free function or extern variable that a library header
declares and only an executable defines — the same upward reach with the linker
doing the work instead of the preprocessor. Class members are exempt: a
pure-virtual declared low and overridden high is dependency inversion, which is
the intended way out.

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

**Never delete a copyright or licence notice.** `NeuronCore/MathUtils.cpp`
carries a BSD 3-clause grant whose terms require the notice be retained. A
notice travels with its code: if you move the file, the header goes with it, and
if you reformat it, the header survives verbatim. See [`LICENSE`](LICENSE), which
lists every file that carries third-party terms.

Deleting a whole file — notice included — is not shipping someone's code without
their terms, and is fine. Stripping the header while keeping the code is not.

---

## Converting a legacy file

Conversion happens **deliberately and on a plan**, never opportunistically and
never all at once. Each conversion is its own task in a DAG under `tasks/` (see
[`docs/TASK_DAG.md`](docs/TASK_DAG.md)) and its own commit.

Conversions are staged, and a file at stage *n* is fully at stage *n* before it
advances: containers, then strings, then ownership. Later stages assume earlier
ones have landed **for the file they touch** — do not skip ahead within a file.

**There is no per-file stage to look up any more.** The staged sweeps are done
tree-wide and their plans are archived, so nothing here tells you which stage a
file is at — that question no longer has an answer, and no open plan owns a
file for staged conversion. Plans opened since do own files, for their own
subjects; a plan's `files` list is the authority on which, and it is what stops
two agents editing the same file. This section governs a file you are converting
for some other reason, and a conversion still needs a plan written before code
is written.
[`AGENTS.md`](AGENTS.md) carries the status; `docs/TASK_DAG.md` the standard.

1. Read it first. Darwinia code has non-obvious invariants, especially around
   ownership semantics in the inherited containers and the fixed-size buffers in
   the netcode. If the domain terms are unfamiliar,
   [`docs/GLOSSARY.md`](docs/GLOSSARY.md) defines them — converting code you
   cannot read is how invariants get lost. If the file is simulation code,
   re-read [Determinism](#determinism): the conversion must not change iteration
   order, arithmetic grouping, or the RNG call sequence.
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
