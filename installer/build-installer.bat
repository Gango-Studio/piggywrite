@echo off
setlocal
cd /d "%~dp0"

echo [1/2] Building piggywrite.exe...
call "%~dp0..\build.bat"
if errorlevel 1 exit /b 1

set "ISCC=%ProgramFiles%\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    for %%I in (ISCC.exe) do set "ISCC=%%~$PATH:I"
)
if not exist "%ISCC%" (
    echo [ERROR] ISCC.exe was not found. Install Inno Setup 6 or 7 first.
    exit /b 1
)

echo [2/2] Building installer with Inno Setup...
"%ISCC%" "%~dp0piggywrite.iss"
if errorlevel 1 exit /b 1

echo.
echo Installer created: %~dp0dist\piggywrite-1.0-setup.exe
