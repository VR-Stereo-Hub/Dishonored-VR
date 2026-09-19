@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0collect-support.ps1" -GameDir "%~dp0."
if errorlevel 1 pause
