@echo off
setlocal EnableExtensions


call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo === Visual Studio Code Environment initialized ===

REM 
for /f %%A in ('powershell -NoProfile -Command "(Get-Date).ToString('o')"') do set "START_ISO=%%A"

REM Delete and recreate the build directory
if exist build rmdir /s /q build
mkdir build
cd build

REM Generate files with CMakes
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release


REM 
for /f %%A in ('powershell -NoProfile -Command "(Get-Date).ToString('o')"') do set "END_ISO=%%A"

REM
for /f %%A in ('powershell -NoProfile -Command ^
  "$s=[datetime]::ParseExact('%START_ISO%','o',$null); $e=[datetime]::ParseExact('%END_ISO%','o',$null); (New-TimeSpan -Start $s -End $e).ToString()"') do set "DURATION_HMS=%%A"
echo === (%DURATION_HMS%) ===


REM
for /f "usebackq" %%A in (`powershell -NoProfile -Command ^
  "$s=[datetime]::ParseExact('%START_ISO%','o',$null); $e=[datetime]::ParseExact('%END_ISO%','o',$null); (New-TimeSpan -Start $s -End $e).TotalSeconds.ToString('0.00')"`) do set "DURATION=%%A"

echo === Build finished in %DURATION% seconds ===


REM Create the Release directory and resources directory if not exist
mkdir Release
mkdir src\resources

REM Copy the executable dll to the Release directory
copy "C:\libs\SDL3\lib\x64\SDL3.dll" "Release\SDL3.dll"

REM Copy the layout file to the resources directory
copy "..\src\resources\default_imgui_layout.ini" "src\resources\default_imgui_layout.ini"