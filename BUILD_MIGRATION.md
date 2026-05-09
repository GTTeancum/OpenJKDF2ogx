# Xbox XDK Build Migration: VS2005 to Batch/Makefile

## Problem

After a drive failure and VS2005/XDK reinstall, the project could no longer compile. VS2005's `cl.exe` (v14, VC8) cannot compile Xbox XDK D3D8 headers in C89 mode. The COM interface macros (`STDMETHOD`, `DECLARE_INTERFACE_`) produce syntax errors because VS2005's compiler handles them differently than the XDK's native compiler.

The XDK ships with its own VC71 compiler (`C:\XDK\xbox\bin\vc71\CL.Exe`, v13.10) which handles these headers correctly. However, VS2005 ignores `ExecutablePath` in vcproj files for the compiler — it always uses its own `cl.exe`. There is no supported way to make VS2005 use an external compiler through a standard vcproj.

## Solution

Convert the VS2005 project to a **Makefile/NMake configuration** that calls the XDK VC71 toolchain via a batch file (`build_xbox.bat`). VS2005 remains usable as an IDE for code browsing, IntelliSense, and triggering builds via F7.

## What Changed

### 1. `build_xbox.bat` (new file, repo root)

Batch file that drives the entire build:
- Calls `C:\XDK\xbox\bin\vc71\CL.Exe` for compilation
- Calls `C:\XDK\xbox\bin\vc71\Link.Exe` for linking (via response file)
- Compiles C files with `/Tc` flag, C++ files with `/Tp` flag
- Runs `patchxbe.py` as post-build to convert PE to XBE
- Supports `build_xbox.bat clean` for cleaning

Key details:
- The XDK VC71 `link.exe` breaks under `setlocal enabledelayedexpansion`. The batch uses delayed expansion for the compile loop, then writes a temp batch file (`do_post.bat`) containing the link + post-build commands, calls `endlocal`, and runs that temp batch outside the delayed expansion context.
- A linker response file (`link.rsp`) is generated dynamically with all `.obj` paths.
- The `*.obj` wildcard doesn't work reliably with the VC71 linker; the response file lists each obj explicitly.

### 2. `OpenJKDF2_Xbox.vcproj` (modified)

The **Release|Win32** configuration was changed from `ConfigurationType="1"` (application) to `ConfigurationType="0"` (makefile/utility). The build tools were replaced with a single `VCNMakeTool` entry:

```xml
<Tool
    Name="VCNMakeTool"
    BuildCommandLine="$(SolutionDir)build_xbox.bat"
    ReBuildCommandLine="$(SolutionDir)build_xbox.bat clean &amp;&amp; $(SolutionDir)build_xbox.bat"
    CleanCommandLine="$(SolutionDir)build_xbox.bat clean"
    Output="$(OutDir)\openjkdf2_xbox.exe"
    PreprocessorDefinitions="..."
    IncludeSearchPath="..."
    ForcedIncludes="platform_xbox.h"
/>
```

The `PreprocessorDefinitions`, `IncludeSearchPath`, and `ForcedIncludes` fields are for IntelliSense only — they don't affect the actual build (the batch file has its own flags).

All source files remain listed in the vcproj for code browsing. The Debug configuration was left unchanged (it still uses VS2005's compiler — it was already broken before and is unused).

### 3. XDK Include Shims (files added to `C:\XDK\xbox\include\`)

The XDK 5849 install was incomplete — missing several headers that the D3D8 and engine code expect. Shim/stub headers were created:

| File | Purpose |
|---|---|
| `objbase.h` | Redirects to `XObjBase.h` (Xbox COM base) |
| `windows.h` | Includes `windef.h` + `winbase.h` + `wingdi.h` |
| `wingdi.h` | Defines `PALETTEENTRY`, `LOGFONT`, `RGNDATA`, `GLYPHMETRICSFLOAT` — GDI types referenced by D3D8 headers but not present on Xbox |
| `stdint.h` | Empty shim (typedefs provided by `platform_xbox.h` force-include) |
| `stdbool.h` | `bool`/`true`/`false` for VC71 C mode |
| `winsock2.h` | Empty shim (networking stubbed) |

### 4. `platform_xbox.h` (modified)

Changed from `#include <xtl.h>` to individual XDK headers:
```c
#include <excpt.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <xbox.h>
```

**Why:** `xtl.h` pulls in `d3d8.h` which has COM interfaces that fail in C89 mode with both VS2005 and the VC71 compiler. Only C++ files that explicitly need D3D should include `xtl.h` directly.

Also added `_X86_` and `WINVER` defines before the XDK includes (mirrors what `xtl.h` sets internally).

Added `extern "C"` declaration for `std_pHS` so C and C++ translation units share one linker symbol.

### 5. C/C++ Linkage Strategy

The engine codebase was designed for a single compiler mode. On Xbox, some files must compile as C++ (they use C99 features or include `xtl.h` for D3D) while others compile as C89. This creates linkage mismatches:

- **C files** export symbols with C linkage (`_functionName`)
- **C++ files** export symbols with C++ mangling (`?functionName@@...`)
- A C file calling a function defined in a C++ file gets an unresolved external

Solutions applied:
- `extern "C"` blocks in headers shared between C and C++ (e.g., `stdGob.h`)
- `extern "C"` on specific function declarations in C++ files (e.g., `stdFile_xbox.c` for `NtCreateFile`/`NtClose`, `stdFile_xbox_Startup`)
- `extern "C"` block wrapping the bulk of `xbox_stubs.c` (compiled as C++ but most stubs need C linkage)
- C++ stubs for functions referenced by C++ callers placed outside the `extern "C"` block
- CRT aliases in `crt_aliases.c` (compiled as C) providing C-linkage versions of functions that `jk.c` (C++) also defines with C++ mangling
- `stdGob_xbox.c` compiled as C++ but with `extern "C"` in the header

### 6. File List (C vs C++)

**C files (14):** `globals.c`, `main_globals.c`, `jkMain.c`, `jkRes.c`, `Main.c`, `jkGob.c`, `Darray.c`, `crc32.c`, `stdFnames.c`, `rdCache.c`, `rdFace.c`, `xbox_debug.c`, `crt_aliases.c`, `std3D_xbox_stub.c`, `wuRegistry_xbox.c`

**C++ files (19):** `jk.c`, `wprintf.c`, `std.c`, `stdString.c`, `stdHashTable.c`, `stdMath.c`, `stdMemory.c`, `stdLinklist.c`, `stdSingleLinklist.c`, `sithCvar.c`, `main_xbox.c`, `stdControl_xbox.c`, `stdFile_xbox.c`, `stdSound_xbox.c`, `stdPlatform_xbox.c`, `Window_xbox.c`, `xbox_stubs.c`, `stdGob_xbox.c`

**Rule:** Any file that includes `<xtl.h>` (directly or needs D3D types) MUST be in the C++ list. Any file that uses C99 features (compound literals, mixed declarations) also needs C++.

### 7. Other Source Changes

- `types_win_enums.h`: `D3DBLEND_*` macros gated behind `#ifndef TARGET_XBOX` to prevent conflicts with real D3D8Types.h enums
- `InstallHelper.h/.c`, `Main.c`: `nfd.h` (native file dialog) includes gated behind `#ifndef TARGET_XBOX`
- `globals.h`, `globals.c`: `std_pHS` definition gated behind `#ifndef TARGET_XBOX` (Xbox provides it via `xbox_stubs.c` with `extern "C"`)
- `stdPlatform_xbox.c`: Includes `<xtl.h>` before engine headers to prevent D3D enum conflicts
- `std3D.c`: Includes `<xtl.h>` before `platform_xbox.h` for same reason; adds `typedef IDirect3DTexture8 D3DTexture` since Xbox-specific `D3D8-Xbox.h` is incomplete
- `xbox_debug.h`: `XDBGF` macro changed from `__VA_ARGS__` (C99) to direct function alias (VC71 C89 doesn't support variadic macros)
- `jk.h`: Added `extern "C"` inline stubs for `jk_UnmapViewOfFile`/`jk_CloseHandle` on Xbox
- `patchxbe.py`: Removed `/SAVEIMAGE` flag (unsupported by this XDK's `imagebld.exe`)

## How to Build

From command line:
```
cd C:\Programming\GitHub\OpenJKDF2ogx
build_xbox.bat
```

From VS2005: Open solution, select Release configuration, press F7.

To clean: `build_xbox.bat clean`

Output:
- `build\xbox\release\openjkdf2_xbox.exe` (PE)
- `build\xbox\release\default.xbe` (Xbox executable)

## How to Add New Source Files

1. Add the file to the vcproj (for IDE browsing)
2. Add the file to `build_xbox.bat` in the appropriate list (C or C++)
3. If the file includes `<xtl.h>` or uses C99 features, it goes in the C++ list
4. If the file defines functions called from both C and C++ translation units, add `extern "C"` guards to its header

## Known Issues

- `d3d8-xbox.lib` in the linker libs is the debug D3D lib which can hang on retail hardware. When re-enabling real D3D rendering, switch to `d3d8.lib` (retail).
- The `LIBCMT` default lib conflict warning is harmless (XDK CRT vs VS2005 CRT).
- The `DSOUND`/`XPP` section attribute warnings are harmless (XDK lib quirk).
- 33/34 compile count is because the batch reports the total against a hardcoded `/34` that should be updated when files are added/removed.
