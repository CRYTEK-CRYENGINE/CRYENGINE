@echo off

REM Tested with Python37, assumes installed in C:\Python37
REM Can't use p3 -m PyInstaller because it doesn't pick up changes....

REM PyInstaller must be installed before
REM To Install PyInstaller run:
REM 	py -3 -m pip install PyInstaller pypiwin32

SET sdir=%~dp0

REM Build cry_cmake.exe
echo PyInstalling EXE.....
echo.
C:\Python37\Scripts\PyInstaller.exe --distpath=%sdir%dist --workpath=%sdir%build %sdir%cry_cmake.spec &&^
echo. && echo Copying cry_cmake.exe to ENGINE_ROOT..... &&^
copy /Y %sdir%dist\cry_cmake.exe %sdir%..\..\..\cry_cmake.exe

REM Pause unless there's any arguments.
IF "%1" == "" ( echo. && PAUSE )