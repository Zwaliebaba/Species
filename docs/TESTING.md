# Testing

What to test, where the test goes, and what "tested" is allowed to mean in this
repository.

Read this before writing code. Tests are part of the change that introduces the
behaviour, not a follow-up task — a plan node that lands code and defers its
tests has not landed.

---

## The suite

One `<Name>Tests` project per library, under `Tests/`, using the **Microsoft
Native Unit Test Framework** (`CppUnitTest.h`) that ships with Visual Studio.
There is no third-party test dependency, which keeps the "clone and build" rule
in [`BUILD.md`](BUILD.md) true.

| Project | Covers | State |
|---|---|---|
| `Tests/NeuronCoreTests` | `NeuronCore` | Real coverage. IP conversion, the `speciesRandom` sequence, the `ByteStream` wire macros. |
| `Tests/NeuronClientTests` | `NeuronClient` | Real coverage of the path helpers in `FilesysUtils`. |
| `Tests/NeuronServerTests` | `NeuronServer` | Wiring smoke test only — the layer is a stub with no behaviour yet. |
| `Tests/GameLogicTests` | `GameLogic` | Real coverage of `WorldObjectId`, plus `LinkStubs.cpp`. See [What cannot be tested yet](#what-cannot-be-tested-yet). |

`Species` and `Server` have no test project. They are executables, and an `.exe`
cannot be linked into a test DLL. Code in either that is worth testing is code
that belongs in a library — move it down and test it there rather than
reaching for a way to link an executable.

Every test DLL is built by the ordinary solution build and run by CI. Nothing is
opt-in.

---

## What to test

**Test the thing that would be wrong if you got it wrong.** Not the accessor
that returns the member you just set.

In this codebase specifically, three categories earn a test every time:

**Anything on the wire.** Packet layout, byte order, `ByteStream` encoding,
`WorldObjectId` identity, sequence numbering. Two machines have to agree, and
when they do not, the symptom appears somewhere else entirely. A round-trip test
is not enough on its own — it passes on either byte order, because the same
machine does both halves. Pin the bytes.

**Anything the simulation depends on being identical everywhere.** The
`speciesRandom` sequence, iteration order over a container that holds simulation
state, arithmetic that feeds `GenerateSyncValue()`. Read
[`CODING_STANDARDS.md`](../CODING_STANDARDS.md#determinism) first. A test that
pins the *values* is the only thing standing between a "harmless" reimplementation
and a desync that every build reports as green.

**Anything you are converting.** A modernisation commit claims no behaviour
change. Characterise the legacy behaviour first, in its own commit, then convert
with the tests already passing on both sides. If the behaviour is too tangled to
characterise, that is the finding — record it in the task's `notes` and say so,
rather than converting blind.

### What not to test

- Getters and setters that do nothing else.
- Rendering output, sound output, or anything needing a GL context or a window.
- Third-party or OS behaviour. `strrchr` works.
- Bugs, unless you are fixing them. Pinning current-but-wrong behaviour makes the
  eventual fix look like a regression. `NeuronClient/FilesysUtils.cpp`
  `GetFilenamePart` and `GetExtensionPart` both add 1 to the result of `strrchr`
  before testing it for null, so a path with no separator is undefined behaviour
  rather than a defined "returns nullptr" — the tests deliberately do not cover
  those inputs, and say so at the point they stop.

---

## Unit tests and integration tests

Both belong in the same `<Name>Tests` project. The distinction that matters here
is not where the file lives, it is what the test is allowed to touch.

**A unit test** exercises one function or class against values. No files, no
sockets, no globals, no clock. It runs in under a millisecond and it fails for
exactly one reason. Most of what is worth testing in this tree is at this level,
because most of the risk is in encoding, identity and arithmetic.

**An integration test** exercises two or more components across a seam that is
itself the thing at risk — a `NetworkUpdate` written by one component and read by
another, a level file parsed into the structures the loader hands on. Name it for
the seam, not for the classes: `NetworkUpdateRoundTripTests`, not
`NetworkUpdateAndServerTests`.

Integration tests may touch the filesystem. If one does, it creates what it needs
under its own temporary directory and removes it afterwards; it never reads
`GameData/` in place and it never writes into the source tree. A test that leaves
state behind makes the next test's failure somebody else's problem.

**Neither may open a socket, a window, or an audio device.** Those are the parts
of the system a CI runner cannot give you and a headless machine will fail on in
a way that looks like your test being wrong.

---

## Writing a test

Files are named for what they cover — `ByteStreamTests.cpp`, not
`NeuronCoreTests.cpp`. One `TEST_CLASS` per unit under test, named
`<Unit>Tests`. Test methods are sentences that state the expected behaviour:

```cpp
#include "pch.h"

#include "Generic.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  TEST_CLASS(GenericTests)
  {
    public:
      TEST_METHOD(ConvertIpToIntPutsTheFirstOctetInTheLowByte)
      {
        Assert::AreEqual(0x04030201, ConvertIPToInt("1.2.3.4"));
      }
  };
}
```

- **Neuron style throughout.** Test code is new code, so
  [`CODING_STANDARDS.md`](../CODING_STANDARDS.md) applies without exception —
  two-space indent, Allman braces, 150 columns. `check_format.py` covers
  `Tests/` like anywhere else.
- **`#include "pch.h"` first**, then the header under test. The test project's
  own `pch.h` wins over the library's, because the compiler searches the
  including file's directory first.
- **Name the method after the behaviour**, not the function.
  `SeedOneProducesTheKnownSequence` tells you what broke from the failure list
  alone; `TestRandom1` does not.
- **One reason to fail per test.** A method with six unrelated asserts reports
  the first one and hides the rest.
- **Say why in a comment when the value is not obvious.** A pinned constant with
  no explanation is a constant the next person will "fix". The comment above
  `RandomTests` says what desyncs if the number changes; that is the part worth
  writing down.

### Adding a file

Same four steps as anywhere else in the tree — see
[`CODING_STANDARDS.md`](../CODING_STANDARDS.md#adding-and-removing-files). MSBuild
does not glob, and a test file missing from its `.vcxproj` is worse than a
library file missing from one: the suite still reports green, and it reports
green for a test nobody is running. `tools/check_project_files.py` covers every
`Tests/<Name>Tests` directory and catches it.

### Adding a test project

If a new library is added, give it a test project the same day:

1. Copy the shape of an existing one. `Tests/NeuronServerTests` is the smallest.
2. Import `$(SpeciesRoot)Tests\Tests.props` from the `PropertySheets` group. It
   carries every setting that must match the library under test — `/std:c++20`,
   `/permissive-`, MultiByte, the `_CRT_*` suppressions. A test binary built with
   different settings links a differently-translated copy of the code it is
   testing.
3. Set `<ProjectSubType>NativeUnitTestProject</ProjectSubType>`. That is what
   puts `CppUnitTest.h` on the include path and marks the DLL as a test
   container. The `$(VCInstallDir)UnitTest\include` path the Visual Studio
   template writes does not exist in VS 2026 — do not copy it forward.
4. `AdditionalIncludeDirectories` mirrors the library under test, plus the
   library's own directory.
5. `ProjectReference` every project in the link chain explicitly, GUID included.
   Static libraries do not chain.
6. Add it to `Species.slnx` under the `/Tests/` folder.

`tools/check_layering.py` and `tools/check_project_files.py` both discover
`Tests/<Name>Tests` directories from the tree, so a new project is covered as
soon as it exists — there is no list to remember to update.

---

## Layering applies to tests

`<Name>Tests` sits directly above `<Name>` and inherits its dependencies. **A
test may reach no further up than the code it covers already does.**
`tools/check_layering.py` enforces this exactly as it does for the libraries:

```
Tests/NeuronCoreTests/GenericTests.cpp -> App.h
```

is a new violation and fails CI. This is not pedantry. Without the rule the suite
becomes a second place upward dependencies accumulate — and a worse one, because
a test reaching into `Species` to build a fixture reads as diligence rather than
as the layering violation it is. If a test cannot reach what it needs, that is
the design telling you something about the code, not about the test.

The test projects' include paths mirror the library under test, which means
`Species` is on `NeuronCoreTests`' include path today. That is a consequence of
`NeuronCore` still reaching upward, not permission. The checker is what decides.

---

## What cannot be tested yet

`GameLogic` cannot be linked into a test DLL on its own. Pulling in one object
file drags in whatever globals it happens to touch, and those globals live in the
`Species` executable:

```
GameLogic.lib(WorldObject.obj) : error LNK2001:
    unresolved external symbol "class App * g_app"
```

`Tests/GameLogicTests/LinkStubs.cpp` defines the minimum needed to link, so the
parts of `GameLogic` that do not use those globals at runtime can be tested now
instead of after the layering work lands. Every stub is null, so a test that
actually reaches one crashes in that test rather than running against a fake
world.

**That file may only shrink.** Same rule as `tools/layering_allowlist.txt` and
for the same reason: every entry is a dependency that should not exist, and
adding one hides the problem rather than recording it. If a test needs a new
stub, the honest options are to test something that does not reach upward, or to
do the layering task that removes the reference. When migration stage 7
(globals) lands, the file goes away.

The same wall is why `NeuronCore`'s `ClientToServer` has no tests: its header
includes `Entity.h` and `WorldObject.h` from `GameLogic`, two of the thirty
upward includes `tasks/neuroncore-layering.yaml` exists to remove. Coverage of
the client-side netcode arrives with that plan, not before it.

---

## Running them

Locally, from a Developer Command Prompt or anywhere `msbuild` and
`vstest.console` are on `PATH`:

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /m
vstest.console.exe ARM64\Debug\*Tests.dll /Platform:ARM64
```

Swap `ARM64` for `x64` to match what CI runs. The solution build puts every test
DLL in the solution output directory alongside the libraries.

A single project, without building the rest of the solution:

```powershell
msbuild Tests\NeuronCoreTests\NeuronCoreTests.vcxproj /p:Configuration=Debug /p:Platform=ARM64
vstest.console.exe Tests\NeuronCoreTests\ARM64\Debug\NeuronCoreTests.dll /Platform:ARM64
```

Note the different output path: a project built on its own writes beside itself,
not into the solution output directory.

If a test project is ever removed, delete its DLL from the solution output
directory by hand. `Clean` only removes what a project still in the tree
recorded, so an orphaned `*Tests.dll` keeps being discovered and keeps reporting
its old results as if they were current. CI checks out fresh and never sees
this; you will.

In Visual Studio, Test Explorer discovers all four projects with no
configuration.

### In CI

`.github/workflows/ci.yml` builds x64 Debug and then runs every
`x64\Debug\*Tests.dll` through `vstest.console`. The step fails if fewer than
four test DLLs are discovered, because an empty glob would otherwise let the run
go green having tested nothing — the one failure mode a test gate must not have.
A failing run uploads the `.trx` as an artifact.

CI runs the tests on **x64 Debug only**, matching the build. If a test is
sensitive to architecture — and anything pinning floating-point results is —
run it on ARM64 yourself, because nothing else will.

---

## Reporting

The rule from [`AGENTS.md`](../AGENTS.md) holds and gets sharper now that a suite
exists: **say what you actually ran.**

- "The suite passes" means you ran it. If you built and did not run, say that.
- A green suite is not evidence the game works. It covers wire encoding, string
  helpers and identity — a few hundred lines out of 113,000. The Garden smoke
  test in [`AGENTS.md`](../AGENTS.md#what-working-looks-like) is still the thing
  that would tell you the game runs, and it still cannot be run.
- If you changed behaviour and no test failed, that is information. Either the
  behaviour was not covered — in which case add the test — or you did not change
  what you thought you did.
