@echo off
PUSHD ..\..\..\
choice /n /m "Attempt checkout to default perforce changelist (Y/N)?"
set OPTIONS=-n
if NOT ERRORLEVEL 2 set OPTIONS=-c
%~dp0\waf2cmake.exe %OPTIONS% %*
pause
POPD
