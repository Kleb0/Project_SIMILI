@echo off
echo ========================================
echo  SIMILI - Recreate Build Folder
echo ========================================
echo.

cd /d "%~dp0"

echo [1/4] Cleaning existing build folder...
if exist build (
    rmdir /s /q build
    echo Build folder deleted.
) else (
    echo No existing build folder found.
)

echo.
echo [2/4] Creating new build folder...
mkdir build
cd build

echo.
echo [3/4] Running CMake configuration with vcpkg toolchain...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [4/4] Building project in Release mode...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build folder recreated successfully!
echo ========================================
echo.
pause
