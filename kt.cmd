@echo off
setlocal
if not exist "%~dp0.venv\Scripts\python.exe" (
  echo Run setup.cmd first to install the project-local tools.
  exit /b 1
)
"%~dp0.venv\Scripts\python.exe" "%~dp0tools\kt.py" %*
