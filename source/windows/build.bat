@echo off
REM SPDX-License-Identifier: GPL-2.0
REM
REM build.bat - Build the MOTU PCI-424 Windows WDF driver and test tools.
REM
REM Usage:
REM   build.bat           - Build everything (driver + test tool)
REM   build.bat driver    - Build only the kernel driver
REM   build.bat test      - Build only the userspace test tool
REM   build.bat clean     - Clean build output
REM
REM Output goes to build\windows\ (relative to repo root)

setlocal enabledelayedexpansion

REM ---- Resolve repo root from script location ------------------------------
REM This script lives at <repo>/source/windows/build.bat
REM So the repo root is two directories up.

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
set REPO_ROOT=%SCRIPT_DIR%\..\..
pushd "%REPO_ROOT%"
set REPO_ROOT=%CD%
popd

set SRC=%SCRIPT_DIR%\
set SHARED=%REPO_ROOT%\source\shared\
set BUILD=%REPO_ROOT%\build\windows\

REM ---- Configuration -------------------------------------------------------

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
set WDK_BASE=C:\Program Files (x86)\Windows Kits\10
set WDK_VER=10.0.26100.0
set WDK_INC=%WDK_BASE%\Include\%WDK_VER%
set WDK_LIB=%WDK_BASE%\Lib\%WDK_VER%
set WDF_VER=1.35
set WDF_INC=%WDK_BASE%\Include\wdf\kmdf\%WDF_VER%
set WDF_LIB=%WDK_BASE%\Lib\wdf\kmdf\x64\%WDF_VER%
set MSVC_VER=14.44.35207
set MSVC_BASE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\%MSVC_VER%

REM Allow overriding via environment variables
if defined VCVARS_PATH set VCVARS=%VCVARS_PATH%
if defined WDK_VERSION set WDK_VER=%WDK_VERSION%
if defined WDK_BASE_PATH set WDK_BASE=%WDK_BASE_PATH%
if defined MSVC_VERSION set MSVC_VER=%MSVC_VERSION%

REM Recompute derived paths after potential overrides
set WDK_INC=%WDK_BASE%\Include\%WDK_VER%
set WDK_LIB=%WDK_BASE%\Lib\%WDK_VER%
set WDF_INC=%WDK_BASE%\Include\wdf\kmdf\%WDF_VER%
set WDF_LIB=%WDK_BASE%\Lib\wdf\kmdf\x64\%WDF_VER%
set MSVC_BASE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\%MSVC_VER%

REM ---- Parse arguments ----------------------------------------------------

set TARGET=%1
if "%TARGET%"=="" set TARGET=all

if /i "%TARGET%"=="clean" (
    echo Cleaning build output...
    if exist "%BUILD%obj" rmdir /s /q "%BUILD%obj"
    if exist "%BUILD%motu424.sys" del "%BUILD%motu424.sys"
    if exist "%BUILD%motu424_test.exe" del "%BUILD%motu424_test.exe"
    mkdir "%BUILD%obj" 2>nul
    echo Done.
    exit /b 0
)

REM ---- Verify prerequisites ------------------------------------------------

if not exist "%VCVARS%" (
    echo ERROR: Visual Studio vcvarsall.bat not found.
    echo Install Visual Studio 2022 BuildTools with C++ workload.
    echo Or set VCVARS_PATH to the correct path.
    exit /b 1
)

if not exist "%WDK_INC%\km\ntddk.h" (
    echo ERROR: WDK not found.
    echo Install the Windows Driver Kit ^(WDK^).
    echo   winget install Microsoft.WindowsWDK.10.0.26100
    echo Or set WDK_BASE_PATH and WDK_VERSION to the correct paths.
    exit /b 1
)

REM ---- Set up build environment -------------------------------------------

echo Setting up build environment...
call "%VCVARS%" x64 >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to set up build environment.
    exit /b 1
)

REM Create output directories
mkdir "%BUILD%" 2>nul
mkdir "%BUILD%obj" 2>nul

REM ---- Create response file for include paths ----------------------------
REM Note: No trailing backslashes in paths (they escape the closing quote)

set SHARED_I=%SHARED:~0,-1%
echo -I"%SHARED_I%" > "%BUILD%obj\kernel_includes.rsp"
echo -I"%WDK_INC%\km" >> "%BUILD%obj\kernel_includes.rsp"
echo -I"%WDK_INC%\shared" >> "%BUILD%obj\kernel_includes.rsp"
echo -I"%WDF_INC%" >> "%BUILD%obj\kernel_includes.rsp"
echo -I"%MSVC_BASE%\include" >> "%BUILD%obj\kernel_includes.rsp"

REM ---- Compiler flags (kernel driver) ------------------------------------

set KERNEL_CFLAGS=/kernel /GS- /Zc:wchar_t /utf-8 /W3 /O2 /D_WIN64 /D_AMD64_ /DWIN32 /D_WIN32_WINNT=0x0A00 /c /Fo"%BUILD%obj\\"

REM ---- Build kernel driver ------------------------------------------------

if /i "%TARGET%"=="all" goto build_driver
if /i "%TARGET%"=="driver" goto build_driver
goto check_test

:build_driver
echo.
echo === Building kernel driver (motu424.sys) ===

echo Compiling shared\motu424_fpga.c...
cl %KERNEL_CFLAGS% @"%BUILD%obj\kernel_includes.rsp" "%SHARED%motu424_fpga.c"
if errorlevel 1 goto build_failed

echo Compiling shared\motu424_init.c...
cl %KERNEL_CFLAGS% @"%BUILD%obj\kernel_includes.rsp" "%SHARED%motu424_init.c"
if errorlevel 1 goto build_failed

echo Compiling shared\motu424_dma.c...
cl %KERNEL_CFLAGS% @"%BUILD%obj\kernel_includes.rsp" "%SHARED%motu424_dma.c"
if errorlevel 1 goto build_failed

echo Compiling windows\motu424_win_pal.c...
cl %KERNEL_CFLAGS% @"%BUILD%obj\kernel_includes.rsp" "%SRC%motu424_win_pal.c"
if errorlevel 1 goto build_failed

echo Compiling windows\motu424_wdf.c...
cl %KERNEL_CFLAGS% @"%BUILD%obj\kernel_includes.rsp" "%SRC%motu424_wdf.c"
if errorlevel 1 goto build_failed

echo Linking motu424.sys...
link /OUT:"%BUILD%motu424.sys" /SUBSYSTEM:NATIVE,10.0 /DRIVER /ENTRY:DriverEntry /NODEFAULTLIB /MACHINE:X64 /LIBPATH:"%WDK_LIB%\km\x64" /LIBPATH:"%WDF_LIB%" /LIBPATH:"%MSVC_BASE%\lib\x64" ntoskrnl.lib hal.lib wdfdriverentry.lib wdfldr.lib BufferOverflowK.lib "%BUILD%obj\motu424_fpga.obj" "%BUILD%obj\motu424_init.obj" "%BUILD%obj\motu424_dma.obj" "%BUILD%obj\motu424_win_pal.obj" "%BUILD%obj\motu424_wdf.obj"
if errorlevel 1 goto build_failed

echo Driver built: %BUILD%motu424.sys

REM Copy firmware and INF
copy "%SHARED%altera424b.rbf" "%BUILD%" >nul 2>&1
copy "%SHARED%init_sequence.bin" "%BUILD%" >nul 2>&1
copy "%SRC%motu424.inf" "%BUILD%" >nul 2>&1

if /i "%TARGET%"=="driver" goto build_done

:check_test
if /i "%TARGET%"=="all" goto build_test
if /i "%TARGET%"=="test" goto build_test
goto build_done

REM ---- Build userspace test tool ------------------------------------------

:build_test
echo.
echo === Building test tool (motu424_test.exe) ===

echo Compiling windows\motu424_test.c...
cl /utf-8 /W3 /O2 /D_WIN64 /D_AMD64_ /c /Fo"%BUILD%obj\\" -I"%SHARED_I%" "%SRC%motu424_test.c"
if errorlevel 1 goto build_failed

echo Linking motu424_test.exe...
link /OUT:"%BUILD%motu424_test.exe" /SUBSYSTEM:CONSOLE /MACHINE:X64 /LIBPATH:"%MSVC_BASE%\lib\x64" /LIBPATH:"%WDK_LIB%\um\x64" kernel32.lib user32.lib advapi32.lib "%BUILD%obj\motu424_test.obj"
if errorlevel 1 goto build_failed

echo Test tool built: %BUILD%motu424_test.exe

REM ---- Done ---------------------------------------------------------------

:build_done
echo.
echo === Build complete ===
echo Output directory: %BUILD%
echo.
dir /b "%BUILD%*.sys" "%BUILD%*.exe" "%BUILD%*.inf" 2>nul
echo.
exit /b 0

:build_failed
echo.
echo *** BUILD FAILED ***
exit /b 1
