@echo off
rem Double-click launcher for the OpenRCT2-Access installer.
rem
rem PowerShell refuses to run downloaded .ps1 files under the default execution policy, so the
rem script is invoked explicitly with -ExecutionPolicy Bypass. -NoProfile keeps a user's own
rem PowerShell profile from printing anything into the installer's output, which matters when the
rem output is being read aloud.
rem
rem To uninstall, run Uninstall-OpenRCT2Access.bat instead.

setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0OpenRCT2Access-Installer.ps1"
endlocal
