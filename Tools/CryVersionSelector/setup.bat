@ECHO OFF

REM tested with Python37

REM install required modules
py -3 -m pip install PyInstaller pypiwin32

REM build CrySelect
py -3 -m PyInstaller cryselect.spec
copy /Y dist\cryselect.exe cryselect.exe

REM build CryRun
py -3 -m PyInstaller cryrun.spec
copy /Y dist\cryrun.exe cryrun.exe
