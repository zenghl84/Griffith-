@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0Griffith.exe" (
    echo Griffith.exe is missing. Run the build script first.
    pause
    exit /b 1
)
start "" "%~dp0Griffith.exe" %*
