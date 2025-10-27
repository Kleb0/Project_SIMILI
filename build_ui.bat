@echo off

echo ========================================
echo    Compilation de SIMILI_UI.exe
echo ========================================
echo.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %errorlevel% neq 0 (
    echo ERREUR: Impossible d'initialiser vcvars64
    exit /b 1
)


cd /d "%~dp0src\SIMILI_UI\build"
if %errorlevel% neq 0 (
    echo ERREUR: Dossier src\SIMILI_UI\build introuvable
    exit /b 1
)

echo Compilation en cours (Release)...
echo.

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo.
    echo ERROR : SIMILI_UI.exe compilation failed !
    exit /b 1
)

echo.
echo ========================================
echo  SIMULI.UI successful compiled !
echo ========================================
echo Emplacement: src\SIMILI_UI\build\Release\SIMILI_UI.exe
echo.

cd /d "%~dp0"
