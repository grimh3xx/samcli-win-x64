@echo off
rem Compila o samcli.exe. Use o "x64 Native Tools Command Prompt" (MSVC) ou tenha o MinGW-w64 no PATH.
set SRC=src\main.c src\steam.c src\ui.c src\platform.c
where cl >nul 2>nul && (cl /nologo /O2 /W3 /utf-8 %SRC% /Fe:samcli.exe advapi32.lib & goto :eof)
where gcc >nul 2>nul && (gcc -O2 -Wall -o samcli.exe %SRC% -ladvapi32 & goto :eof)
echo Nenhum compilador encontrado (cl ou gcc).
