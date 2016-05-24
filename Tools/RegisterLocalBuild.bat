@ECHO OFF

pushd ..

reg add "HKEY_CURRENT_USER\Software\Crytek\CryEngine" /f /v "local" /t REG_SZ /d "%cd%"

popd

ECHO This build has been registered as "local"
PAUSE