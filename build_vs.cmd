@echo off
call "I:\MSVS18\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 9
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_C_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" "-DCMAKE_CXX_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe" > configure.log 2>&1 || exit /b 1
cmake --build build > ninja.log 2>&1
