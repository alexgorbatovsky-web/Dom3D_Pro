@echo off
setlocal
set "DLL=%~dp0..\build\Release\Dom3DThumbnailProvider.dll"
if not exist "%DLL%" (
    echo Dom3DThumbnailProvider.dll not found:
    echo   %DLL%
    echo Build it first:
    echo   cmake --build build --config Release --target Dom3DThumbnailProvider
    exit /b 1
)
regsvr32 "%DLL%"
