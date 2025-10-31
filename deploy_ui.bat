@echo off

echo Deploying SIMILI_UI.exe...

copy /Y "src\SIMILI_UI\build\UI_Engine\Release\SIMILI_UI.exe" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy SIMILI_UI.exe
    exit /b 1
)

echo Copying CEF DLLs...
copy /Y "src\ThirdParty\CEF\cef_binary\Release\*.dll" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy CEF DLLs
    exit /b 1
)

copy /Y "src\ThirdParty\CEF\cef_binary\Release\*.bin" "build\Release\" >nul 2>nul

echo Copying CEF resources...
copy /Y "src\ThirdParty\CEF\cef_binary\Resources\*.pak" "build\Release\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy .pak files
    exit /b 1
)

copy /Y "src\ThirdParty\CEF\cef_binary\Resources\*.dat" "build\Release\" >nul 2>nul

if not exist "build\Release\locales" mkdir "build\Release\locales"
copy /Y "src\ThirdParty\CEF\cef_binary\Resources\locales\*" "build\Release\locales\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy locales folder
    exit /b 1
)

echo Copying UI HTML files...
if not exist "build\Release\ui" mkdir "build\Release\ui"
copy /Y "ui\*.html" "build\Release\ui\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy UI HTML files
    exit /b 1
)

copy /Y "ui\*.css" "build\Release\ui\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy UI CSS files
    exit /b 1
)

copy /Y "ui\*.js" "build\Release\ui\" >nul
if %errorlevel% neq 0 (
    echo ERROR: Unable to copy UI JS files
    exit /b 1
)

echo.
echo Deployment completed successfully!
echo.
echo Files copied to build\Release\ :
echo   - SIMILI_UI.exe
echo   - CEF DLLs (libcef.dll, etc.)
echo   - CEF Resources (*.pak, locales/)
echo   - UI HTML files (ui/*.html)
echo   - UI CSS files (ui/*.css)
echo   - UI JS files (ui/*.js)
echo.
