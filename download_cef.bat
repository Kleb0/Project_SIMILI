@echo off
setlocal EnableExtensions

echo ========================================
echo   CEF Download and Extraction
echo ========================================
echo.

REM CEF version to download
set CEF_VERSION=141.0.11+g7e73ac4+chromium-141.0.7390.123
set CEF_ARCHIVE=cef_binary_%CEF_VERSION%_windows64.tar.bz2
set CEF_URL=https://cef-builds.spotifycdn.com/%CEF_ARCHIVE%
set CEF_EXTRACT_DIR=%~dp0src\ThirdParty\CEF\cef_binary

REM Check if CEF already exists
if exist "%CEF_EXTRACT_DIR%\include" (
    echo CEF is already installed in %CEF_EXTRACT_DIR%
    choice /C YN /M "Do you want to re-download and reinstall"
    if errorlevel 2 goto :end
    echo Removing existing CEF...
    rmdir /s /q "%CEF_EXTRACT_DIR%"
)

REM Create temp directory
set TEMP_DIR=%~dp0temp_cef_download
if not exist "%TEMP_DIR%" mkdir "%TEMP_DIR%"

echo.
echo [1/4] Downloading CEF from Spotify CDN...
echo URL: %CEF_URL%
echo.

REM Download using curl (available on Windows 10+)
curl -L -o "%TEMP_DIR%\%CEF_ARCHIVE%" "%CEF_URL%"
if %errorlevel% neq 0 (
    echo ERROR: Failed to download CEF
    echo.
    echo Alternative: Download manually from:
    echo https://cef-builds.spotifycdn.com/index.html
    goto :cleanup
)

echo   + Downloaded successfully
echo   Downloaded to: %TEMP_DIR%\%CEF_ARCHIVE%

REM Check if 7-Zip is installed
set SEVENZIP_PATH=
if exist "C:\Program Files\7-Zip\7z.exe" (
    set "SEVENZIP_PATH=C:\Program Files\7-Zip\7z.exe"
) else if exist "C:\Program Files (x86)\7-Zip\7z.exe" (
    set "SEVENZIP_PATH=C:\Program Files (x86)\7-Zip\7z.exe"
) else (
    echo.
    echo ERROR: 7-Zip not found!
    echo.
    echo Downloaded file is located at:
    echo   %TEMP_DIR%\%CEF_ARCHIVE%
    echo.
    echo Please install 7-Zip from: https://www.7-zip.org/download.html
    echo After installation, run this script again.
    goto :cleanup
)

echo.
echo [2/4] Extracting .tar.bz2 archive (Step 1: decompress .bz2)...
"%SEVENZIP_PATH%" x "%TEMP_DIR%\%CEF_ARCHIVE%" -o"%TEMP_DIR%" -y
if %errorlevel% neq 0 (
    echo ERROR: Failed to decompress .bz2
    goto :cleanup
)

echo   + Decompressed .bz2

echo.
echo [3/4] Extracting .tar archive (Step 2: extract .tar)...
set TAR_FILE=%TEMP_DIR%\cef_binary_%CEF_VERSION%_windows64.tar
"%SEVENZIP_PATH%" x "%TAR_FILE%" -o"%TEMP_DIR%" -y
if %errorlevel% neq 0 (
    echo ERROR: Failed to extract .tar
    goto :cleanup
)

echo   + Extracted .tar

echo.
echo [4/4] Moving CEF to destination folder...
if not exist "src\ThirdParty\CEF" mkdir "src\ThirdParty\CEF"

REM Move the extracted folder
set EXTRACTED_FOLDER=%TEMP_DIR%\cef_binary_%CEF_VERSION%_windows64
if not exist "%EXTRACTED_FOLDER%" (
    echo ERROR: Extracted folder not found at %EXTRACTED_FOLDER%
    goto :cleanup
)

REM Delete destination if it already exists
if exist "%CEF_EXTRACT_DIR%" (
    echo   Removing existing CEF installation...
    rmdir /s /q "%CEF_EXTRACT_DIR%"
)

REM Use robocopy to copy files (more reliable than move)
robocopy "%EXTRACTED_FOLDER%" "%CEF_EXTRACT_DIR%" /E /MOVE /NFL /NDL /NJH /NJS
if %errorlevel% geq 8 (
    echo ERROR: Failed to move CEF to destination
    goto :cleanup
)

echo   + CEF installed successfully!

echo.
echo ========================================
echo   CEF Installation Complete!
echo ========================================
echo.
echo Location: %CEF_EXTRACT_DIR%
echo Version: %CEF_VERSION%
echo.
echo You can now build SIMILI_UI with CEF support.
echo.

:cleanup
echo.
echo Cleaning up temporary files...
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"

:end
pause
