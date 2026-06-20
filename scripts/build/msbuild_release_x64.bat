@echo off
setlocal

set "VS_DEV_CMD=D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
for %%I in ("%~dp0..\..") do set "REPO_ROOT=%%~fI"

if "%~1"=="" (
    echo Usage: %~nx0 project-or-solution [extra-msbuild-args]
    exit /b 2
)

if not exist "%VS_DEV_CMD%" (
    echo VS DevCmd not found: %VS_DEV_CMD%
    exit /b 2
)

call "%VS_DEV_CMD%"
if errorlevel 1 exit /b %ERRORLEVEL%

set "PROJECT_PATH=%REPO_ROOT%\%~1"
if not exist "%PROJECT_PATH%" (
    echo Project not found: %PROJECT_PATH%
    exit /b 2
)

msbuild "%PROJECT_PATH%" /p:Configuration=Release /p:Platform=x64 %~2
exit /b %ERRORLEVEL%
