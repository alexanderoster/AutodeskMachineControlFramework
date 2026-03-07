@echo off

set basepath=%~dp0
echo %basepath%
cd %basepath%

if not exist "..\build_client_svelte" (mkdir "..\build_client_svelte")
if not exist "..\build_client_svelte\Client" (mkdir "..\build_client_svelte\Client")
if not exist "..\build_client_svelte\core" (mkdir "..\build_client_svelte\core")
if not exist "..\build_client_svelte\core\common" (mkdir "..\build_client_svelte\core\common")
if not exist "..\build_client_svelte\core\modules" (mkdir "..\build_client_svelte\core\modules")

REM Copy Svelte project files
xcopy ..\Client\svelte\*.* ..\build_client_svelte\Client\ /E /Y /EXCLUDE:svelte_exclude.txt

REM Shared core JS
copy ..\Client\core\common\*.* ..\build_client_svelte\core\common
copy ..\Client\core\modules\*.* ..\build_client_svelte\core\modules

cd ..
git log -n 1 --format="%%H" -- "Client/core" "Client/svelte" >"build_client_svelte\Client\_githash_client_svelte.txt"
git log -n 1 --format="%%H" -- "Client/core" "Client/svelte" >"Artifacts\clientdist\_githash_client_svelte.txt"

cd build_client_svelte\Client

set TOOLBUILDDIR=..\..\build_clientdist_tools
if not exist "%TOOLBUILDDIR%" (mkdir "%TOOLBUILDDIR%")
git rev-parse --verify --short HEAD >"%TOOLBUILDDIR%\githash.txt"
cmake -S ..\.. -B "%TOOLBUILDDIR%"
cmake --build "%TOOLBUILDDIR%" --target create_client_dist --config Release

call npm install
call npm run build

cd ..\..\

cd build_client_svelte\Client

"%TOOLBUILDDIR%\DevPackage\Framework\create_client_dist.exe" dist ..\..\Artifacts\clientdist\clientpackage_svelte.zip

if "%1" neq "NOPAUSE" (
	pause
)

exit 0
