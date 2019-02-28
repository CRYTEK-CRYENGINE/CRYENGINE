@ECHO OFF
setlocal

set solution_path=solutions_cmake\win32
if EXIST "%~dp0\..\..\%solution_path%\CMakeCache.txt" goto :RunCMake

echo Detecting available compilers...
set choices=
call :VisitCompilers :PrintCompiler

IF %i% EQU 0 (
	ECHO No VS installations found!
	PAUSE
	goto :eof
)
set compiler_choice=1
if %i% GTR 1 call :ChooseCompiler
if %compiler_choice% EQU 0 goto NoCompilerSelected
call :VisitCompilers :GenerateCMake
goto :eof

:NoCompilerSelected
echo Nothing selected, aborting.
pause
goto :eof

:ChooseCompiler
choice /C %choices% /N /M "Choose VS version: "
set compiler_choice=%ERRORLEVEL%
goto :eof

REM VisitCompilers(func)
REM Call to execute func(generator_name) for every compiler found. %i% is set to the 1-based index of the compiler.
REM To add more compilers, add lines of the form "call :CheckCompiler %1 <registry_key> <generator_name> <display_name>"
:VisitCompilers
set i=0
call :CheckCompiler %1 "HKEY_CLASSES_ROOT\WDExpress.DTE.14.0" "Visual Studio 14 2015" "Visual Studio 2015 Express"
call :CheckCompiler %1 "HKEY_CLASSES_ROOT\VisualStudio.DTE.14.0" "Visual Studio 14 2015" "Visual Studio 2015"
call :CheckCompiler %1 "HKEY_CLASSES_ROOT\VisualStudio.DTE.15.0" "Visual Studio 15 2017" "Visual Studio 2017"
goto :eof

REM CheckCompiler(func, reg_key, generator_name)
REM If reg_key is found in the registry, calls func(generator_name, display_name)
:CheckCompiler
reg query %2 >> nul 2>&1
if ERRORLEVEL 1 goto :eof
set /a i+=1
call %1 %3 %4
goto :eof

REM PrintCompiler(generator_name, display_name)
REM Prints the name of the generator given, along with its index.
:PrintCompiler
echo %i%. %2
set choices=%choices%%i%
goto :eof

REM GenerateCMake(generator_name, display_name)
REM Runs CMake with a generator matching the specified index.
:GenerateCMake
if %i% EQU %compiler_choice% call :RunCMake %1 %2
goto :eof

REM RunCMake(generator_name, display_name)
REM Runs CMake to generate a solution. If specified, uses generator_name as the generator.
:RunCMake
pushd "%~dp0\..\.."
mkdir %solution_path% > nul 2>&1
cd %solution_path%
if "%compiler_choice%" EQU "" (
..\..\Tools\CMake\Win32\bin\cmake -D CMAKE_TOOLCHAIN_FILE=Tools\CMake\toolchain\windows\WindowsPC-MSVC.cmake ..\..
) ELSE (
echo Generating CMake solution for %2
..\..\Tools\CMake\Win32\bin\cmake -G %1 -D CMAKE_TOOLCHAIN_FILE=Tools\CMake\toolchain\windows\WindowsPC-MSVC.cmake ..\..
)
if ERRORLEVEL 1 (
PAUSE
) ELSE (
ECHO Starting cmake-gui...
start ..\..\Tools\CMake\Win32\bin\cmake-gui .
)
popd
goto :eof

