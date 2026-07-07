@echo off
REM ===========================================================================
REM  create_listing.bat  -  pic32mm_app (PIC32MM0256GPM064)
REM
REM  Generates a disassembly listing + symbol table from the XC32 ELF so you can
REM  locate a MIPS exception (the "epc=" / "ra=" address from the UART dump) in
REM  your source. See ..\MIPS\Exception_Dump_Mips\tools\reading_the_dump.md.
REM
REM  Run it from this .X folder (double-click, or from a shell). You can also
REM  pass an explicit ELF path as the first argument:
REM
REM      create_listing.bat  dist\default\production\pic32mm_app.X.production.elf
REM ===========================================================================

setlocal

REM --- XC32 toolchain bin directory (this project uses XC32 v4.60) ------------
if "%XC32_BIN%"=="" set "XC32_BIN=C:\Program Files\Microchip\xc32\v4.60\bin"

REM --- ELF selection ---------------------------------------------------------
REM  1) explicit argument wins; 2) else production if built; 3) else debug.
set "ELF_PROD=dist\default\production\pic32mm_app.X.production.elf"
set "ELF_DBG=dist\default\debug\pic32mm_app.X.debug.elf"

if not "%~1"=="" (
    set "ELF=%~1"
) else if exist "%ELF_PROD%" (
    set "ELF=%ELF_PROD%"
) else (
    set "ELF=%ELF_DBG%"
)

REM ---------------------------------------------------------------------------
if not exist "%ELF%" (
    echo [ERROR] ELF not found: "%ELF%"
    echo Build the project first, or pass the ELF path as the first argument.
    echo Usage: %~nx0 [path\to\project.elf]
    exit /b 1
)
if not exist "%XC32_BIN%\xc32-objdump.exe" (
    echo [ERROR] xc32-objdump.exe not found in "%XC32_BIN%"
    echo Adjust XC32_BIN to your installed XC32 version under
    echo   C:\Program Files\Microchip\xc32\
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
echo (the "epc=" value from the dump), or run analyze_dump.py.
echo.
endlocal
exit /b 0

:err
echo.
echo [ERROR] A tool step failed. Check XC32_BIN and the ELF path.
endlocal
exit /b 1
