# Building Species

## Requirements

| | |
|---|---|
| OS | Windows. The code uses Win32, Winsock, WGL, XAudio2 and DirectSound directly. |
| Toolchain | Visual Studio 2026, MSVC toolset **v145**, Windows SDK 10 |
| Language | C++23 (`/permissive-`). **Debug sets `stdcpplatest`, Release still sets `stdcpp20`** — see *Release* below.  |
| Platforms | **ARM64** (primary) and **x64** |
| Dependencies | **DirectXMath**, header-only, from the Windows SDK. Links only against `opengl32`, `glu32`, `winmm`, `dsound`, `dxguid`, `xaudio2`, `Ws2_32`. `xaudio2.lib` arrived with `sound-xaudio2` T1 and covers XAudio2, X3DAudio and XAPOFX between them — no separate `x3daudio.lib` or `xapofx.lib`, and no DirectX SDK. `dsound`/`dxguid` go when that plan deletes the DirectSound backend. |

There is nothing to install, restore or vendor. Clone and build.

The one dependency is DirectXMath, and it is not a package: `<DirectXMath.h>`
and `<DirectXCollision.h>` ship in the Windows SDK 10 that the toolchain row
above already requires, so there is nothing to fetch and no library to link.
Every math type in the tree is one of its — `XMFLOAT2/3`, `XMFLOAT3X3`,
`XMFLOAT4X4`, `XMVECTOR`, `XMMATRIX` — as of
`tasks/Archive/directxmath-migration.yaml`. An SDK old enough to lack
`DirectXCollision.h`'s `TriangleTests` will not compile `NeuronCore`.

The solution is `Species.slnx` — the XML solution format, which requires
MSBuild 17.14 or newer. Visual Studio 2026 supplies it.

---

## Building

From a Developer Command Prompt, or anywhere `msbuild` is on `PATH`:

```powershell
msbuild Species.slnx /p:Configuration=Debug   /p:Platform=ARM64 /m
msbuild Species.slnx /p:Configuration=Release /p:Platform=ARM64 /m
msbuild Species.slnx /p:Configuration=Debug   /p:Platform=x64   /m
msbuild Species.slnx /p:Configuration=Release /p:Platform=x64   /m
```

Or open `Species.slnx` in Visual Studio and build normally.

That includes the four test projects under `Tests/` — they are part of the
solution, not an opt-in target. To run what it just built:

```powershell
vstest.console.exe ARM64\Debug\*Tests.dll /Platform:ARM64
```

See [`TESTING.md`](TESTING.md).

A single project builds on its own, pulling its dependencies through project
references:

```powershell
msbuild Species\Species.vcxproj /p:Configuration=Debug /p:Platform=ARM64
```

This works because include paths are anchored on `$(SpeciesRoot)` — defined in
`Directory.Build.props` — rather than `$(SolutionDir)`, which is only meaningful
in a solution build.

---

## Configurations

Both configurations apply to both platforms. Nothing in the project files is
platform-specific, so the configuration groups are conditioned on
`'$(Configuration)'` alone rather than on `Configuration|Platform`.

| | Debug | Release |
|---|---|---|
| `_DEBUG` / `NDEBUG` | `_DEBUG` | `NDEBUG` |
| Subsystem | Windows (`Species`), Console (`Server`) | same |
| Precompiled header | Used | Used |
| Whole program optimisation | off | on |
| Debug info | generated | generated |
| `LanguageStandard` | `stdcpplatest` | `stdcpplatest` |

Both configurations carry identical `ClCompile` settings — include paths,
precompiled header, language standard, conformance — and the same subsystem.
Only the optimisation and `_DEBUG`/`NDEBUG` settings differ.

> **Release used to set `stdcpp20` against Debug's `stdcpplatest`**, in all six
> projects. Debug is the only configuration CI builds or anyone builds by hand,
> so the two never got compared — the same blind spot that hid the three Release
> defects below. Both are `stdcpplatest` now, which on toolset v145 is C++23
> plus whatever is in preview. That is deliberate: the tree tracks the latest
> standard rather than pinning a numbered one, so
> [`CODING_STANDARDS.md`](../CODING_STANDARDS.md)'s "C++23" is the floor you can
> rely on, not a ceiling the compiler enforces.
>
> **Nothing has compiled Release since**, so this is a corrected setting rather
> than a verified one — the same caveat as the subsystem fix below.
>
> The four `Tests/*` projects carried a sharper version of the same bug and are
> fixed with it. Each set `LanguageStandard` under
> `Condition="'$(Configuration)|$(Platform)'=='Debug|ARM64'"` — the shape the
> IDE writes when it records a setting against whichever configuration happened
> to be active — so **every other configuration, including the x64 Debug that CI
> builds and runs the suite in, compiled the tests at the toolset default.** The
> condition is gone; all four now set `stdcpplatest` unconditionally, matching
> the `AdditionalIncludeDirectories` beside them, which never carried one.
>
> All ten projects in the tree now agree. There is one setting to check when
> this next drifts: `grep -rn LanguageStandard --include=*.vcxproj .` should
> print ten lines saying `stdcpplatest` and nothing else.

Every configuration defines `_CRT_SECURE_NO_WARNINGS`,
`_CRT_NONSTDC_NO_WARNINGS` and `_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`,
because the legacy code still uses the deprecated CRT string functions.

> **Release had never built.** Exercising it in CI turned up three independent
> defects, all template leftovers rather than code problems:
>
> 1. Every project's Release configuration was missing
>    `AdditionalIncludeDirectories`, so it could not resolve a single
>    cross-project header.
> 2. `GameLogic`, `NeuronClient`, `NeuronCore` and `NeuronServer` set
>    `PrecompiledHeader=Use` in Release while `pch.cpp` only carried `Create` in
>    Debug — nothing produced the `.pch`, so every translation unit failed C1083.
> 3. `Species` set `SubSystem=Console` in Release against `Windows` in Debug.
>    `WinMain` lives in `NeuronClient/WindowManager.cpp`, so the console entry
>    point looked for `main` and the link failed with LNK2001.
>
> All three are fixed. (1) and (2) were confirmed by a CI run in which Release
> compiled every translation unit cleanly and failed only at the link step on
> (3). The fix for (3) was applied after CI dropped to Debug-only, so **no
> Release build has been run since** — it is expected to link, because Debug
> links with the same subsystem, but that is inference, not evidence. Build
> Release locally before anything that ships.

### 32-bit host toolchain

Each project sets:

```xml
<PreferredToolArchitecture Condition="'$(PreferredToolArchitecture)' == '' and '$(PROCESSOR_ARCHITECTURE)' != 'ARM64'">x64</PreferredToolArchitecture>
```

The x86-hosted cross compiler runs out of address space loading these PCHs
(C3859 / C1076), so a 64-bit host compiler is requested. On a native ARM64 host
the toolset already defaults to the arm64 tools and this does not apply.

---

## GameData

`GameData.targets` stages the `GameData/` tree next to the executable after every
build, and removes it on `Clean`. The copy is incremental —
`SkipUnchangedFiles` compares timestamp and size — and retries three times, since
a previous run of the game may still hold a file open.

Content is resolved at runtime relative to the working directory. **Run the
executable from its output directory**, or it will start and then fail to find
any asset.

To skip staging (for a compile-only check):

```powershell
msbuild Species.slnx /p:Configuration=Debug /p:Platform=ARM64 /p:GameDataSkipCopy=true
```

---

## Continuous integration

`.github/workflows/ci.yml` runs on every push, on every branch, and on every
pull request — work happens on a branch for a while before a PR exists, and the
build result is most useful earliest.

**Static checks** (`ubuntu-latest`, under a minute):

| Check | What it catches |
|---|---|
| `tools/check_project_files.py` | A source file on disk that no `.vcxproj` compiles |
| `tools/check_layering.py` | A new include pointing at a higher layer |
| `tools/check_task_dag.py` | A task plan with a cycle, dangling edge or inconsistent status |
| `tools/check_containers.py` | A call site still asking a `std::vector` for `Size()` or `ValidIndex()` |
| `tools/check_math_types.py` | A call site still asking a native math type for `Mag()`, an operator or a named row |
| `tools/check_format.py` | Changed lines that do not match `.clang-format` |
| `tools/check_hygiene.py` | Changed lines reintroducing `NULL`, `_included` guards, `strcpy` or a plain `enum` |

`check_containers.py` and `check_math_types.py` exist because the same mistake
cost several consecutive red CI rounds each: a call site a type sweep did not
reach, which names no type and so survives every grep for one. Both resolve by
member name, and both skip a name that means two things rather than guessing.
AGENTS.md explains what that trade buys.

The first two cover `Tests/<Name>Tests` as well as the six library projects, and
discover those directories from the tree rather than from a hard-coded list — so
a test project added later is checked from the moment it exists.

**Build and test**: one job, **x64 Debug**, on `windows-2025-vs2026` with the
same v145 toolset used locally. It then:

- asserts `GameData` was staged beside the output;
- runs every `x64\Debug\*Tests.dll` through `vstest.console`, failing if fewer
  than four test DLLs are discovered — an empty glob would otherwise let the run
  go green having tested nothing;
- uploads the `.trx` as an artifact when the run fails.

The tests share the build job rather than getting their own because they need the
build output, and shipping several hundred megabytes of `.lib` and `.obj` between
jobs to save nothing is not a trade worth making.

CI is a compile-and-unit-test gate, not a coverage matrix. Two things it
deliberately does not build, both of which are therefore your responsibility:

- **Release.** It differs from Debug in optimisation settings alone and catches
  little Debug does not. Build it before anything that ships.
- **ARM64.** It is the primary development platform, so it gets built constantly
  on the desk, and the `windows-11-vs2026-arm` runner is a preview image that
  took over fifteen minutes per build against roughly ninety seconds on x64.

> **ARM64 Debug has one unexplained failure.** While the ARM64 job still existed,
> a run failed immediately after `NeuronClient.lib` linked — placing the break in
> `GameLogic` or `Species` — but the first error had scrolled past the
> retrievable portion of the log and was never isolated. A later run on the same
> code got much further without failing, so it may have been a flake. If you hit
> an ARM64 Debug error, capture the first `error C...` line and record it in
> AGENTS.md rather than assuming it is new.

Run the static checks locally before pushing — they are fast and they fail on
exactly the mistakes that are easy to make without an IDE open:

```bash
python3 tools/check_project_files.py
python3 tools/check_layering.py
python3 tools/check_task_dag.py
python3 tools/check_containers.py
python3 tools/check_math_types.py
python3 tools/check_format.py            # --fix to apply
python3 tools/check_hygiene.py
```

`check_format.py` and `check_hygiene.py` look at *changed lines*, and they
compare against `merge-base(HEAD, origin/main)`. After merging `main` into a
branch, commit the merge before running them — until you do, `main`'s own lines
count as yours and get reported.

The Python tools need `pyyaml`; `check_format.py` needs `clang-format` 18 on
`PATH`. CI pins clang-format 18 so local and CI results agree.

---

## Troubleshooting

**Unresolved externals for a function you just wrote.** The `.cpp` is not in the
`.vcxproj`. `python3 tools/check_project_files.py`.

**`Cannot open include file` for a header that plainly exists.** Includes are
resolved by bare basename through `AdditionalIncludeDirectories`. Either the
owning project is not on the including project's include path, or you are
reaching upward through the layering — which should not work and should not be
made to work. See [`ARCHITECTURE.md`](ARCHITECTURE.md).

**C3859 / C1076, out of heap space.** Two different causes, and it is worth
telling them apart before changing anything.

*Reproducible, every build, same file:* the host compiler is 32-bit. Confirm
`PreferredToolArchitecture` is taking effect, or build from a 64-bit developer
prompt.

*Intermittent, a different file each time:* the machine is short of memory. These
PCHs are large and every `cl.exe` has to map one, so on a 16 GB machine with
Visual Studio open — `devenv` alone holds around 1.7 GB, plus the ReSharper
backend — a build can simply run out of room. Reducing `/m` does not reliably
help; one `cl.exe` short of address space is enough. Close Visual Studio, or
re-run: each pass gets further, and a project that failed inside a solution build
usually succeeds when built on its own. Lingering MSBuild nodes make it worse, so
add `/nr:false` if a dozen of them have accumulated. Do not treat it as a break
in whichever file it happened to land on — see AGENTS.md, Known issues.

**A test binary fails to link with an unresolved external in a library you did
not touch.** The library reaches upward and the symbol lives in an executable
that a test DLL does not link. That is a layering defect surfacing, not a build
misconfiguration; [`TESTING.md`](TESTING.md) covers what to do about it and why
adding a stub is a last resort.

**A test that no longer exists keeps showing up in the results.** Its DLL is
still in the solution output directory. `Clean` only removes what a project
still in the tree recorded, so delete the orphan by hand.

**The game starts and immediately fails to load content.** It is being run from
the wrong working directory. Run it from its output directory, where
`GameData.targets` staged the tree.

**Deleted files still appear in the build output.** The C++ `Clean` target only
removes what the compiler and linker recorded. `CleanGameData` handles the staged
content tree; for object files, delete the intermediate directory.
