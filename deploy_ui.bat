@echo off


echo Déploiement de SIMILI_UI.exe...

copy /Y "src\SIMILI_UI\build\Release\SIMILI_UI.exe" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERREUR: Impossible de copier SIMILI_UI.exe
    exit /b 1
)


echo Copie des DLLs CEF...
copy /Y "src\ThirdParty\CEF\cef_binary\Release\*.dll" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERREUR: Impossible de copier les DLLs CEF
    exit /b 1
)


copy /Y "src\ThirdParty\CEF\cef_binary\Release\*.bin" "build\Release\" >nul 2>nul


echo Copie des ressources CEF...
copy /Y "src\ThirdParty\CEF\cef_binary\Resources\*.pak" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERREUR: Impossible de copier les fichiers .pak
    exit /b 1
)

copy /Y "src\ThirdParty\CEF\cef_binary\Resources\*.dat" "build\Release\" >nul 2>nul


if not exist "build\Release\locales" mkdir "build\Release\locales"
copy /Y "src\ThirdParty\CEF\cef_binary\Resources\locales\*" "build\Release\locales\" >nul
if %errorlevel% neq 0 (
    echo ERREUR: Impossible de copier le dossier locales
    exit /b 1
)

echo.
echo ✓ Déploiement terminé avec succès !
echo.
echo Fichiers copiés dans build\Release\ :
echo   - SIMILI_UI.exe
echo   - DLLs CEF (libcef.dll, etc.)
echo   - Ressources CEF (*.pak, locales/)
echo.
