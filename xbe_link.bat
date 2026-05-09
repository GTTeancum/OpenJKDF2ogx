@echo off
REM xbe_link.bat - Patches PE subsystem to 14, then runs imagebld to produce XBE
REM Usage: xbe_link.bat <OutDir>

SET OUTDIR=%1
SET PE_IN=%OUTDIR%\openjkdf2_xbox.exe
SET XBE_OUT=%OUTDIR%\default.xbe
SET SCRIPT=%~dp0patch_subsystem.py

echo XBE Build: %PE_IN% -^> %XBE_OUT%

IF NOT EXIST "%PE_IN%" (
    echo ERROR: Input PE not found: %PE_IN%
    exit /b 1
)

REM Find Python - try common locations
SET PYTHON=
IF EXIST "%LOCALAPPDATA%\Programs\Python\Python313\python.exe" SET PYTHON=%LOCALAPPDATA%\Programs\Python\Python313\python.exe
IF EXIST "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" SET PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe
IF EXIST "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" SET PYTHON=%LOCALAPPDATA%\Programs\Python\Python311\python.exe
IF EXIST "%LOCALAPPDATA%\Programs\Python\Python310\python.exe" SET PYTHON=%LOCALAPPDATA%\Programs\Python\Python310\python.exe
IF EXIST "C:\Python313\python.exe" SET PYTHON=C:\Python313\python.exe
IF EXIST "C:\Python312\python.exe" SET PYTHON=C:\Python312\python.exe
IF EXIST "C:\Python311\python.exe" SET PYTHON=C:\Python311\python.exe
IF EXIST "C:\Python310\python.exe" SET PYTHON=C:\Python310\python.exe

IF "%PYTHON%"=="" (
    echo ERROR: Python not found. Install Python or add it to PATH.
    exit /b 1
)

echo Using Python: %PYTHON%

REM Step 1: Patch subsystem
"%PYTHON%" "%SCRIPT%" "%PE_IN%"
IF %ERRORLEVEL% NEQ 0 (
    echo Patch FAILED
    exit /b 1
)

REM Step 2: imagebld PE -> XBE
echo Running imagebld...
"C:\XDK\xbox\bin\imagebld.exe" /IN:"%PE_IN%" /OUT:"%XBE_OUT%" /TESTID:0x41560000 /TESTNAME:"OpenJKDF2" /LIMITMEM /DEBUG

IF %ERRORLEVEL% NEQ 0 (
    echo imagebld FAILED with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo XBE build succeeded: %XBE_OUT%
