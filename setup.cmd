@echo off
setlocal
if exist "%~dp0support\downloads\" goto offline
if exist "%~dp0support\wheels\" goto offline
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup-source.ps1" %*
exit /b %errorlevel%
:offline
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup-windows.ps1" %*
exit /b %errorlevel%
