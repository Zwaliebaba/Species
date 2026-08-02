# Building Species

## Requirements

| | |
|---|---|
| OS | Windows. The code uses Win32, Winsock, WGL and DirectSound directly. |
| Toolchain | Visual Studio 2026, MSVC toolset **v145**, Windows SDK 10 |
| Language | C++20 (`/std:c++20`, `/permissive-`) |
| Platforms | **ARM64** (primary) and **x64** |
| Dependencies | None. Links only against `opengl32`, `glu32`, `winmm`, `dsound`, `dxguid`, `Ws2_32`. |

There is nothing to install, restore or vendor. Clone and build.

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

Both configurations carry identical `ClCompile` settings — include paths,
precompiled header, language standard, conformance — and the same subsystem.
Only the optimisation and `_DEBUG`/`NDEBUG` settings differ.

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
> All three are fixed and Release compiles cleanly. It is still the
> less-travelled configuration, and CI no longer gates on it — build it locally
> before anything that ships.

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

`.github/workflows/ci.yml` runs on every push to `main` and every pull request.

**Static checks** (`ubuntu-latest`, under a minute):

| Check | What it catches |
|---|---|
| `tools/check_project_files.py` | A source file on disk that no `.vcxproj` compiles |
| `tools/check_layering.py` | A new include pointing at a higher layer |
| `tools/check_task_dag.py` | A task plan with a cycle, dangling edge or inconsistent status |
| `tools/check_format.py` | Changed lines that do not match `.clang-format` |

**Build**, Debug only, with the same v145 toolset used locally:

| Platform | Runner image |
|---|---|
| x64 | `windows-2025-vs2026` (GA) |
| ARM64 | `windows-11-vs2026-arm` (public preview) |

Each build job then asserts `GameData` was staged beside the output.

Release is not built in CI. It differs from Debug in optimisation settings
alone, and catches little Debug does not for twice the runner time — so building
it is your responsibility before anything that ships.

> The ARM64 image is a public preview. The `windows-11-arm` label migrates onto
> the VS 2026 image in early September 2026, after which the matrix entry can be
> renamed and the preview label dropped.

Run the static checks locally before pushing — they are fast and they fail on
exactly the mistakes that are easy to make without an IDE open:

```bash
python3 tools/check_project_files.py
python3 tools/check_layering.py
python3 tools/check_task_dag.py
python3 tools/check_format.py            # --fix to apply
```

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

**C3859 / C1076, out of heap space.** The host compiler is 32-bit. Confirm
`PreferredToolArchitecture` is taking effect, or build from a 64-bit developer
prompt.

**The game starts and immediately fails to load content.** It is being run from
the wrong working directory. Run it from its output directory, where
`GameData.targets` staged the tree.

**Deleted files still appear in the build output.** The C++ `Clean` target only
removes what the compiler and linker recorded. `CleanGameData` handles the staged
content tree; for object files, delete the intermediate directory.
