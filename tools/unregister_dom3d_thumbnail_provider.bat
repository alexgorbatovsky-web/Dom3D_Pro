@echo off
setlocal
set "DLL=%~dp0..\build\Release\Dom3DThumbnailProvider.dll"
if not exist "%DLL%" (
    echo Dom3DThumbnailProvider.dll not found:
    echo   %DLL%
    exit /b 1
)
regsvr32 /u "%DLL%"
