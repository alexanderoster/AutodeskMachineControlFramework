@echo off

set basepath=%~dp0
echo %basepath%
cd %basepath%

if exist "..\build_client_svelte" (
	echo Error: build_client_svelte directory already exists. Please remove it before running this script.
	exit 1
)

if not exist "..\build_client_svelte" (mkdir "..\build_client_svelte")
if not exist "..\build_client_svelte\Client" (mkdir "..\build_client_svelte\Client")
if not exist "..\build_client_svelte\core" (mkdir "..\build_client_svelte\core")
if not exist "..\build_client_svelte\core\common" (mkdir "..\build_client_svelte\core\common")
if not exist "..\build_client_svelte\core\modules" (mkdir "..\build_client_svelte\core\modules")
if not exist "..\build_client_svelte\core\theme" (mkdir "..\build_client_svelte\core\theme")
if not exist "clientdist" (mkdir "clientdist")

REM Copy Svelte project files (exclude build artifacts)
echo [%TIME%] Copying Svelte project sources...
robocopy ..\Client\svelte ..\build_client_svelte\Client /E /XD node_modules dist .svelte-kit /NFL /NDL /NJH /NJS /NC /NS /NP
if %ERRORLEVEL% GEQ 8 (
	echo ROBOCOPY failed with error %ERRORLEVEL%
	goto :error
)

REM Shared core JS
echo [%TIME%] Copying shared core JS...
copy ..\Client\core\common\*.* ..\build_client_svelte\core\common
copy ..\Client\core\modules\*.* ..\build_client_svelte\core\modules
copy ..\Client\core\theme\*.* ..\build_client_svelte\core\theme

cd ..
echo [%TIME%] Writing client git hash...
git log -n 1 --format="%%H" -- "Client/core" "Client/svelte" >"build_client_svelte\Client\_githash_client_svelte.txt"
git log -n 1 --format="%%H" -- "Client/core" "Client/svelte" >"Artifacts\clientdist\_githash_client_svelte.txt"
SET /p CLIENTDIRHASH=<"build_client_svelte\Client\_githash_client_svelte.txt"

echo export function getClientGitHash ()> build_client_svelte\core\common\AMCGitHash.js
echo {>> build_client_svelte\core\common\AMCGitHash.js
echo   return "%CLIENTDIRHASH%";>> build_client_svelte\core\common\AMCGitHash.js
echo }>> build_client_svelte\core\common\AMCGitHash.js

cd build_client_svelte\Client

REM AMCF_NPM_REGISTRY overrides the configured npm registry, e.g. https://registry.npmjs.org/ if a mirror is unreachable.
set NPM_REGISTRY_OPTION=
if defined AMCF_NPM_REGISTRY set NPM_REGISTRY_OPTION=--registry=%AMCF_NPM_REGISTRY%

echo [%TIME%] Node and npm versions:
call node --version
call npm --version
echo npm registry:
call npm config get registry %NPM_REGISTRY_OPTION%

REM --loglevel=http lists every package request and --foreground-scripts shows install scripts (e.g. svelte-kit sync),
REM so a stalled download is visible. Audit and funding requests are skipped, as they can hang behind proxies.
echo [%TIME%] Installing npm packages...
call npm install --no-audit --no-fund --loglevel=http --foreground-scripts %NPM_REGISTRY_OPTION%
if %ERRORLEVEL% NEQ 0 goto :error

echo [%TIME%] Building Svelte client...
call npm run build
if %ERRORLEVEL% NEQ 0 goto :error
echo [%TIME%] Svelte client built.

cd ..\..

REM Build create_client_dist in its own lightweight build directory
set CLIENTDIST_BUILDDIR=build_client_dist
if not exist "%CLIENTDIST_BUILDDIR%\CMakeCache.txt" (
	echo [%TIME%] Configuring create_client_dist tool...
	if not exist "%CLIENTDIST_BUILDDIR%" (mkdir "%CLIENTDIST_BUILDDIR%")
	cmake -S BuildScripts\ClientDist -B "%CLIENTDIST_BUILDDIR%"
	if %ERRORLEVEL% NEQ 0 goto :error
)
echo [%TIME%] Building create_client_dist tool...
cmake --build "%CLIENTDIST_BUILDDIR%" --config Release
if %ERRORLEVEL% NEQ 0 goto :error

if exist "%CLIENTDIST_BUILDDIR%\Release\create_client_dist.exe" (
	set CLIENTDIST_EXE=%CLIENTDIST_BUILDDIR%\Release\create_client_dist.exe
) else (
	set CLIENTDIST_EXE=%CLIENTDIST_BUILDDIR%\create_client_dist.exe
)

REM Package the build output into a client ZIP
echo [%TIME%] Packaging client ZIP...
"%CLIENTDIST_EXE%" build_client_svelte\Client\dist Artifacts\clientdist\clientpackage_svelte.zip
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
