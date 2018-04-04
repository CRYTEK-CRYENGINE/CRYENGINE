@ECHO OFF

REM Initialize values
SET CRYSELECT=%~dp0tools\CryVersionSelector\cryselect.exe
SET ENGINE=%~dp0cryengine.cryengine
SET PAUSE_ON_END=0

REM Install CryVersionSelector
@ECHO Installing CryVersionSelector...
IF EXIST "%CRYSELECT%" (
	"%CRYSELECT%" install
) ELSE (
	@ECHO Unable to find cryselect at "%CRYSELECT%"!
	SET PAUSE_ON_END=1
)

REM Register the engine
@ECHO Registering engine...
IF NOT "%PAUSE_ON_END%"=="1" (
	IF EXIST "%ENGINE%" (
		"%ENGINE%"
	) ELSE (
		@ECHO Unable to register engine at "%ENGINE%"!
		SET PAUSE_ON_END=1
	)
)

REM Pause at the end if an error was detected during execution.
IF "%PAUSE_ON_END%"=="1" (
	@ECHO An error occured while installing the engine!
	PAUSE
) ELSE (
	@ECHO Engine installed successful.
	PAUSE
)