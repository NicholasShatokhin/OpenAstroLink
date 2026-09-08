@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0repair_windows_vendor_runtime.ps1" %*
exit /b %ERRORLEVEL%
