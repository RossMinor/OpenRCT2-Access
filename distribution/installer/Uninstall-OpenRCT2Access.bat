@echo off
rem Removes the accessibility mod and puts the original openrct2.exe back.
rem
rem This works from the download folder, but it also works on its own: the uninstall only needs the
rem backup that the install left beside the game, so this file can be kept anywhere.

setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0OpenRCT2Access-Installer.ps1" -Uninstall
endlocal
