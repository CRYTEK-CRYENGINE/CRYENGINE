@echo off

REM Tested with Python37, assumes installed in C:\Python37
REM Can't use p3 -m PyInstaller because it doesn't pick up changes....

REM PyInstaller must be installed before
REM To Install PyInstaller run:
REM 	py -3 -m pip install PyInstaller pypiwin32

SET sdir=%~dp0

REM Build CrySelect
echo PyInstalling CrySelect EXE.....
echo.
C:\Python37\Scripts\PyInstaller.exe --distpath=%sdir%dist --workpath=%sdir%build %sdir%cryselect.spec &&^
echo. && echo Copying cryselect.exe..... &&^
copy /Y %sdir%dist\cryselect.exe %sdir%cryselect.exe

REM Pause unless there's any arguments.
IF "%1" == "" ( echo. && PAUSE )

REM Build CryRun
echo PyInstalling CryRun EXE.....
echo.
py -3 -m PyInstaller --distpath=%sdir%dist --workpath=%sdir%build %sdir%cryrun.spec &&^
echo. && echo Copying cryrun.exe..... &&^
copy /Y %sdir%dist\cryrun.exe %sdir%cryrun.exe

REM Pause unless there's any arguments.
IF "%1" == "" ( echo. && PAUSE )

REM Installing new CrySelect
echo Installing new CrySelect.
%sdir%\cryselect.exe install

REM Pause unless there's any arguments.
IF "%1" == "" ( echo Installed successfully && PAUSE )
