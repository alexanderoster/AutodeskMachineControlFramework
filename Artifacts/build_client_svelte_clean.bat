@echo off

set basepath=%~dp0
echo %basepath%
cd %basepath%

if not exist "..\build_client_svelte" (mkdir "..\build_client_svelte")
if not exist "..\build_client_svelte\Client" (mkdir "..\build_client_svelte\Client")
if not exist "..\build_client_svelte\core" (mkdir "..\build_client_svelte\core")
if not exist "..\build_client_svelte\core\common" (mkdir "..\build_client_svelte\core\common")
if not exist "..\build_client_svelte\core\modules" (mkdir "..\build_client_svelte\core\modules")
if not exist "..\build_client_svelte\core\theme" (mkdir "..\build_client_svelte\core\theme")
if not exist "clientdist" (mkdir "clientdist")

REM Copy Svelte project files (exclude build artifacts)
robocopy ..\Client\svelte ..\build_client_svelte\Client /E /XD node_modules dist .svelte-kit /NFL /NDL /NJH /NJS /NC /NS /NP
if %ERRORLEVEL% GEQ 8 (
	echo ROBOCOPY failed with error %ERRORLEVEL%
	goto :error
)

REM Shared core JS
copy ..\Client\core\common\*.* ..\build_client_svelte\core\common
copy ..\Client\core\modules\*.* ..\build_client_svelte\core\modules
copy ..\Client\core\theme\*.* ..\build_client_svelte\core\theme

cd ..
git log -n 1 --format="%%H" -- "Client/core" "Client/svelte" >"build_client_svelte\Client\_githash_client_svelte.txt"
git log -n 1 --format="%%H" -- "Client/core" "Client/svelte" >"Artifacts\clientdist\_githash_client_svelte.txt"
SET /p CLIENTDIRHASH=<"build_client_svelte\Client\_githash_client_svelte.txt"

echo export function getClientGitHash ()> build_client_svelte\core\common\AMCGitHash.js
echo {>> build_client_svelte\core\common\AMCGitHash.js
echo   return "%CLIENTDIRHASH%";>> build_client_svelte\core\common\AMCGitHash.js
echo }>> build_client_svelte\core\common\AMCGitHash.js

cd build_client_svelte\Client

call npm install
if %ERRORLEVEL% NEQ 0 goto :error

call npm run build
if %ERRORLEVEL% NEQ 0 goto :error

cd ..\..\

REM Package the build output into a client ZIP
python BuildScripts\createClientDist.py build_client_svelte\Client\dist Artifacts\clientdist\clientpackage_svelte.zip
if %ERRORLEVEL% NEQ 0 goto :error

echo.
echo Build complete!
echo.

if "%1" neq "NOPAUSE" (
	pause
)

exit 0

:error
echo.
echo Build failed!
echo.
if "%1" neq "NOPAUSE" (
	pause
)
exit 1
