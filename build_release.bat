@echo off
setlocal enableextensions enabledelayedexpansion
rem ===========================================================================
rem  aquadeck - release build.
rem    1. finds MSBuild and SDL3
rem    2. compiles Release x64: GUI app (NO console window), static CRT, icon from aquadeck.rc / mark.ico
rem    3. checks that the exe really is a GUI app and has the icon resource
rem    4. assembles  release\aquadeck\  (exe + SDL3.dll + sprite.bmp + font note)  and  release\aquadeck_vX.zip
rem
rem  Needs: Visual Studio with the "Desktop development with C++" workload, SDL3 unpacked to a folder.
rem  SDL3 folder: edit SDL_DIR below, or set the SDL_DIR environment variable before running.
rem  Run with the argument  nopause  to skip the final "press any key".
rem ===========================================================================
if not defined SDL_DIR set "SDL_DIR=C:\libs\SDL3-3.4.20"
set "VERSION=1.0.0"
set "APP=aquadeck"

cd /d "%~dp0"
set "ROOT=%CD%"
set "BIN=%ROOT%\build\bin"
set "OBJ=%ROOT%\build\obj"
set "REL=%ROOT%\release\%APP%"
set "ZIP=%ROOT%\release\%APP%_v%VERSION%.zip"

echo [1/6] Checking tools and files...
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
    echo        or run this file from "Developer Command Prompt for VS".
    goto :fail
)
echo       MSBuild: %MSBUILD%

if not exist "%SDL_DIR%\include\SDL3\SDL.h" (
    echo ERROR: SDL3 headers not found in "%SDL_DIR%\include".
    echo        Fix SDL_DIR at the top of this file, or set the SDL_DIR environment variable.
    goto :fail
)
if not exist "%SDL_DIR%\lib\x64\SDL3.lib" (
    echo ERROR: "%SDL_DIR%\lib\x64\SDL3.lib" not found ^(need the x64 SDL3 development package^).
    goto :fail
)
if not exist "%SDL_DIR%\lib\x64\SDL3.dll" (
    echo ERROR: "%SDL_DIR%\lib\x64\SDL3.dll" not found.
    goto :fail
)
echo       SDL3:    %SDL_DIR%

if not exist "%ROOT%\mark.ico" (
    echo ERROR: mark.ico not found - the exe icon comes from it ^(see %APP%.rc^).
    goto :fail
)
if not exist "%ROOT%\%APP%.rc" (
    echo ERROR: %APP%.rc not found.
    goto :fail
)

echo [2/6] Closing running %APP% and cleaning old output...
taskkill /f /im %APP%.exe >nul 2>nul
if exist "%ROOT%\build" rmdir /s /q "%ROOT%\build"
if exist "%ROOT%\release" rmdir /s /q "%ROOT%\release"
mkdir "%BIN%" "%OBJ%" "%REL%"
if errorlevel 1 (
    echo ERROR: cannot create output folders. Is something from build\ or release\ still open?
    goto :fail
)

echo [3/6] Compiling ^(Release, x64, no console window, icon^)...
"%MSBUILD%" "%ROOT%\test.vcxproj" /t:Rebuild /p:Configuration=Release /p:Platform=x64 "/p:SDL_DIR=%SDL_DIR%" "/p:OutDir=%BIN%\\" "/p:IntDir=%OBJ%\\" /m /nologo /v:minimal
if errorlevel 1 (
    echo ERROR: build failed, see messages above.
    goto :fail
)
if not exist "%BIN%\%APP%.exe" (
    echo ERROR: %APP%.exe was not produced.
    goto :fail
)

echo [4/6] Checking the exe...
rem PE header: Subsystem 2 = Windows GUI (no console window), 3 = Console.
powershell -NoProfile -Command "$b=[IO.File]::ReadAllBytes('%BIN%\%APP%.exe'); $pe=[BitConverter]::ToInt32($b,60); $s=[BitConverter]::ToUInt16($b,$pe+24+68); if($s -eq 2){exit 0}else{exit 1}"
if errorlevel 1 (
    echo ERROR: the exe is built as a console app ^(a black window would open^). Check Link/SubSystem=Windows
    echo        and EntryPointSymbol=mainCRTStartup for Release^|x64 in test.vcxproj.
    goto :fail
)
echo       GUI app, no console window: OK
if exist "%OBJ%\%APP%.res" (
    echo       icon resource compiled from %APP%.rc: OK
) else (
    echo WARNING: %OBJ%\%APP%.res not found - the exe may have no icon. Is %APP%.rc in test.vcxproj?
)

echo [5/6] Assembling release folder...
copy /y "%BIN%\%APP%.exe" "%REL%\" >nul
copy /y "%SDL_DIR%\lib\x64\SDL3.dll" "%REL%\" >nul
if exist "%ROOT%\sprite.bmp" copy /y "%ROOT%\sprite.bmp" "%REL%\" >nul
if exist "%ROOT%\FONT_SETUP.txt" copy /y "%ROOT%\FONT_SETUP.txt" "%REL%\README_FONT.txt" >nul

echo [6/6] Making zip...
powershell -NoProfile -Command "Compress-Archive -Path '%REL%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 echo WARNING: zip was not created, the folder %REL% is ready anyway.

echo.
echo DONE: %REL%
echo       %ZIP%
echo Notes:
echo   - The font "Press Start 2P" must be installed on the target PC ^(see README_FONT.txt^).
echo   - If Explorer still shows an old icon, it is the icon cache: rename the exe or restart Explorer.
echo.
if /i not "%~1"=="nopause" pause
exit /b 0

:fail
echo.
echo Build failed.
if /i not "%~1"=="nopause" pause
exit /b 1
