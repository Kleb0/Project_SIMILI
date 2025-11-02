@echo off
setlocal enabledelayedexpansion
echo ============================================
echo Installing Boost via vcpkg
echo ============================================

REM Check if vcpkg is installed
where vcpkg >nul 2>&1
if !errorlevel! neq 0 (
    echo vcpkg not found in PATH, checking C:\vcpkg...
    
    if exist "C:\vcpkg\vcpkg.exe" (
        echo Found vcpkg at C:\vcpkg
        set "PATH=C:\vcpkg;!PATH!"
        set "VCPKG_EXE=C:\vcpkg\vcpkg.exe"
    ) else (
        echo ERROR: vcpkg not found
        echo.
        echo Would you like to install vcpkg now? (Y/N)
        choice /C YN /N /M "Press Y for Yes, N for No: "
        if !errorlevel! equ 2 goto :no_install
        if !errorlevel! equ 1 goto :install_vcpkg
        
        :install_vcpkg
        echo.
        echo Running install_vcpkg.bat...
        call "%~dp0install_vcpkg.bat"
        if !errorlevel! neq 0 (
            echo ERROR: Failed to install vcpkg
            pause
            exit /b 1
        )
        set "PATH=C:\vcpkg;!PATH!"
        set "VCPKG_EXE=C:\vcpkg\vcpkg.exe"
        goto :continue
        
        :no_install
        echo.
        echo Installation cancelled.
        echo Please run install_vcpkg.bat manually first.
        pause
        exit /b 1
    )
) else (
    set "VCPKG_EXE=vcpkg"
)

:continue

:continue

echo.
echo Installing Boost.Beast (includes Boost.Asio) and OpenSSL...
echo Installing with STATIC linking for standalone distribution...
"!VCPKG_EXE!" install boost-beast:x64-windows-static
"!VCPKG_EXE!" install boost-asio:x64-windows-static
"!VCPKG_EXE!" install openssl:x64-windows-static

if !errorlevel! neq 0 (
    echo ERROR: Failed to install Boost
    pause
    exit /b 1
)

echo.
echo ============================================
echo Boost installed successfully!
echo ============================================
echo.
echo vcpkg location: C:\vcpkg
echo.
echo Next steps:
echo 1. Regenerate CMake with vcpkg toolchain:
echo    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
echo      -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
echo.
echo 2. Or update your CMakePresets.json to include vcpkg toolchain
echo.
pause
endlocal
