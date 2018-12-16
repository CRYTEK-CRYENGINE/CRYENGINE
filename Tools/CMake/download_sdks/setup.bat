@echo off

REM Tested with Python37, assumes installed in C:\Python37
REM Can't use p3 -m PyInstaller because it doesn't pick up changes....

REM PyInstaller must be installed before
REM To Install PyInstaller run:
REM 	py -3 -m pip install PyInstaller pypiwin32

SET sdir=%~dp0

REM Build download_sdk.exe
echo PyInstalling EXE.....
echo.
C:\Python37\Scripts\PyInstaller.exe --distpath=%sdir%dist --workpath=%sdir%build %sdir%download_sdks.spec &&^
echo. && echo Copying download_sdks.exe..... &&^
copy /Y %sdir%dist\download_sdks.exe %sdir%download_sdks.exe

REM Pause unless there's any arguments.
IF "%1" == "" ( echo. && PAUSE )