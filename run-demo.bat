@echo off
rem Double-click this file to run the RadSlice demo (about 20 seconds). Needs MSYS2 in C:\msys64.
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d "%~dp0"
python scripts\demo.py
echo.
echo Done. Press any key to close this window.
pause >nul
