@echo off
setlocal enabledelayedexpansion

rem Find Visual Studio installation using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Error: vswhere.exe not found.
  echo Install Visual Studio or Visual Studio Build Tools and make sure the installer is present.
  exit /b 1
)
for /f "usebackq tokens=* delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL (
  echo Error: Visual Studio with C++ tools not found.
  exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set "QT_DIR=C:\Qt\6.5.3\msvc2019_64"
if not exist "%QT_DIR%" (
  echo Error: Qt directory not found: %QT_DIR%
  echo Update QT_DIR in build.bat or install Qt 6.5.3.
  exit /b 1
)
set "CMAKE_PREFIX_PATH=%QT_DIR%"
set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"
for %%I in ("%TEMP%") do set "TEMP_BASE=%%~fI"
if not "%TEMP_BASE:~-1%"=="\" set "TEMP_BASE=%TEMP_BASE%\"
set "RUN_ID=%RANDOM%%RANDOM%%RANDOM%"
set "TEMP_ROOT=%TEMP_BASE%QuizAppBuild-%RUN_ID%"
set "TEMP_SOURCE=%TEMP_ROOT%\source"
set "TEMP_BUILD=%TEMP_ROOT%\build"
set "DIST_DIR=%PROJECT_ROOT%\dist\QuizApp-Windows"

where cmake >nul 2>&1
if errorlevel 1 (
  set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
  if not exist "%CMAKE%" (
    echo Error: cmake not found in PATH or Program Files.
    exit /b 1
  )
) else (
  set "CMAKE=cmake"
)

where ninja >nul 2>&1
if errorlevel 1 (
  echo Error: ninja not found in PATH. Please install Ninja and restart the console.
  exit /b 1
)

mkdir "%TEMP_SOURCE%"
if errorlevel 1 exit /b 1

mkdir "%TEMP_BUILD%"
if errorlevel 1 exit /b 1

robocopy "%PROJECT_ROOT%\src" "%TEMP_SOURCE%\src" /E >nul
if errorlevel 8 exit /b 1

robocopy "%PROJECT_ROOT%\data" "%TEMP_SOURCE%\data" /E >nul
if errorlevel 8 exit /b 1

if exist "%PROJECT_ROOT%\assets" (
  robocopy "%PROJECT_ROOT%\assets" "%TEMP_SOURCE%\assets" /E >nul
  if errorlevel 8 exit /b 1
)

copy /y "%PROJECT_ROOT%\CMakeLists.txt" "%TEMP_SOURCE%\CMakeLists.txt" >nul
if errorlevel 1 exit /b 1

if exist "%PROJECT_ROOT%\README.md" (
  copy /y "%PROJECT_ROOT%\README.md" "%TEMP_SOURCE%\README.md" >nul
  if errorlevel 1 exit /b 1
)

"%CMAKE%" -S "%TEMP_SOURCE%" -B "%TEMP_BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%TEMP_BUILD%" --config Release
if errorlevel 1 exit /b 1

set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
  echo Error: windeployqt not found at %WINDEPLOYQT%.
  echo Install Qt or update QT_DIR in build.bat.
  exit /b 1
)
"%WINDEPLOYQT%" --release "%TEMP_BUILD%\bin\QuizApp.exe"
if errorlevel 1 exit /b 1

mkdir "%DIST_DIR%"
if errorlevel 1 if not exist "%DIST_DIR%" exit /b 1

robocopy "%TEMP_BUILD%\bin" "%DIST_DIR%" /MIR >nul
if errorlevel 8 exit /b 1

echo Build and deploy complete: %DIST_DIR%\QuizApp.exe
endlocal
