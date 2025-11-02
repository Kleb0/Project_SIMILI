@echo off
setlocal enabledelayedexpansion
echo ============================================
echo Installing OpenSSL via vcpkg
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
        echo Please run install_vcpkg.bat first
        pause
        exit /b 1
    )
) else (
    set "VCPKG_EXE=vcpkg"
)

echo.
echo Installing OpenSSL (static)...
"!VCPKG_EXE!" install openssl:x64-windows-static

if !errorlevel! neq 0 (
    echo ERROR: Failed to install OpenSSL
    pause
    exit /b 1
)

echo.
echo ============================================
echo OpenSSL installed successfully!
echo ============================================
pause
endlocal
