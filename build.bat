@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "OUT_DIR=%ROOT%bin\x64\Release"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSWHERE_RESULT=%TEMP%\UnrealVTDbg-vswhere-%RANDOM%.txt"
set "VS_INSTALL="

if exist "%VSWHERE%" goto vswhere_found
echo [build] vswhere.exe was not found: %VSWHERE%
exit /b 1

:vswhere_found
"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath > "%VSWHERE_RESULT%"
if errorlevel 1 (
    del /q "%VSWHERE_RESULT%" >nul 2>nul
    exit /b 1
)
set /p VS_INSTALL=<"%VSWHERE_RESULT%"
del /q "%VSWHERE_RESULT%" >nul 2>nul
if not defined VS_INSTALL (
    echo [build] A Visual Studio installation with MSBuild was not found.
    exit /b 1
)

set "VSDEVCMD=%VS_INSTALL%\Common7\Tools\VsDevCmd.bat"
call "%VSDEVCMD%" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b 1

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

if /I "%~1"=="vt" (
    echo [build] Building VT driver only...
    call :build_project "%ROOT%VT_Driver\VT_Driver.vcxproj" VT_Driver x64
    if errorlevel 1 exit /b 1
    echo [build] VT driver artifact: %OUT_DIR%\VT_Driver.sys
    exit /b 0
)

if /I "%~1"=="drivers" (
    echo [build] Building release drivers...
    call :build_project "%ROOT%DbgkSysWin10\DbgkSysWin10.vcxproj" DbgkSysWin10 x64
    if errorlevel 1 exit /b 1
    call :build_project "%ROOT%DbgkSysWin11\DbgkSysWin11.vcxproj" DbgkSysWin11 x64
    if errorlevel 1 exit /b 1
    call :build_project "%ROOT%VT_Driver\VT_Driver.vcxproj" VT_Driver x64
    if errorlevel 1 exit /b 1
    echo [build] Driver artifacts: %OUT_DIR%
    exit /b 0
)

if /I "%~1"=="tests" (
    echo [build] Building EPT policy tests...
    call :build_project "%ROOT%Tests\EptCapabilityPolicyTests\EptCapabilityPolicyTests.vcxproj" EptCapabilityPolicyTests x64
    if errorlevel 1 exit /b 1
    "%OUT_DIR%\EptCapabilityPolicyTests.exe"
    if errorlevel 1 exit /b 1
    exit /b 0
)

if /I "%~1"=="host" (
    echo [build] Building C++ host replacement...
    call :build_host
    if errorlevel 1 exit /b 1
    call :verify_host
    if errorlevel 1 exit /b 1
    echo [build] Host artifact: %OUT_DIR%\UnrealDbgHostCli.exe
    exit /b 0
)

if /I "%~1"=="launcher" (
    echo [build] Building C++ host and launcher...
    call :build_host
    if errorlevel 1 exit /b 1
    call :verify_host
    if errorlevel 1 exit /b 1
    call :build_project "%ROOT%Loader\Loader.vcxproj" Loader x64
    if errorlevel 1 exit /b 1
    echo [build] Launcher artifact: %OUT_DIR%\虚幻调试器.exe
    exit /b 0
)

if /I "%~1"=="hooks" (
    echo [build] Building x64 and Win32 hook bridges...
    call :build_project "%ROOT%Hook\Hook.vcxproj" Hook64 x64
    if errorlevel 1 exit /b 1
    call :build_project "%ROOT%Hook\Hook.vcxproj" Hook Win32
    if errorlevel 1 exit /b 1
    echo [build] Hook artifacts: %OUT_DIR%\Hook64.dll and %OUT_DIR%\Hook.dll
    exit /b 0
)

if /I "%~1"=="dll" (
    echo [build] Building UnrealDbgDll...
    call :build_project "%ROOT%UnrealDbgDll\UnrealDbgDll.vcxproj" UnrealDbgDll x64
    if errorlevel 1 exit /b 1
    copy /Y "%ROOT%UnrealDbgDll\VMProtectSDK64.dll" "%OUT_DIR%\VMProtectSDK64.dll" >nul
    if errorlevel 1 exit /b 1
    echo [build] DLL artifact: %OUT_DIR%\UnrealDbgDll.dll
    exit /b 0
)

echo [build] Building x64 release projects...
call :build_host
if errorlevel 1 exit /b 1
call :verify_host
if errorlevel 1 exit /b 1
call :build_project "%ROOT%Loader\Loader.vcxproj" Loader x64
if errorlevel 1 exit /b 1
call :build_project "%ROOT%Hook\Hook.vcxproj" Hook64 x64
if errorlevel 1 exit /b 1
call :build_project "%ROOT%DbgkSysWin10\DbgkSysWin10.vcxproj" DbgkSysWin10 x64
if errorlevel 1 exit /b 1
call :build_project "%ROOT%DbgkSysWin11\DbgkSysWin11.vcxproj" DbgkSysWin11 x64
if errorlevel 1 exit /b 1
call :build_project "%ROOT%VT_Driver\VT_Driver.vcxproj" VT_Driver x64
if errorlevel 1 exit /b 1
call :build_project "%ROOT%UnrealDbgDll\UnrealDbgDll.vcxproj" UnrealDbgDll x64
if errorlevel 1 exit /b 1

echo [build] Building Win32 hook bridge...
call :build_project "%ROOT%Hook\Hook.vcxproj" Hook Win32
if errorlevel 1 exit /b 1

call :build_project "%ROOT%Tests\EptCapabilityPolicyTests\EptCapabilityPolicyTests.vcxproj" EptCapabilityPolicyTests x64
if errorlevel 1 exit /b 1
"%OUT_DIR%\EptCapabilityPolicyTests.exe"
if errorlevel 1 exit /b 1

copy /Y "%ROOT%UnrealDbgDll\VMProtectSDK64.dll" "%OUT_DIR%\VMProtectSDK64.dll" >nul
if errorlevel 1 exit /b 1

echo [build] Release artifacts: %OUT_DIR%
exit /b 0

:build_project
set "PROJECT_PATH=%~1"
set "PROJECT_NAME=%~2"
set "PROJECT_PLATFORM=%~3"
set "PROJECT_INT_DIR=%ROOT%obj\%PROJECT_NAME%\%PROJECT_PLATFORM%\Release"
msbuild "%PROJECT_PATH%" /m /t:Build /p:Configuration=Release /p:Platform=%PROJECT_PLATFORM% /p:SignMode=Off /p:VcpkgEnabled=false "/p:OutDir=%OUT_DIR%\\" "/p:IntDir=%PROJECT_INT_DIR%\\"
if errorlevel 1 exit /b 1
exit /b 0

:build_host
msbuild "%ROOT%UnrealDbgHostCli\UnrealDbgHostCli.vcxproj" /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnabled=false
if errorlevel 1 exit /b 1
exit /b 0

:verify_host
dumpbin /nologo /exports "%OUT_DIR%\UnrealDbgHostCli.exe" | findstr /r /c:"PrintLog" >nul
if errorlevel 1 (
    echo [build] UnrealDbgHostCli.exe does not export PrintLog.
    exit /b 1
)
exit /b 0
