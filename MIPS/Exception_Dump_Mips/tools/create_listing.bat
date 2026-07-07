@echo off
REM ===========================================================================
REM  create_listing.bat  -  TEMPLATE
REM
REM  Generates a disassembly listing + symbol table from an XC32 ELF so you can
REM  locate a PIC32 / MIPS exception by its address (see reading_the_dump.md).
REM
REM  This is a TEMPLATE. Adapt the two ADAPT lines below to your project, or let
REM  Claude Code adapt it for you. You can also pass the ELF path as argument 1:
REM
REM      create_listing.bat  path\to\MyProject.X.production.elf
REM
REM  Typical place to run it from: your MPLAB X ".X" project folder.
REM ===========================================================================

setlocal

REM --- ADAPT 1: XC32 toolchain bin directory (must contain xc32-objdump.exe) --
REM     Check your installed version under C:\Program Files\Microchip\xc32\
if "%XC32_BIN%"=="" set "XC32_BIN=C:\Program Files\Microchip\xc32\v4.45\bin"

REM --- ADAPT 2: path to your built ELF (or pass it as the first argument) -----
REM     MPLAB X puts it under: <project>.X\dist\<config>\production\<name>.production.elf
if not "%~1"=="" (
    set "ELF=%~1"
) else (
    set "ELF=.\dist\default\production\MyProject.X.production.elf"
)

REM ---------------------------------------------------------------------------
if not exist "%ELF%" (
    echo [ERROR] ELF not found: "%ELF%"
    echo Usage: %~nx0 [path\to\project.elf]
    exit /b 1
)
if not exist "%XC32_BIN%\xc32-objdump.exe" (
    echo [ERROR] xc32-objdump.exe not found in "%XC32_BIN%"
    echo Set XC32_BIN to your XC32 bin directory (see ADAPT 1^).
    exit /b 1
)

REM Output base name = ELF path without the .elf extension
set "OUT=%ELF:.elf=%"

echo.
echo Using XC32 : %XC32_BIN%
echo ELF        : %ELF%
echo.

echo [1/2] Source-interleaved disassembly -^> "%OUT%.disassembly.txt"
"%XC32_BIN%\xc32-objdump" -d -S -l "%ELF%" > "%OUT%.disassembly.txt"
if errorlevel 1 goto :err

echo [2/2] Symbol table (address -^> name^) -^> "%OUT%.symbols.txt"
"%XC32_BIN%\xc32-readelf" -s "%ELF%" > "%OUT%.symbols.txt"
if errorlevel 1 goto :err

echo.
echo Done. Open the .disassembly.txt and search for the exception address
echo (the "epc=" value from the dump). See reading_the_dump.md.
echo.
endlocal
exit /b 0

:err
echo.
echo [ERROR] A tool step failed. Check the paths in ADAPT 1 / ADAPT 2.
endlocal
exit /b 1
