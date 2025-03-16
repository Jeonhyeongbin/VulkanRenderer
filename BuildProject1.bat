@echo off
setlocal enabledelayedexpansion

@echo off
set LOGFILE=%~dp0BuildProject_log.txt

net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    pause
)

if not defined VULKAN_SDK (
    echo Vulkan SDK not installed. Start installing Vulkan Sdk Progress...

    set "VULKAN_INSTALLER_URL=https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe"

    set "INSTALLER_PATH=%TEMP%\vulkan-sdk.exe"
    set "LOG_PATH=%TEMP%\vulkan_install.log"

    echo [DEBUG] Download URL: !VULKAN_INSTALLER_URL!
    echo [DEBUG] Vulkan Path: !INSTALLER_PATH!
    echo [DEBUG] Log Path: !LOG_PATH!

    echo [%date% %time%] Vulkan SDK downloading...
    echo [%date% %time%] Vulkan SDK downloading... >> "%LOGFILE%"
    powershell -Command "(New-Object System.Net.WebClient).DownloadFile('!VULKAN_INSTALLER_URL!', '!INSTALLER_PATH!')"
    
    if not exist "!INSTALLER_PATH!" (
        echo [%date% %time%] Vulkan SDK Download Fail.
        echo [%date% %time%] Vulkan SDK Download Fail. >> "%LOGFILE%"
        exit /b 1
    )

    echo Vulkan SDK Installing...
    "!INSTALLER_PATH!" --accept-licenses --default-answer --confirm-command install

    if not defined %VULKAN_SDK% (
        echo [%date% %time%] Vulkan SDK Installing Failed.
        echo [%date% %time%] Vulkan SDK Installing Failed. >> "%LOGFILE%"
        exit /b 1
    )
) else (
    echo [%date% %time%] Vulkan SDK already installed.
    echo [%date% %time%] Vulkan SDK already installed. >> "%LOGFILE%"
)

echo [%date% %time%] shader compile running...
echo [%date% %time%] shader compile running... >> "%LOGFILE%"
call "%~dp0Project1\ShaderCompile.bat"

if %errorlevel% neq 0 (
    echo [%date% %time%] compile.bat excution failed. error code: %errorlevel%
    echo [%date% %time%] compile.bat excution failed. error code: %errorlevel% >> "%LOGFILE%"
    pause
)

echo [%date% %time%] success
echo [%date% %time%] success >> "%LOGFILE%"
pause