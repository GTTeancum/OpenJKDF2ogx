@echo off
REM ============================================================================
REM  build_xbox.bat - Build OpenJKDF2 Xbox port using XDK 5849 VC71 toolchain
REM  Usage: build_xbox.bat [clean]
REM ============================================================================

setlocal enabledelayedexpansion

REM Always run from the script's own directory regardless of caller cwd.
cd /d "%~dp0"

REM ── Paths ──────────────────────────────────────────────────────────────────
REM  Switched to XDK 5558.  XDK 5849 was missing d3d8types-xbox.h (the file
REM  that gives Xbox-correct enum values for D3DPT_*, D3DRS_*, D3DTSS_*,
REM  D3DFMT_*) and shipped only an LTCG-stripped 3-symbol d3d8.lib stub,
REM  forcing reliance on d3d8ltcg.lib whose CPU-side path locks NV2A on
REM  retail hardware in our setup.  XDK 5558's D3D8Types.h already has
REM  Xbox-correct values, and its full d3d8.lib (2.1 MB, 214 D3DDevice
REM  exports — versus 5849's 3.5 KB stub) gives us a non-LTCG link path.
set XDK_ROOT=C:\XDK_5558\XDK\xbox
set CC=%XDK_ROOT%\bin\vc71\CL.Exe
set LINK=%XDK_ROOT%\bin\vc71\Link.Exe
set IMAGEBLD=%XDK_ROOT%\bin\imagebld.exe
set SRCDIR=%~dp0src
set OBJDIR=%~dp0build\xbox\release\obj
set OUTDIR=%~dp0build\xbox\release
set OUTEXE=%OUTDIR%\openjkdf2_xbox.exe
set PYTHON=python

REM ── Clean ──────────────────────────────────────────────────────────────────
if "%1"=="clean" (
    echo Cleaning...
    if exist "%OBJDIR%" rd /s /q "%OBJDIR%"
    if exist "%OUTEXE%" del /q "%OUTEXE%"
    echo Done.
    goto :eof
)

REM ── Create output dirs ─────────────────────────────────────────────────────
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ── Common compiler flags ──────────────────────────────────────────────────
set CFLAGS=/nologo /MT /O2 /W2
REM Include order: project sources → XDK 5558 (Xbox-correct D3D8Types.h with
REM D3DPT_TRIANGLELIST=5 etc.) → XDK 5849 fallback (for headers 5558 lacks
REM like stdint.h, winsock2.h).
set CFLAGS=%CFLAGS% /I"%~dp0src" /I"%~dp0src\Platform\Xbox" /I"%~dp03rdparty" /I"%XDK_ROOT%\include" /I"C:\XDK\xbox\include"
set CFLAGS=%CFLAGS% /DTARGET_XBOX=1 /DSDL2_RENDER=1 /DSTDSOUND_NULL=1 /D_XBOX=1
set CFLAGS=%CFLAGS% /DPLATFORM_NOSOCKETS=1 /DQOL_IMPROVEMENTS=1
set CFLAGS=%CFLAGS% /DTARGET_NO_MULTIPLAYER_MENUS=1 /DLINUX_TMP=1
set CFLAGS=%CFLAGS% /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS
set CFLAGS=%CFLAGS% /DWIN32 /DNDEBUG
set CFLAGS=%CFLAGS% /FI"platform_xbox.h"
set CFLAGS=%CFLAGS% /wd4996 /wd4244 /wd4267 /wd4305 /wd4013 /wd4133 /wd4114 /wd4028

set ERRORS=0
set COMPILED=0

REM ── Pre-build audit ────────────────────────────────────────────────────────
echo.
echo ══════════════════════════════════════════════════════════════
echo  Running pre-build audit...
echo ══════════════════════════════════════════════════════════════
%PYTHON% "%~dp0audit_xbox.py"
if errorlevel 1 (
    echo.
    echo  PRE-BUILD AUDIT FAILED — fix issues above before compiling.
    exit /b 1
)

REM ── Compile C files (21 files) ─────────────────────────────────────────────
echo.
echo ══════════════════════════════════════════════════════════════
echo  Compiling C files...
echo ══════════════════════════════════════════════════════════════

for %%F in (
    src\Platform\Xbox\xbox_debug.c
    src\Platform\Xbox\crt_aliases.c
    src\Platform\Xbox\quake_stubs.c
) do (
    set "SRC=%%F"
    for %%N in (%%~nF) do set "OBJ=%OBJDIR%\%%N.obj"
    echo   [C] %%F
    "%CC%" /c /Tc "%%F" %CFLAGS% /Fo"!OBJ!" 2>&1
    if errorlevel 1 (
        echo   *** FAILED: %%F
        set /a ERRORS+=1
    ) else (
        set /a COMPILED+=1
    )
)

REM ── Compile C++ files (14 files) ───────────────────────────────────────────
echo.
echo ══════════════════════════════════════════════════════════════
echo  Compiling C++ files...
echo ══════════════════════════════════════════════════════════════

for %%F in (
    src\globals.c
    src\main_globals.c
    src\Main\jkMain.c
    src\Main\jkRes.c
    src\Main\Main.c
    src\Main\jkGob.c
    src\Main\jkControl.c
    src\General\Darray.c
    src\General\crc32.c
    src\General\stdFnames.c
    src\General\stdPalEffects.c
    src\Raster\rdCache.c
    src\Raster\rdFace.c
    src\Platform\Xbox\wuRegistry_xbox.c
    src\jk.c
    src\wprintf.c
    src\Win95\std.c
    src\General\stdString.c
    src\General\stdStrTable.c
    src\General\stdHashTable.c
    src\General\stdMath.c
    src\General\stdMemory.c
    src\General\stdLinklist.c
    src\General\stdSingleLinklist.c
    src\General\stdConffile.c
    src\Main\sithCvar.c
    src\Main\sithMain.c
    src\World\sithWorld.c
    src\World\sithSector.c
    src\World\sithSurface.c
    src\World\sithThing.c
    src\World\sithActor.c
    src\World\sithModel.c
    src\World\sithSprite.c
    src\World\sithTemplate.c
    src\World\sithMaterial.c
    src\World\sithSoundClass.c
    src\Gameplay\sithPlayer.c
    src\Engine\sithPhysics.c
    src\Engine\sithCollision.c
    src\Engine\sithIntersect.c
    src\Engine\sithRender.c
    src\Engine\sithRenderSky.c
    src\Engine\sithCamera.c
    src\Gameplay\sithTime.c
    src\Engine\sithKeyFrame.c
    src\Engine\sithAnimClass.c
    src\Engine\rdCamera.c
    src\Engine\rdMaterial.c
    src\Engine\rdColormap.c
    src\Engine\rdKeyframe.c
    src\Engine\rdPuppet.c
    src\Engine\rdClip.c
    src\Engine\rdCanvas.c
    src\Engine\rdroid.c
    src\Engine\rdLight.c
    src\Engine\rdActive.c
    src\Primitives\rdModel3.c
    src\Primitives\rdPrimit3.c
    src\Primitives\rdMath.c
    src\Primitives\rdVector.c
    src\Primitives\rdMatrix.c
    src\Engine\rdThing.c
    src\Cog\sithCog.c
    src\Cog\sithCogParse.c
    src\Cog\sithCogExec.c
    src\Cog\sithCogFunction.c
    src\Cog\sithCogFunctionAI.c
    src\Cog\sithCogFunctionPlayer.c
    src\Cog\sithCogFunctionSector.c
    src\Cog\sithCogFunctionSound.c
    src\Cog\sithCogFunctionSurface.c
    src\Cog\sithCogFunctionThing.c
    src\Cog\jkCog.c
    src\Cog\lex.yy.c
    src\Cog\y.tab.c
    src\AI\sithAI.c
    src\AI\sithAIAwareness.c
    src\AI\sithAICmd.c
    src\AI\sithAIClass.c
    src\Devices\sithConsole.c
    src\Devices\sithControl.c
    src\Devices\sithSound.c
    src\Devices\sithSoundMixer.c
    src\Engine\sithParticle.c
    src\Engine\sithPuppet.c
    src\Gameplay\sithEvent.c
    src\Gameplay\sithInventory.c
    src\Gameplay\sithOverlayMap.c
    src\Gameplay\sithPlayerActions.c
    src\General\sithStrTable.c
    src\Main\sithCommand.c
    src\World\sithArchLighting.c
    src\World\sithExplosion.c
    src\World\sithItem.c
    src\World\sithMap.c
    src\World\sithTrackThing.c
    src\World\sithWeapon.c
    src\World\jkPlayer.c
    src\Gameplay\jkSaber.c
    src\Main\jkAI.c
    src\Main\jkEpisode.c
    src\Main\jkGame.c
    src\Main\jkHud.c
    src\Main\jkHudInv.c
    src\Main\jkHudScope.c
    src\Main\jkHudCameraView.c
    src\Main\jkStrings.c
    src\Platform\Xbox\std3D.c
    src\Platform\Xbox\std3D_draw.cpp
    src\Platform\Xbox\fakeglx.cpp
    src\Platform\Xbox\xbox_world_helper.cpp
    src\Platform\Xbox\main_xbox.c
    src\Platform\Xbox\stdControl_xbox.c
    src\Platform\Xbox\stdFile_xbox.c
    src\Platform\Xbox\stdSound_xbox.c
    src\Platform\Xbox\stdPlatform_xbox.c
    src\Platform\Xbox\Window_xbox.c
    src\Platform\Xbox\xbox_stubs.c
    src\Platform\Xbox\xbox_autostubs.c
    src\Platform\Xbox\stdGob_xbox.c
) do (
    set "SRC=%%F"
    for %%N in (%%~nF) do set "OBJ=%OBJDIR%\%%N.obj"
    echo   [C++] %%F
    "%CC%" /c /Tp "%%F" %CFLAGS% /Fo"!OBJ!" 2>&1
    if errorlevel 1 (
        echo   *** FAILED: %%F
        set /a ERRORS+=1
    ) else (
        set /a COMPILED+=1
    )
)

REM ── Check for compile errors ───────────────────────────────────────────────
echo.
echo  Compiled: !COMPILED!/34  Errors: !ERRORS!
if !ERRORS! GTR 0 (
    echo.
    echo  BUILD FAILED
    exit /b 1
)

REM ── Link ───────────────────────────────────────────────────────────────────
echo.
echo ══════════════════════════════════════════════════════════════
echo  Linking...
echo ══════════════════════════════════════════════════════════════

REM Build linker response file with all obj files
set RSPFILE=%OBJDIR%\link.rsp
> "!RSPFILE!" echo /nologo
>> "!RSPFILE!" echo /OUT:"!OUTEXE!"
>> "!RSPFILE!" echo /LIBPATH:"!XDK_ROOT!\lib"
>> "!RSPFILE!" echo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /FIXED:NO
>> "!RSPFILE!" echo /IGNORE:4254
REM XDK 5558 has full d3d8.lib + xgraphics.lib (non-LTCG variants).
REM This sidesteps any LTCG-specific issues in d3d8ltcg.lib.
>> "!RSPFILE!" echo d3d8.lib d3dx8.lib dsound.lib xboxkrnl.lib xgraphics.lib xonline.lib libc.lib xapilib.lib
for %%O in (!OBJDIR!\*.obj) do >> "!RSPFILE!" echo "%%O"

REM VC71 link.exe breaks under enabledelayedexpansion.
REM Write link + post-build to a temp batch and run it without delayed expansion.
set _POSTBAT=!OBJDIR!\do_post.bat
> "!_POSTBAT!" echo @echo off
>> "!_POSTBAT!" echo echo  Linking...
>> "!_POSTBAT!" echo "!LINK!" @"!RSPFILE!"
>> "!_POSTBAT!" echo if errorlevel 1 exit /b 1
>> "!_POSTBAT!" echo echo  Output: !OUTEXE!
>> "!_POSTBAT!" echo echo  Running patchxbe.py...
>> "!_POSTBAT!" echo python "%~dp0patchxbe.py" "!OUTEXE!" "!OUTDIR!\default.xbe"
>> "!_POSTBAT!" echo if errorlevel 1 exit /b 1

endlocal
call "%~dp0build\xbox\release\obj\do_post.bat"

if errorlevel 1 (
    echo.
    echo  XBE GENERATION FAILED
    exit /b 1
)

echo.
echo ══════════════════════════════════════════════════════════════
echo  BUILD SUCCEEDED
echo  EXE: %OUTEXE%
echo  XBE: %OUTDIR%\default.xbe
echo ══════════════════════════════════════════════════════════════
