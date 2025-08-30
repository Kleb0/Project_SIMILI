@echo off
setlocal enabledelayedexpansion

if /I "%~1" NEQ "test" (
    echo Usage: run test ^<file_name.cpp^>
    exit /b 1
)

if "%~2"=="" (
    echo Usage: run test ^<file_name.cpp^>
    exit /b 1
)

set TEST_ARG=%~2
set "PROJ_ROOT=%~dp0"
set "BUILD_DIR=%PROJ_ROOT%build_test"

set "CAND1=%PROJ_ROOT%src\UnitTest\%TEST_ARG%"
if exist "%CAND1%" (
  set "TEST_FILE=%CAND1%"
) else (
  echo Test source not found: %TEST_ARG%
  exit /b 1
)

for %%I in ("%TEST_ARG%") do set "TEST_NAME=%%~nI"

echo.
echo [SIMILI TEST] Building and running %TEST_FILE%  -- exe: %TEST_NAME%.exe
echo.

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

cmake -S "%PROJ_ROOT%src\UnitTest" -B "%BUILD_DIR%" -DTEST_FILE=%TEST_FILE% -DTEST_NAME=%TEST_NAME% -G "Visual Studio 17 2022" -A x64 || exit /b %ERRORLEVEL%
cmake --build "%BUILD_DIR%" --config Release || exit /b %ERRORLEVEL%
"%BUILD_DIR%\Release\%TEST_NAME%.exe"

endlocal
