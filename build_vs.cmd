@echo off
rem Rebuild the checkout or worktree this file is in, from a VS x64 developer environment (Ninja + clang-cl); LLVM on
rem PATH for the shader preprocess step. Visual Studio is found with vswhere, LLVM in its default install folder
rem (set LLVM_DIR to another).
setlocal EnableDelayedExpansion
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -prerelease -products * -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio not found & exit /b 9)
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 9
if not defined LLVM_DIR set "LLVM_DIR=C:\Program Files\LLVM"
set "PATH=%LLVM_DIR%\bin;%PATH%"
cd /d "%~dp0"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_C_COMPILER=%LLVM_DIR:\=/%/bin/clang-cl.exe" "-DCMAKE_CXX_COMPILER=%LLVM_DIR:\=/%/bin/clang-cl.exe" > configure.log 2>&1 || exit /b 1
cmake --build build > ninja.log 2>&1
