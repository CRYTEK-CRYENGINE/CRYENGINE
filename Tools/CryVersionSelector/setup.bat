@ECHO OFF

REM Tested with Python35
REM Assumes Python 3.5 installed in c:\Python35

REM PyInstaller must be installed before
REM To Install PyInstaller run:
REM  c:\Python35\Scripts\pip.exe install PyInstaller

REM Delete the __pycache__ folder first, since this isn't always updated properly.
SET PYCACHE_DIR=%~dp0__pycache__\
IF EXIST "%PYCACHE_DIR%" (
	@ECHO Deleting __pycache__ folder...
	@RD /S /Q "%PYCACHE_DIR%"
	@ECHO Deleted cache.
)

REM Build CrySelect
c:\Python35\Scripts\PyInstaller.exe --clean --onefile --icon editor_icon16.ico cryselect.py
copy /Y dist\cryselect.exe cryselect.exe

REM Build CryRun
c:\Python35\Scripts\PyInstaller.exe --clean --onefile --icon editor_icon16.ico cryrun.py
copy /Y dist\cryrun.exe cryrun.exe
