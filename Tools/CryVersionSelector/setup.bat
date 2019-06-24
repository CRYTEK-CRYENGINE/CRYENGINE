@ECHO OFF

REM tested with Python37

REM install required modules
py -3 -m pip install PyInstaller pypiwin32

REM clean temp
rmdir bin /s /q
rmdir dist /s /q
rmdir build /s /q

REM build CrySelect
py -3 -m PyInstaller cryselect.spec

REM build CryRun
py -3 -m PyInstaller cryrun.spec

REM deploy
xcopy /Y /S dist\*.* bin\