@echo off 
:: This script originally authored by Eric Falsken 
 
IF [%1]==[] GOTO usage 
IF [%2]==[] GOTO usage 
 
ping -n 1 %1 | FIND "TTL=" >NUL 
IF errorlevel 1 GOTO SystemOffline 
SC \\%1 query %2 | FIND "STATE" >NUL 
IF errorlevel 1 GOTO SystemOffline 
 
:ResolveInitialState 
SC \\%1 query %2 | FIND "STATE" | FIND "RUNNING" >NUL 
IF errorlevel 0 IF NOT errorlevel 1 GOTO StopService 
SC \\%1 query %2 | FIND "STATE" | FIND "STOPPED" >NUL 
IF errorlevel 0 IF NOT errorlevel 1 GOTO StopedService 
SC \\%1 query %2 | FIND "STATE" | FIND "PAUSED" >NULL 
IF errorlevel 0 IF NOT errorlevel 1 GOTO SystemOffline 
echo Service State is changing, waiting for service to resolve its state before making changes 
sc \\%1 query %2 | Find "STATE" 
timeout /t 2 /nobreak >NUL 
GOTO ResolveInitialState 
 
:StopService 
echo Stopping %2 on \\%1 
sc \\%1 stop %2 %3 >nul
 
GOTO StopingService 
:StopingServiceDelay 
echo Waiting for %2 to stop 
timeout /t 2 /nobreak >NUL 
:StopingService 
SC \\%1 query %2 | FIND "STATE" | FIND "STOPPED" >NUL 
IF errorlevel 1 GOTO StopService 
 
:StopedService 
echo %2 on \\%1 is stopped 
GOTO:eof 
 
:SystemOffline 
echo Server \\%1 or service %2 is not accessible or is offline 
GOTO:eof 
 
:usage 
echo Will cause a remote service to STOP (if not already stopped). 
echo This script will waiting for the service to enter the stpped state if necessary. 
echo. 
echo %0 [system name] [service name] {reason} 
echo Example: %0 server1 MyService 
echo. 
echo For reason codes, run "sc stop" 
GOTO:eof 
