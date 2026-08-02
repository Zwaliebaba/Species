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
`std::set` as the shape demands. Not `LList`, `DArray`, `BTree`, `FastDArray`.

**Iteration.** Range-`for` and `<ranges>` over index loops. Reach for
`std::ranges::` algorithms before writing a loop.

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

## Includes

Each project's `pch.h` pulls in its layer header (`NeuronCore.h` /
`NeuronClient.h` / `NeuronServer.h`), which brings in the standard library and
the Windows headers. **Every `.cpp` starts with `#include "pch.h"`** — the
compiler requires it, and it must be the first line.

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

---

## Modernisation migration

Legacy code is converted **deliberately and on a plan**, not opportunistically
and not all at once. Each conversion is its own task in a DAG under `tasks/`
(see [`docs/TASK_DAG.md`](docs/TASK_DAG.md)) and its own commit.

### Order

Later stages assume earlier ones have landed for the code they touch. Do not skip
ahead within a file.

| # | Stage | What changes | Why here |
|---|---|---|---|
| 1 | **Containers down** | `LList`, `DArray`, `BTree`, `FastDArray` move from `NeuronClient` into `NeuronCore` | Nothing else can be layered correctly while the foundation reaches upward for its own containers |
| 2 | **Maths down** | `Vector3`, `Matrix33/34`, `MathUtils` move into `NeuronCore` | The wire protocol serialises these; a headless server needs them without a renderer |
| 3 | **Containers replaced** | `LList`/`DArray` → `std::vector`, `std::list`, `std::map` | Once they are in one place, replacing them is mechanical and reviewable |
| 4 | **Strings** | `char*`, `char[N]`, `strcpy`, `sprintf` → `std::string`, `string_view`, `std::format` | The largest source of latent buffer bugs |
| 5 | **Ownership** | raw `new`/`delete`, `SAFE_DELETE`, `EmptyAndDelete` → `unique_ptr` and values | Depends on containers being standard, since ownership currently lives in `LList` |
| 6 | **Protocol types** | `Entity`, `Team`, `WorldObject` out of `NeuronCore`'s headers | Severs the last game dependency from the network layer |
| 7 | **Globals** | `g_app` reached from inside `NeuronCore` → injected dependencies | The final blocker on a standalone server binary |
| 8 | **Dead targets** | `TARGET_FULLGAME`, `TARGET_DEMOGAME`, `DARWINIA_*`, the unbuilt Linux/macOS branches in `NeuronCore.h` | Safe to do any time; deliberately last so it does not churn files mid-migration |

Stages 1, 2 and 8 are tree-wide moves. Stages 3–7 proceed file by file, and a
file at stage *n* is fully at stage *n* before it advances.

### Converting a file

1. Read it first. Darwinia code has non-obvious invariants, especially around
   `LList` ownership semantics and the fixed-size buffers in the netcode.
2. Convert the whole file. Half-converted is not a state a file may rest in.
3. `python3 tools/check_format.py --all <file>` — whole-file formatting is
   correct here, and only here.
4. Build both configurations.
5. Nothing but the conversion in the commit. No bug fixes, no behaviour changes,
   no renames beyond what the conversion requires.

If you find a bug while converting, note it in the task's `notes` and fix it
separately. A conversion commit that also changes behaviour cannot be reviewed
and cannot be reverted cleanly.
