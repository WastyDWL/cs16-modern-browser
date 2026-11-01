@echo off
rem Usage: tools\build.cmd [target]   (Release, Win32, NMake; works with an incomplete Build Tools install)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0.."
if not exist build\CMakeCache.txt cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
if "%~1"=="" (cmake --build build) else (cmake --build build --target %*)
