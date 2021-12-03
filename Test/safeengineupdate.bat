@echo off
if [%1]==[] GOTO usage 
if [%2]==[] GOTO usage 

call safeservicestop.bat %1 coappsvr
call safeservicestop.bat %1 codbsvr

@copy /y %2 \\%1\engine\

IF [%3]==[] GOTO start
@copy /y %3 \\%1\engine\

IF [%4]==[] GOTO start
@copy /y %4 \\%1\engine\

IF [%5]==[] GOTO start
@copy /y %5 \\%1\engine\

IF [%6]==[] GOTO start
@copy /y %6 \\%1\engine\

IF [%7]==[] GOTO start
@copy /y %7 \\%1\engine\

IF [%8]==[] GOTO start
@copy /y %8 \\%1\engine\

:start
call safeservicestart.bat %1 coappsvr

goto:eof

:usage
echo Will update the engine on an application server
echo. 
echo %0 [system name] [new engine filename] 
echo Example: %0 192.168.3.1 engine.exe 

goto:eof
