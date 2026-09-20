@echo off
rem Double-clickable wrapper for vr-runtime.ps1, matching Collect VR Support.cmd.
rem
rem WHY THIS EXISTS. The .ps1 works, but only when it is launched the one right
rem way. Double-clicking a .ps1 opens it in an editor instead of running it;
rem "Run with PowerShell" runs it under the machine's ExecutionPolicy and closes
rem the window the instant it is refused; and typing the bare name at a prompt
rem fails because PowerShell does not search the current directory. All three
rem look identical from the outside: nothing happens and the ini is unchanged.
rem Measured 2026-09-20 - the selection was still Runtime=steamvr and the ini's
rem timestamp showed the script had never run at all. collect-support.ps1 shipped
rem with a wrapper and vr-runtime.ps1 did not; that was the whole difference.
rem
rem A double-click cannot pass an argument, so this asks.
setlocal
cd /d "%~dp0"
echo.
echo   Which OpenXR runtime should the mod use?
echo.
echo     1  Virtual Desktop (VDXR)   - pinned by path
echo     2  SteamVR via the shim     - start SteamVR before launching
echo     3  Follow the system        - whatever is registered as the default
echo     4  Just show what is set now
echo.
set "MODE="
set /p "PICK=  Choose 1-4 (or press Enter to just show): "
if "%PICK%"=="1" set "MODE=vdxr"
if "%PICK%"=="2" set "MODE=shim"
if "%PICK%"=="3" set "MODE=auto"
if "%PICK%"=="4" set "MODE=show"
if "%MODE%"=="" set "MODE=show"
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr-runtime.ps1" -Mode %MODE% -GameDir "%~dp0."
echo.
echo   A runtime change takes effect at the NEXT launch.
echo.
pause
endlocal
