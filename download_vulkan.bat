@echo off
setlocal enabledelayedexpansion

echo ========================================
echo    Vulkan SDK Installation Script
echo ========================================
echo.

set /p CONFIRM="Are you using Windows? (y/n): "
if /i not "%CONFIRM%"=="y" (
    echo Installation cancelled.
    pause
    exit /b 1
)

echo.
echo Detecting system architecture...
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCH=x64
) else if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    set ARCH=x86
) else (
    set ARCH=x64
)
echo Architecture detected: %ARCH%

set VULKAN_VERSION=1.3.290.0
set VULKAN_INSTALLER=VulkanSDK-%VULKAN_VERSION%-Installer.exe
set VULKAN_URL=https://sdk.lunarg.com/sdk/download/%VULKAN_VERSION%/windows/%VULKAN_INSTALLER%
set DOWNLOAD_DIR=%TEMP%\vulkan_download
set INSTALL_DIR=C:\VulkanSDK

echo.
echo Configuration:
echo - Version: %VULKAN_VERSION%
echo - URL: %VULKAN_URL%
echo - Install directory: %INSTALL_DIR%
echo.

if not exist "%DOWNLOAD_DIR%" mkdir "%DOWNLOAD_DIR%"

echo Downloading Vulkan SDK...
echo This may take several minutes...
echo.

powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%VULKAN_URL%' -OutFile '%DOWNLOAD_DIR%\%VULKAN_INSTALLER%' -UseBasicParsing }"

if not exist "%DOWNLOAD_DIR%\%VULKAN_INSTALLER%" (
    echo.
    echo ERROR: Download failed.
    echo Please download manually from: https://vulkan.lunarg.com/sdk/home
    pause
    exit /b 1
)

echo.
echo Download complete!
echo.
echo Launching installation...
echo IMPORTANT: Install to the folder %INSTALL_DIR%
echo.

start /wait "" "%DOWNLOAD_DIR%\%VULKAN_INSTALLER%"

if exist "%INSTALL_DIR%" (
    echo.
    echo ========================================
    echo Installation completed successfully!
    echo ========================================
    echo.
    echo Vulkan SDK installed in: %INSTALL_DIR%
    echo.
    echo Environment variables configured:
    echo - VULKAN_SDK=%INSTALL_DIR%\%VULKAN_VERSION%
    echo - PATH includes Vulkan binaries
    echo.
    echo You can now compile the SIMILI project.
) else (
    echo.
    echo WARNING: Installation directory was not detected.
    echo Please verify that the installation completed successfully.
)

echo.
echo Cleaning up...
if exist "%DOWNLOAD_DIR%" rd /s /q "%DOWNLOAD_DIR%"

echo.
pause
