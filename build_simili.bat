@echo off
setlocal EnableExtensions

echo ========================================
echo   FAST BUILD (UI + SIMILI)
echo ========================================
echo.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo === Visual Studio Environment initialized ===

for /f %%A in ('powershell -NoProfile -Command "(Get-Date).ToString('o')"') do set "START_ISO=%%A"

echo.
echo [1/3] Building SIMILI_UI...
cd /d "%~dp0src\SIMILI_UI\build"
if %errorlevel% neq 0 (
    echo ERROR: Folder src\SIMILI_UI\build not found
    exit /b 1
)

cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo ERROR: SIMILI_UI compilation failed
    exit /b 1
)
echo   + SIMILI_UI.exe compiled

cd /d "%~dp0"

echo.
echo [2/3] Deploying SIMILI_UI...


if not exist "build" mkdir "build"
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

copy /Y "src\SIMILI_UI\build\UI_Engine\Release\SIMILI_UI.exe" "build\Release\" >nul 2>nul
if %errorlevel% neq 0 (
    echo WARNING: Failed to copy SIMILI_UI.exe
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

echo Copying UI HTML files...
if not exist "build\Release\ui" mkdir "build\Release\ui"
xcopy /Y /Q "ui\*.html" "build\Release\ui\" >nul 2>nul
xcopy /Y /Q "ui\*.css" "build\Release\ui\" >nul 2>nul
xcopy /Y /Q "ui\*.js" "build\Release\ui\" >nul 2>nul
if %errorlevel% neq 0 (
    echo WARNING: Failed to copy UI HTML files
)

echo   + SIMILI_UI deployed with CEF dependencies

echo.
echo [3/3] Building SIMILI...

if not exist build mkdir build
cd build

if not exist "CMakeCache.txt" (
    echo Generating CMake configuration...
    cmake -G "Visual Studio 17 2022" -A x64 ..
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

if not exist "Release\SDL3.dll" copy "C:\libs\SDL3\lib\x64\SDL3.dll" "Release\SDL3.dll" >nul

if not exist "src\resources" mkdir src\resources
if not exist "src\resources\default_imgui_layout.ini" copy "..\src\resources\default_imgui_layout.ini" "src\resources\default_imgui_layout.ini" >nul

echo   + SIMILI compiled

cd ..

for /f %%A in ('powershell -NoProfile -Command "(Get-Date).ToString('o')"') do set "END_ISO=%%A"

for /f "usebackq" %%A in (`powershell -NoProfile -Command ^
  "$s=[datetime]::ParseExact('%START_ISO%','o',$null); $e=[datetime]::ParseExact('%END_ISO%','o',$null); (New-TimeSpan -Start $s -End $e).TotalSeconds.ToString('0.00')"`) do set "DURATION=%%A"

echo.
echo ========================================
echo    BUILD COMPLETE!
echo    Total time: %DURATION%s
echo ========================================
echo.
echo Executables:
echo   - build\Release\Project_SIMILI.exe
echo   - build\Release\SIMILI_UI.exe
echo.
