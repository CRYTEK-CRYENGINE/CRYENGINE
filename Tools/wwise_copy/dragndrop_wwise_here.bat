@echo off

cd %~dp0
SET WWISEDIR="..\..\Code\SDKs\Audio\wwise"

IF NOT EXIST %WWISEDIR% GOTO NOCURRENTINSTALL
    echo Removing current installation...
    rmdir /q /s %WWISEDIR%
:NOCURRENTINSTALL

REM Create directory
mkdir %WWISEDIR%

REM Copy SDK inside
echo Copying files...
xcopy %1\SDK %WWISEDIR%\SDK /E /I /Q /H /K

pause
