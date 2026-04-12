@echo off
setlocal EnableExtensions

echo ========================================
echo  SIMILI - Recreate Build Folder
echo ========================================
echo.

cd /d "%~dp0"

REM Auto-detect Visual Studio installation
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Please install Visual Studio 2017 or later.
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: Visual Studio installation not found
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationVersion`) do set "VS_VERSION=%%i"
for /f "tokens=1 delims=." %%a in ("%VS_VERSION%") do set "VS_MAJOR=%%a"

echo Detected Visual Studio %VS_VERSION% at: %VS_PATH%

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
echo === Visual Studio Environment initialized ===
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
echo Using CMake generator: Visual Studio %VS_MAJOR%
cmake .. -G "Visual Studio %VS_MAJOR%" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static

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
