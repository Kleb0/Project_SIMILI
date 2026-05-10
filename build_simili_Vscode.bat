@echo off
setlocal EnableExtensions

echo =============================================
echo   BUILD SIMILI FOR VSCODE - CEF INTEGRATION 
echo =============================================
echo.

REM Auto-detect Visual Studio installation
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Please install Visual Studio 2017 or later.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: Visual Studio installation not found
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationVersion`) do set "VS_VERSION=%%i"
for /f "tokens=1 delims=." %%a in ("%VS_VERSION%") do set "VS_MAJOR=%%a"

echo Detected Visual Studio %VS_VERSION% at: %VS_PATH%

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
echo === Visual Studio Environment initialized ===

for /f %%A in ('powershell -NoProfile -Command "(Get-Date).ToString('o')"') do set "START_ISO=%%A"

echo.
echo [1/3] Building SIMILI (with CEF integration)...

if not exist build mkdir build
cd build

if exist "CMakeCache.txt" (
    echo Removing old CMake cache...
    del /F /Q "CMakeCache.txt"
)

if not exist "CMakeCache.txt" (
    echo Generating CMake configuration...
    echo Using CMake generator: Visual Studio %VS_MAJOR%
    cmake -G "Visual Studio %VS_MAJOR%" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static ..
    if %errorlevel% neq 0 (
        echo ERROR: CMake generation failed
        exit /b 1
    )
)

cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo ERROR: SIMILI compilation failed
    exit /b 1
)

REM Auto-detect SDL3 installation for DLL deployment
set "SDL3_BASE=C:\SDL3"
set "SDL3_DLL_PATH="

for /d %%d in ("%SDL3_BASE%\SDL3-devel-*-VC") do (
    if exist "%%d\lib\x64\SDL3.dll" (
        set "SDL3_DLL_PATH=%%d\lib\x64\SDL3.dll"
        goto :sdl3_found
    )
)

:sdl3_found
if "%SDL3_DLL_PATH%"=="" (
    echo WARNING: SDL3.dll not found in %SDL3_BASE%
    echo Expected pattern: SDL3-devel-[version]-VC\lib\x64\SDL3.dll
    echo Please run the 'Download SDL3' task to install SDL3.
) else (
    if not exist "Release\SDL3.dll" (
        echo Copying SDL3.dll from detected installation...
        copy "%SDL3_DLL_PATH%" "Release\SDL3.dll" >nul
        if %errorlevel% neq 0 (
            echo WARNING: Failed to copy SDL3.dll
        ) else (
            echo   + SDL3.dll deployed
        )
    )
)

if not exist "src\resources" mkdir src\resources
if not exist "src\resources\default_imgui_layout.ini" copy "..\src\resources\default_imgui_layout.ini" "src\resources\default_imgui_layout.ini" >nul

echo   + SIMILI compiled

cd ..

echo.
echo [2/3] Deploying CEF dependencies...

if not exist "build\Release" mkdir "build\Release"

if not exist "src\ThirdParty\CEF\cef_binary\" (
    echo ERROR: CEF binary directory not found: src\ThirdParty\CEF\cef_binary\
    echo Please download CEF binaries first using download_cef.bat
    exit /b 1
)

if not exist "src\ThirdParty\CEF\cef_binary\Release\libcef.dll" (
    echo ERROR: CEF binaries not found in src\ThirdParty\CEF\cef_binary\Release\
    echo Please download CEF binaries first using download_cef.bat
    exit /b 1
)

echo Copying CEF DLLs...
copy /Y "src\ThirdParty\CEF\cef_binary\Release\*.dll" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Failed to copy CEF DLLs
    exit /b 1
)

copy /Y "src\ThirdParty\CEF\cef_binary\Release\*.bin" "build\Release\" >nul 2>nul

echo Copying CEF resources...
copy /Y "src\ThirdParty\CEF\cef_binary\Resources\*.pak" "build\Release\" >nul 2>nul
copy /Y "src\ThirdParty\CEF\cef_binary\Resources\*.dat" "build\Release\" >nul 2>nul

if not exist "build\Release\locales" mkdir "build\Release\locales"
xcopy /Y /Q "src\ThirdParty\CEF\cef_binary\Resources\locales\*" "build\Release\locales\" >nul 2>nul

echo   + CEF dependencies deployed

echo.
echo [3/3] Copying UI HTML files...

if exist "build\Release\ui" (
    echo Removing old UI folder...
    rmdir /S /Q "build\Release\ui"
)

echo Copying fresh UI files from project root...
xcopy /E /I /Y /Q "ui" "build\Release\ui" >nul 2>nul
if %errorlevel% neq 0 (
    echo WARNING: Failed to copy UI files
) else (
    echo   + UI files deployed
)

for /f %%A in ('powershell -NoProfile -Command "(Get-Date).ToString('o')"') do set "END_ISO=%%A"

for /f "usebackq" %%A in (`powershell -NoProfile -Command ^
  "$s=[datetime]::ParseExact('%START_ISO%','o',$null); $e=[datetime]::ParseExact('%END_ISO%','o',$null); (New-TimeSpan -Start $s -End $e).TotalSeconds.ToString('0.00')"`) do set "DURATION=%%A"

for /f "usebackq tokens=*" %%A in (`powershell -NoProfile -Command "(Get-Date).ToString('yyyy-MM-dd HH:mm:ss')"`) do set "BUILD_DATETIME=%%A"

echo.
echo ========================================
echo    BUILD COMPLETE! 
echo    Date/Time:  %BUILD_DATETIME%
echo    Total time: %DURATION%s
echo ========================================
echo.
echo Executable:
echo   - build\Release\Project_SIMILI.exe (with CEF integrated)
echo.
