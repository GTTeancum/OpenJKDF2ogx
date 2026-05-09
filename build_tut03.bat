@echo off
REM ============================================================================
REM  build_tut03.bat - Build Microsoft XDK Tut03_Matrices as a standalone XBE
REM                    with xbox_debug.c instrumentation wired in.
REM
REM  Purpose: have a verified-working D3D draw-call reference on this XDK 5849
REM  + retail hardware combination.  Tut03 is Microsoft's canonical "rotating
REM  triangle with XYZ vertices + SetTransform + DrawPrimitive" sample.
REM
REM  Build path mirrors Microsoft's matrices.vcproj Release_LTCG config:
REM    - XDK VC71 cl.exe + link.exe
REM    - Lib pairing: xapilib + d3d8ltcg + d3dx8 + xgraphicsltcg + dsound +
REM      xboxkrnl + libc  (matches Microsoft's sample exactly, plus libc).
REM    - patchxbe.py post-processing (now with library version injection).
REM
REM  Two TUs:
REM    1. tut03_matrices.cpp  — Microsoft's source, copied + lightly patched:
REM       * timeGetTime() -> GetTickCount()  (timeGetTime is in DSound.h)
REM       * XDBG calls inserted at every checkpoint
REM       * Compiled WITHOUT /FI"platform_xbox.h" — uses xtl.h via the sample
REM         original include.
REM    2. xbox_debug.c        — our existing NtCreateFile-based logger.
REM       * Compiled WITH /FI"platform_xbox.h" so it gets the same env our
REM         main app uses (where the logger is proven to work).
REM
REM  Output: build\xbox\test\default.xbe
REM ============================================================================

setlocal enabledelayedexpansion

REM -- Paths --
set CC=C:\XDK\xbox\bin\vc71\CL.Exe
set LINK=C:\XDK\xbox\bin\vc71\Link.Exe
set SRC_TUT03=%~dp0build\xbox\test\tut03_matrices.cpp
set SRC_DEBUG=%~dp0src\Platform\Xbox\xbox_debug.c
set OUTDIR=%~dp0build\xbox\test
set OBJDIR=%OUTDIR%\obj
set OUTEXE=%OUTDIR%\tut03_test.exe

if not exist "%OBJDIR%" mkdir "%OBJDIR%"

REM -- Common flags --
set CFLAGS_BASE=/nologo /MT /O2 /W2 /I"C:\XDK\xbox\include" /DWIN32 /DNDEBUG /D_X86_

REM Tut03 source: NO /FI — uses xtl.h cleanly as Microsoft intended.
set CFLAGS_TUT03=%CFLAGS_BASE%

REM xbox_debug.c: WITH /FI"platform_xbox.h" — gets stdint shims, BOOL typedef,
REM all the things our main build's xbox_debug.c gets.  Also include our
REM Xbox platform path for the header.
set CFLAGS_DEBUG=%CFLAGS_BASE% /I"%~dp0src\Platform\Xbox" /FI"platform_xbox.h" /DTARGET_XBOX=1

echo.
echo == Compiling tut03_matrices.cpp (C++, xtl.h) ==
"%CC%" /c /Tp "%SRC_TUT03%" %CFLAGS_TUT03% /Fo"%OBJDIR%\matrices.obj" 2>&1
if errorlevel 1 (
    echo *** COMPILE FAILED ^(matrices.cpp^)
    exit /b 1
)

echo.
echo == Compiling xbox_debug.c (C, platform_xbox.h) ==
"%CC%" /c /Tc "%SRC_DEBUG%" %CFLAGS_DEBUG% /Fo"%OBJDIR%\xbox_debug.obj" 2>&1
if errorlevel 1 (
    echo *** COMPILE FAILED ^(xbox_debug.c^)
    exit /b 1
)

REM -- Link --
REM Lib order matches Microsoft's matrices.vcproj Release_LTCG config:
REM   xapilib.lib d3d8ltcg.lib d3dx8.lib xgraphicsltcg.lib dsound.lib xboxkrnl.lib
REM We add libc.lib at the end (our xbox_debug.c uses memcpy/strlen/etc).
set RSPFILE=%OBJDIR%\link.rsp
> "%RSPFILE%" echo /nologo
>> "%RSPFILE%" echo /OUT:"%OUTEXE%"
>> "%RSPFILE%" echo /LIBPATH:"C:\XDK\xbox\lib"
>> "%RSPFILE%" echo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /FIXED:NO
>> "%RSPFILE%" echo /IGNORE:4254
>> "%RSPFILE%" echo xapilib.lib d3d8ltcg.lib d3dx8.lib xgraphicsltcg.lib dsound.lib xboxkrnl.lib libc.lib
>> "%RSPFILE%" echo "%OBJDIR%\matrices.obj"
>> "%RSPFILE%" echo "%OBJDIR%\xbox_debug.obj"

REM -- VC71 linker has issues with delayed expansion; use a temp batch --
set _POSTBAT=%OBJDIR%\do_post.bat
> "%_POSTBAT%" echo @echo off
>> "%_POSTBAT%" echo echo == Linking ==
>> "%_POSTBAT%" echo "%LINK%" @"%RSPFILE%"
>> "%_POSTBAT%" echo if errorlevel 1 exit /b 1
>> "%_POSTBAT%" echo echo == Patching to XBE ==
>> "%_POSTBAT%" echo python "%~dp0patchxbe.py" "%OUTEXE%" "%OUTDIR%\default.xbe"
>> "%_POSTBAT%" echo if errorlevel 1 exit /b 1

endlocal
call "%~dp0build\xbox\test\obj\do_post.bat"
if errorlevel 1 (
    echo.
    echo == BUILD FAILED ==
    exit /b 1
)

echo.
echo ============================================================
echo  Tut03 BUILD SUCCEEDED
echo  XBE: %~dp0build\xbox\test\default.xbe
echo ============================================================
