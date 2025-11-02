@echo off
setlocal enabledelayedexpansion
echo ============================================
echo Installing vcpkg Package Manager
echo ============================================

set "VCPKG_DIR=C:\vcpkg"

if exist "!VCPKG_DIR!" (
    echo vcpkg already exists at !VCPKG_DIR!
    echo Updating vcpkg...
    cd /d "!VCPKG_DIR!"
    git pull
    goto :bootstrap
)

echo Cloning vcpkg from GitHub...
git clone https://github.com/Microsoft/vcpkg.git "!VCPKG_DIR!"

if !errorlevel! neq 0 (
    echo ERROR: Failed to clone vcpkg
    echo Make sure git is installed and in your PATH
    pause
    exit /b 1
)

:bootstrap
echo.
echo Running vcpkg bootstrap...
cd /d "!VCPKG_DIR!"
call bootstrap-vcpkg.bat

if !errorlevel! neq 0 (
    echo ERROR: Failed to bootstrap vcpkg
    pause
    exit /b 1
)

echo.
echo ============================================
echo vcpkg installed successfully!
echo ============================================
echo.
echo Location: !VCPKG_DIR!
echo.
echo Adding vcpkg to PATH for this session...
set "PATH=!VCPKG_DIR!;!PATH!"
echo.
echo To add vcpkg permanently to your PATH:
echo 1. Open System Properties ^> Environment Variables
echo 2. Add C:\vcpkg to your PATH variable
echo.
echo Or run this command as Administrator:
echo setx /M PATH "%%PATH%%;C:\vcpkg"
echo.
pause
endlocal
