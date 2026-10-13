@echo off
setlocal

if not exist "%~dp0build\windows\CMakeCache.txt" (
    cmake -S "%~dp0Project1" -B "%~dp0build\windows" -G "Visual Studio 17 2022" -A x64
    if errorlevel 1 goto failed
)

cmake --build "%~dp0build\windows" --config Debug
if errorlevel 1 goto failed

pushd "%~dp0Project1"
if errorlevel 1 goto failed
"..\build\windows\bin\Debug\VulkanRenderer.exe"
set "runResult=%errorlevel%"
popd
if not "%runResult%"=="0" goto failed
exit /b 0

:failed
echo Build or execution failed. See the error above.
pause
exit /b 1
