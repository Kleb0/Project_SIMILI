@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo === Visual Studio Code Environment initialized ===

REM Delete and recreate the build directory
if exist build rmdir /s /q build
mkdir build
cd build

REM Generate files with CMake
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

REM Create the Release directory and resources directory if not exist
mkdir Release
mkdir src\resources

REM Copy the executable dll to the Release directory
copy "C:\libs\SDL3\lib\x64\SDL3.dll" "Release\SDL3.dll"

REM Copy the layout file to the resources directory
copy "..\src\resources\default_imgui_layout.ini" "src\resources\default_imgui_layout.ini"