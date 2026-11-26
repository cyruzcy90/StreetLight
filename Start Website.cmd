@echo off
title Smart Street Light - local website
set "NODE_DIR=C:\Program Files\nodejs"
set "PROJ=C:\Users\User\OneDrive\Desktop\smart-street-light"

cd /d "%PROJ%\website"
if errorlevel 1 (
  echo [X] Cannot find the folder:
  echo     %PROJ%\website
  echo     Did you move or rename the smart-street-light folder?
  pause
  exit /b 1
)

if not exist "%NODE_DIR%\npx.cmd" (
  echo [X] Node.js is not installed at %NODE_DIR%
  echo     Install it from https://nodejs.org  ^(LTS button^)
  pause
  exit /b 1
)

if not exist node_modules (
  echo Installing the local server, first time only. This takes about a minute...
  call "%NODE_DIR%\npm.cmd" install
  if errorlevel 1 (
    echo [X] npm install failed. Scroll up for the reason.
    pause
    exit /b 1
  )
)

if not "%NO_BROWSER%"=="1" start "" powershell -NoProfile -WindowStyle Hidden -Command "Start-Sleep 10; Start-Process 'http://127.0.0.1:8787'"

echo.
echo ============================================================
echo   Dashboard starting on http://127.0.0.1:8787
echo   Access key to type in the box:  dev-key-change-me
echo   Keep this window open. Press Ctrl+C to stop.
echo ============================================================
echo.

call "%NODE_DIR%\npx.cmd" wrangler dev --ip 127.0.0.1 --port 8787
echo.
echo Server stopped.
pause
