@echo off

:: change current directory to the one where this script is located
:: if executed as administrator it would change to system32 otherwise
pushd %~dp0

python --version 2>NUL
if errorlevel 1 goto errorNoPython

if exist %UserProfile%\.platformio\penv\Scripts\ (
  echo PIO detected. Skip installation...
) else (
  python get-platformio.py
)

SET PATH=%PATH%;%UserProfile%\.platformio\penv\Scripts

if exist board.txt (
  set /p PIO_ENVIRONMENT=<board.txt
) else (
  echo board.txt not found
  pause
  exit
)

pio run -e %PIO_ENVIRONMENT% -t upload

pause
exit

:errorNoPython
echo Python not installed. You will be redirected to Microsoft Store in order to install it.
echo Execute this script again after installation completed.
pause
start "" https://www.microsoft.com/store/productId/9P7QFQMJRFP7