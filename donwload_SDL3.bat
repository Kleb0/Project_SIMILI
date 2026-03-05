@echo off
setlocal EnableExtensions EnableDelayedExpansion

echo ================================
echo   DOWNLOAD SDL3 (Devel VC)
echo ================================
echo.

set "DOWNLOAD_DIR=C:\SDL3"

echo Fetching latest SDL3 version from GitHub API...
echo.

REM Create temporary PowerShell script
set "PS_SCRIPT=%TEMP%\get_sdl3.ps1"
(
echo try {
echo     $ProgressPreference = 'SilentlyContinue'
echo     [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
echo     $response = Invoke-RestMethod -Uri 'https://api.github.com/repos/libsdl-org/SDL/releases/latest' -Headers @{'Accept'='application/vnd.github.v3+json'} -ErrorAction Stop
echo     $asset = $response.assets ^| Where-Object { $_.name -match '^SDL3-devel-.+-VC\.zip$' } ^| Select-Object -First 1
echo     if ($asset^) {
echo         Write-Output $asset.name
echo         Write-Output $asset.browser_download_url
echo     } else {
echo         throw 'No SDL3-devel-VC.zip found in latest release'
echo     }
echo } catch {
echo     Write-Host "ERROR: $_"
echo     exit 1
echo }
) > "%PS_SCRIPT%"

REM Execute PowerShell script and capture output
set "LINE_NUM=0"
for /f "usebackq delims=" %%i in (`powershell -ExecutionPolicy Bypass -File "%PS_SCRIPT%" 2^>^&1`) do (
    set /a LINE_NUM+=1
    if !LINE_NUM! EQU 1 set "SDL3_ARCHIVE=%%i"
    if !LINE_NUM! EQU 2 set "SDL3_URL=%%i"
)

REM Cleanup temp script
del /F /Q "%PS_SCRIPT%" 2>nul

if "%SDL3_ARCHIVE%"=="" (
    echo ERROR: Could not detect SDL3-devel-VC.zip from GitHub releases
    echo Please check your internet connection or visit:
    echo https://github.com/libsdl-org/SDL/releases
    pause
    exit /b 1
)

if "%SDL3_URL%"=="" (
    echo ERROR: Could not get download URL for SDL3
    pause
    exit /b 1
)

set "ZIP_FILE=%DOWNLOAD_DIR%\%SDL3_ARCHIVE%"

REM Extract folder name from archive name (remove .zip extension)
set "EXTRACT_FOLDER=%SDL3_ARCHIVE:~0,-4%"
set "EXTRACT_DIR=%DOWNLOAD_DIR%\%EXTRACT_FOLDER%"

REM Extract version from archive name (SDL3-devel-3.4.2-VC.zip -> 3.4.2)
for /f "tokens=3 delims=-" %%v in ("%SDL3_ARCHIVE%") do set "SDL3_VERSION=%%v"

echo Detected SDL3 Archive: %SDL3_ARCHIVE%
echo SDL3 Version: %SDL3_VERSION%
echo Download URL: %SDL3_URL%
echo Installation Directory: %DOWNLOAD_DIR%
echo.

REM Check if base download directory exists
if not exist "%DOWNLOAD_DIR%" (
    echo WARNING: SDL3 installation directory does not exist: %DOWNLOAD_DIR%
    echo.
    choice /C YN /M "Do you want to create this directory"
    if errorlevel 2 (
        echo Installation cancelled by user.
        pause
        exit /b 0
    )
    echo Creating directory: %DOWNLOAD_DIR%
    mkdir "%DOWNLOAD_DIR%"
    if %errorlevel% neq 0 (
        echo ERROR: Failed to create directory %DOWNLOAD_DIR%
        echo Please check your permissions.
        pause
        exit /b 1
    )
    echo   + Directory created successfully
    echo.
)

REM Check if already downloaded
if exist "%EXTRACT_DIR%" (
    echo SDL3 is already downloaded and extracted at: %EXTRACT_DIR%
    echo.
    choice /C YN /M "Do you want to re-download and overwrite"
    if errorlevel 2 (
        echo Installation cancelled.
        exit /b 0
    )
    echo Cleaning up existing installation...
    rmdir /S /Q "%EXTRACT_DIR%"
)

echo.
echo [1/3] Downloading SDL3-devel-%SDL3_VERSION%-VC.zip...
echo This may take a few minutes...

powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ProgressPreference = 'SilentlyContinue'; try { Invoke-WebRequest -Uri '%SDL3_URL%' -OutFile '%ZIP_FILE%' -ErrorAction Stop; Write-Host 'Download completed successfully' } catch { Write-Host 'ERROR: Download failed -' $_.Exception.Message; exit 1 } }"

if %errorlevel% neq 0 (
    echo ERROR: Failed to download SDL3
    exit /b 1
)

if not exist "%ZIP_FILE%" (
    echo ERROR: Downloaded file not found
    exit /b 1
)

echo   + Download complete: %ZIP_FILE%

echo.
echo [2/3] Extracting SDL3...

REM Extract to temporary location first
set "TEMP_EXTRACT=%DOWNLOAD_DIR%\temp_extract"
if exist "%TEMP_EXTRACT%" rmdir /S /Q "%TEMP_EXTRACT%"
mkdir "%TEMP_EXTRACT%"

powershell -Command "& { $ProgressPreference = 'SilentlyContinue'; try { Expand-Archive -Path '%ZIP_FILE%' -DestinationPath '%TEMP_EXTRACT%' -Force -ErrorAction Stop; Write-Host 'Extraction completed successfully' } catch { Write-Host 'ERROR: Extraction failed -' $_.Exception.Message; exit 1 } }"

if %errorlevel% neq 0 (
    echo ERROR: Failed to extract SDL3
    rmdir /S /Q "%TEMP_EXTRACT%" 2>nul
    exit /b 1
)

REM Find the extracted folder and rename it to our desired format
set "FOUND_DIR="
for /d %%d in ("%TEMP_EXTRACT%\*") do (
    set "FOUND_DIR=%%d"
    goto :found
)
:found

if "%FOUND_DIR%"=="" (
    echo ERROR: No folder found in extracted archive
    rmdir /S /Q "%TEMP_EXTRACT%" 2>nul
    exit /b 1
)

REM Remove existing installation if present
if exist "%EXTRACT_DIR%" (
    echo Removing old installation...
    rmdir /S /Q "%EXTRACT_DIR%"
)

REM Move and rename to desired format: SDL3-devel-[version]-VC
move "%FOUND_DIR%" "%EXTRACT_DIR%" >nul

REM Cleanup temp directory
rmdir /S /Q "%TEMP_EXTRACT%" 2>nul

echo   + Extraction complete: %EXTRACT_DIR%

echo.
echo [3/3] Cleaning up...
del /F /Q "%ZIP_FILE%"
echo   + Temporary files removed

echo.
echo ========================================
echo    SDL3 INSTALLATION COMPLETE!
echo ========================================
echo.
echo Installation path: %EXTRACT_DIR%
echo.
echo Directory structure:
echo   %EXTRACT_DIR%\
echo   ├── include\
echo   │   └── SDL3\
echo   └── lib\
echo       ├── x86\   (32-bit libraries)
echo       ├── x64\   (64-bit libraries)
echo       └── arm64\ (ARM64 libraries)
echo.
echo Next steps:
echo   1. Update CMakeLists.txt with the new SDL3 paths
echo   2. Update c_cpp_properties.json for IntelliSense
echo   3. Update build_simili.bat to copy SDL3.dll
echo.
echo Use the following paths in your configuration:
echo   SDL3_INCLUDE_DIR: %EXTRACT_DIR%\include
echo   SDL3_LIBRARY: %EXTRACT_DIR%\lib\x64\SDL3.lib
echo   SDL3_DLL: %EXTRACT_DIR%\lib\x64\SDL3.dll
echo.
pause
