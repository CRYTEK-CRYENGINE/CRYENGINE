@ECHO OFF

REM Tested with Python35
REM Assumes Python 3.5 installed in c:\Python35

REM PyInstaller must be installed before
REM To Install PyInstaller run:
REM  c:\Python35\Scripts\pip.exe install PyInstaller

REM Build CrySelect
c:\Python35\Scripts\PyInstaller.exe cryselect.spec
copy /Y dist\cryselect.exe cryselect.exe

REM Build CryRun
c:\Python35\Scripts\PyInstaller.exe cryrun.spec
copy /Y dist\cryrun.exe cryrun.exe

