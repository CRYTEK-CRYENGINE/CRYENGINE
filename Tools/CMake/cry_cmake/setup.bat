REM Tested with Python35
REM Assumes Python 3.5 installed in c:\Python35

REM PyInstaller must be installed before
REM To Install PyInstaller run:
REM  c:\Python35\Scripts\pip.exe install PyInstaller

REM Build CrySelect
c:\Python35\Scripts\PyInstaller.exe cry_cmake.spec
copy /Y dist\cry_cmake.exe ..\..\..\cry_cmake.exe
