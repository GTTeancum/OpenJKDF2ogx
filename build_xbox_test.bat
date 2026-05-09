@echo off
REM ============================================================================
REM  build_xbox_test.bat - Build the Tut01 isolation test XBE
REM  Uses the EXACT same toolchain + patchxbe.py as build_xbox.bat.
REM  Links with XDK Release canonical libs (d3d8.lib + xgraphics.lib).
REM ============================================================================

setlocal enabledelayedexpansion

set CC=C:\XDK\xbox\bin\vc71\CL.Exe
set LINK=C:\XDK\xbox\bin\vc71\Link.Exe
set SRCDIR=%~dp0test_tut01
set OBJDIR=%~dp0build\xbox\test\obj
set OUTDIR=%~dp0build\xbox\test
set OUTEXE=%OUTDIR%\tut01_test.exe

if "%1"=="clean" (
    if exist "%OBJDIR%" rd /s /q "%OBJDIR%"
    if exist "%OUTEXE%" del /q "%OUTEXE%"
    echo Done.
    goto :eof
)

if not exist "%OBJDIR%" mkdir "%OBJDIR%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM Minimal flags — no FI header, no project-specific defines.
REM xtl.h wants _XBOX defined.
set CFLAGS=/nologo /MT /O2 /W2
set CFLAGS=%CFLAGS% /I"C:\XDK\xbox\include"
set CFLAGS=%CFLAGS% /D_XBOX /DNDEBUG
set CFLAGS=%CFLAGS% /D_CRT_SECURE_NO_WARNINGS

echo.
echo ══════════════════════════════════════════════════════════════
echo  Compiling test_tut01\tut01_test.cpp ...
echo ══════════════════════════════════════════════════════════════

"%CC%" /c /Tp "%SRCDIR%\tut01_test.cpp" %CFLAGS% /Fo"%OBJDIR%\tut01_test.obj"
if errorlevel 1 (
    echo.
    echo  COMPILE FAILED
    exit /b 1
)

echo.
echo ══════════════════════════════════════════════════════════════
echo  Linking ...
echo ══════════════════════════════════════════════════════════════

set RSPFILE=%OBJDIR%\link.rsp
> "!RSPFILE!" echo /nologo
>> "!RSPFILE!" echo /OUT:"!OUTEXE!"
>> "!RSPFILE!" echo /LIBPATH:"C:\XDK\xbox\lib"
>> "!RSPFILE!" echo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /FIXED:NO
>> "!RSPFILE!" echo /IGNORE:4254
REM imagebld in this XDK rejects by-name imports from d3d8.dll (Release config).
REM Use LTCG canonical pair (self-contained, matches main project now).
>> "!RSPFILE!" echo xapilib.lib d3d8ltcg.lib xgraphicsltcg.lib dsound.lib xboxkrnl.lib libc.lib
>> "!RSPFILE!" echo "%OBJDIR%\tut01_test.obj"

set _POSTBAT=!OBJDIR!\do_post.bat
> "!_POSTBAT!" echo @echo off
>> "!_POSTBAT!" echo "!LINK!" @"!RSPFILE!"
>> "!_POSTBAT!" echo if errorlevel 1 exit /b 1
>> "!_POSTBAT!" echo echo  Output: !OUTEXE!
>> "!_POSTBAT!" echo echo  Running patchxbe.py...
>> "!_POSTBAT!" echo python "%~dp0patchxbe.py" "!OUTEXE!" "!OUTDIR!\default.xbe"
>> "!_POSTBAT!" echo if errorlevel 1 exit /b 1

endlocal
call "%~dp0build\xbox\test\obj\do_post.bat"

if errorlevel 1 (
    echo.
    echo  XBE GENERATION FAILED
    exit /b 1
)

echo.
echo ══════════════════════════════════════════════════════════════
echo  TEST BUILD SUCCEEDED
echo  XBE: %~dp0build\xbox\test\default.xbe
echo ══════════════════════════════════════════════════════════════
