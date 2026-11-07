@echo off
setlocal enableextensions
rem ===========================================================================
rem  aquadeck: release build -> release\aquadeck\ (+ aquadeck_v1.0.0.zip)
rem  Needs: Visual Studio with the "Desktop development with C++" workload,
rem         SDL3 unpacked to the folder below.
rem ===========================================================================
set "SDL_DIR=C:\libs\SDL3-3.4.20"
set "VERSION=1.0.0"

cd /d "%~dp0"
set "ROOT=%CD%"
set "BIN=%ROOT%\build\bin"
set "OBJ=%ROOT%\build\obj"
set "REL=%ROOT%\release\aquadeck"

echo [1/5] Looking for MSBuild...
set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
    where msbuild >nul 2>nul && set "MSBUILD=msbuild"
)
if not defined MSBUILD (
    echo ERROR: MSBuild not found. Install Visual Studio with the C++ desktop workload,
    echo        or run this file from "Developer Command Prompt".
    goto :fail
)
echo       %MSBUILD%

if not exist "%SDL_DIR%\lib\x64\SDL3.dll" (
    echo ERROR: %SDL_DIR%\lib\x64\SDL3.dll not found. Fix SDL_DIR at the top of this file.
    goto :fail
)

echo [2/5] Closing running aquadeck and cleaning old output...
taskkill /f /im aquadeck.exe >nul 2>nul
if exist "%ROOT%\build" rmdir /s /q "%ROOT%\build"
if exist "%ROOT%\release" rmdir /s /q "%ROOT%\release"
mkdir "%BIN%" "%OBJ%" "%REL%"

echo [3/5] Compiling (Release, x64, no console window)...
"%MSBUILD%" "%ROOT%\test.vcxproj" /t:Rebuild /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%BIN%\\" "/p:IntDir=%OBJ%\\" /m /nologo /v:minimal
if errorlevel 1 (
    echo ERROR: build failed, see messages above.
    goto :fail
)
if not exist "%BIN%\aquadeck.exe" (
    echo ERROR: aquadeck.exe was not produced.
    goto :fail
)

echo [4/5] Assembling release folder...
copy /y "%BIN%\aquadeck.exe" "%REL%\" >nul
copy /y "%SDL_DIR%\lib\x64\SDL3.dll" "%REL%\" >nul
if exist "%ROOT%\sprite.bmp" copy /y "%ROOT%\sprite.bmp" "%REL%\" >nul
if exist "%ROOT%\FONT_SETUP.txt" copy /y "%ROOT%\FONT_SETUP.txt" "%REL%\README_FONT.txt" >nul

echo [5/5] Making zip...
powershell -NoProfile -Command "Compress-Archive -Path '%REL%\*' -DestinationPath '%ROOT%\release\aquadeck_v%VERSION%.zip' -Force"

echo.
echo DONE: %REL%
echo       %ROOT%\release\aquadeck_v%VERSION%.zip
echo Reminder: the font "Press Start 2P" must be installed on the target PC (see README_FONT.txt).
echo.
pause
exit /b 0

:fail
echo.
echo Build failed.
pause
exit /b 1
