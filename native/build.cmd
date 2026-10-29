
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

cl /nologo /std:c++20 /O2 /W4 /EHsc /DUNICODE /D_UNICODE "%~dp0main.cpp" /Fe:GameOverlayDev.exe /link /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" user32.lib gdi32.lib

set "RESULT=%ERRORLEVEL%"
popd

if not "%RESULT%"=="0" exit /b %RESULT%
echo Native build completed successfully.
