@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\apply_buildfix4.ps1"
exit /b %ERRORLEVEL%
