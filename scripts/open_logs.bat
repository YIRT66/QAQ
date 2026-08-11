@echo off
setlocal EnableExtensions
set "LOGDIR=%LOCALAPPDATA%\EvolveMusic\EvolveMusic\logs"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
start "EvolveMusic Logs" explorer.exe "%LOGDIR%"
exit /b 0
