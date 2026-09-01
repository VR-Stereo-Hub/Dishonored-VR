@echo off
rem setup_resolution.bat - kept for users of the original release; the real
rem work is in setup-game-ini.ps1 next to this file.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-game-ini.ps1" -Resolution
pause
