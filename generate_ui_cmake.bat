@echo off
echo ========================================
echo    Regenerating SIMILI_UI CMake
echo ========================================
echo.

cd /d "%~dp0src\SIMILI_Frontend"
if %errorlevel% neq 0 (
    echo ERROR: Folder src\SIMILI_Frontend not found
    exit /b 1
)

echo Cleaning old build files...
if exist build (
    rmdir /s /q build
)
mkdir build

echo.
echo Generating CMake with vcpkg toolchain...
cmake -S . -B build ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static

if %errorlevel% neq 0 (
    echo.
    echo ERROR: CMake generation failed!
    exit /b 1
)

echo.
echo ========================================
echo  CMake generated successfully!
echo ========================================
echo.

cd /d "%~dp0"
pause
