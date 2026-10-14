@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Test
if errorlevel 1 (
    echo Build or validation failed. See the messages above.
    pause
    exit /b 1
)
pause
