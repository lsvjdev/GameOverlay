
@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo Visual Studio C++ build tools were not found.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"

if not defined VSROOT (
    echo Microsoft C++ compiler is not installed.
    exit /b 1
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set "OUT=%~dp0..\build-native"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"

rem Embed build information for support reports and the About page.
set "GAMEOVERLAY_BUILD_VERSION=local dev"
if defined APPVEYOR_BUILD_VERSION set "GAMEOVERLAY_BUILD_VERSION=%APPVEYOR_BUILD_VERSION%"
set "GAMEOVERLAY_BUILD_REVISION=local"
if defined APPVEYOR_REPO_COMMIT set "GAMEOVERLAY_BUILD_REVISION=%APPVEYOR_REPO_COMMIT:~0,7%"
if not defined APPVEYOR_REPO_COMMIT (
    for /f "tokens=*" %%i in ('git -C "%~dp0.." rev-parse --short=7 HEAD 2^>nul') do set "GAMEOVERLAY_BUILD_REVISION=%%i"
)
> "%OUT%\build_info.h" echo #pragma once
>> "%OUT%\build_info.h" echo #define GAMEOVERLAY_VERSION L"%GAMEOVERLAY_BUILD_VERSION%"
>> "%OUT%\build_info.h" echo #define GAMEOVERLAY_REVISION L"%GAMEOVERLAY_BUILD_REVISION%"

cl /nologo /std:c++20 /O2 /W4 /EHsc /I"%OUT%" /DUNICODE /D_UNICODE "%~dp0main.cpp" /Fe:GameOverlayDev.exe /link /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" user32.lib gdi32.lib psapi.lib PowrProf.lib Pdh.lib shell32.lib dwmapi.lib gdiplus.lib

set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
    popd
    exit /b %RESULT%
)

cl /nologo /std:c++20 /O2 /W4 /EHsc "%~dp0tests\frame_window_tests.cpp" /Fe:FrameWindowTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

FrameWindowTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

cl /nologo /std:c++20 /O2 /W4 /EHsc "%~dp0tests\gpu_percentage_tests.cpp" /Fe:GpuPercentageTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

GpuPercentageTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

cl /nologo /std:c++20 /O2 /W4 /EHsc "%~dp0tests\cpu_temperature_tests.cpp" /Fe:CpuTemperatureTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

CpuTemperatureTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

cl /nologo /std:c++20 /O2 /W4 /EHsc "%~dp0tests\metric_order_tests.cpp" /Fe:MetricOrderTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

MetricOrderTests.exe
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
    popd
    exit /b %RESULT%
)

cl /nologo /std:c++20 /O2 /W4 /EHsc "%~dp0tests\metric_detail_tests.cpp" /Fe:MetricDetailTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

MetricDetailTests.exe
set "RESULT=%ERRORLEVEL%"
popd

if not "%RESULT%"=="0" exit /b %RESULT%
echo Native build completed successfully.
