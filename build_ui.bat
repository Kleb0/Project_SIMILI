@echo off

echo ========================================
echo    Building SIMILI_UI.exe
echo ========================================
echo.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Unable to initialize vcvars64
    exit /b 1
)

cd /d "%~dp0src\SIMILI_UI\build"
if %errorlevel% neq 0 (
    echo ERROR: Folder src\SIMILI_UI\build not found
    exit /b 1
)

echo Building (Release)...
echo.

cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo.
    echo ERROR: SIMILI_UI.exe compilation failed!
    exit /b 1
)

echo.
echo ========================================
echo  SIMILI_UI successfully compiled!
echo ========================================
echo Location: src\SIMILI_UI\build\Release\SIMILI_UI.exe
echo.

cd /d "%~dp0"
