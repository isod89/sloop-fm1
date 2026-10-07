@echo off
setlocal
title SLOOP FM-1 Emulator
cd /d "%~dp0"

echo ===============================================================================
echo   SLOOP - Emulador del M-VAVE FM-1 (firmware real sobre HAL emulada)
echo ===============================================================================
echo.

REM Python 3 (py launcher first: "python" may be the Microsoft Store alias)
set "PY="
py -3 --version >nul 2>&1 && set "PY=py -3"
if not defined PY (
    python --version >nul 2>&1 && set "PY=python"
)
if not defined PY (
    echo [ERROR] No se encontro Python 3. Instala Python 3 ^(https://www.python.org^) e intenta de nuevo.
    pause
    exit /b 1
)

REM Builds only when firmware/src, tools/emulator or build/gen changed; extra arguments go to the build
REM (e.g. --force, --no-static-assert)
%PY% tools\emulator\build_emu.py %*
if errorlevel 1 (
    echo.
    echo [ERROR] La compilacion fallo: revisa los errores de arriba. No se lanza un binario viejo.
    pause
    exit /b 1
)

echo.
echo   Teclas  Z..  /  Q..O  notas F3-G5      F1-F10  FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ
echo   Espacio PLAY   Tab REC   AvPag/RePag OCT-/OCT+   Flechas SELECT/PRESET
echo   Raton   clic = presionar, clic derecho = enclavar, rueda sobre un encoder = girarlo
echo   F12     captura de la LCD (build\emu\shots)
echo   Flash   build\emu\sloop_flash.bin (borralo para arrancar como un FM-1 recien formateado)
echo.
start "" /d "build\emu" "build\emu\sloop_emu.exe"
