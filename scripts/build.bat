@echo off
rem Aurora Player - one-command build for Windows (MSVC or MinGW).
setlocal
set ROOT=%~dp0..
set BUILD_DIR=%ROOT%\build\release

where cmake >nul 2>nul
if errorlevel 1 (
  echo cmake is required: https://cmake.org/download/
  exit /b 1
)

echo ==^> Configuring
cmake -S "%ROOT%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release %*
if errorlevel 1 exit /b 1

echo ==^> Building
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

echo ==^> Testing
ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure

echo.
echo Done: %BUILD_DIR%\bin\aurora-player.exe
endlocal
