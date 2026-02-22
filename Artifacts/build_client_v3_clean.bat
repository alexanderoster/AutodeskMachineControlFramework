B@echo off

set basepath=%~dp0
echo %basepath%
cd %basepath%

mkdir ..\build_client_v3\ClientV3
mkdir ..\build_client_v3\ClientV3\public
mkdir ..\build_client_v3\ClientV3\src
mkdir ..\build_client_v3\ClientV3\src\common
mkdir ..\build_client_v3\ClientV3\src\modules
mkdir ..\build_client_v3\ClientV3\src\dialogs
mkdir ..\build_client_v3\ClientV3\src\directives
mkdir ..\build_client_v3\ClientV3\dist
mkdir clientdist_v3

copy ..\ClientV3\public\*.* ..\build_client_v3\ClientV3\public
copy ..\ClientV3\src\*.* ..\build_client_v3\ClientV3\src
copy ..\ClientV3\src\common\*.* ..\build_client_v3\ClientV3\src\common
copy ..\ClientV3\src\modules\*.* ..\build_client_v3\ClientV3\src\modules
copy ..\ClientV3\src\dialogs\*.* ..\build_client_v3\ClientV3\src\dialogs
copy ..\ClientV3\src\directives\*.* ..\build_client_v3\ClientV3\src\directives
copy ..\ClientV3\*.js ..\build_client_v3\ClientV3
copy ..\ClientV3\*.mjs ..\build_client_v3\ClientV3
copy ..\ClientV3\*.json ..\build_client_v3\ClientV3
copy ..\ClientV3\index.html ..\build_client_v3\ClientV3

cd ..
git log -n 1 --format="%%H" -- "ClientV3" >"build_client_v3\ClientV3\dist\_githash_client_vue3.txt"
git log -n 1 --format="%%H" -- "ClientV3" >"Artifacts\clientdist_v3\_githash_client_vue3.txt"
SET /p CLIENTDIRHASH=<"build_client_v3\ClientV3\dist\_githash_client_vue3.txt"

echo export function getClientGitHash ()> build_client_v3\ClientV3\src\common\AMCGitHash.js
echo {>> build_client_v3\ClientV3\src\common\AMCGitHash.js
echo   return "%CLIENTDIRHASH%";>> build_client_v3\ClientV3\src\common\AMCGitHash.js
echo }>> build_client_v3\ClientV3\src\common\AMCGitHash.js

cd build_client_v3\ClientV3

set TOOLBUILDDIR=..\..\build_clientdist_tools_v3
if not exist "%TOOLBUILDDIR%" (mkdir "%TOOLBUILDDIR%")
git rev-parse --verify --short HEAD >"%TOOLBUILDDIR%\githash.txt"
git log -n 1 --format="%%H" -- "ClientV3" >"%TOOLBUILDDIR%\clientdirhash.txt"
cmake -S ..\.. -B "%TOOLBUILDDIR%"
cmake --build "%TOOLBUILDDIR%" --target create_client_dist --config Release
cmake --build "%TOOLBUILDDIR%" --target create_client_source --config Release

call npm install
call npm run build

cd ..\..\

cd build_client_v3\ClientV3

"%TOOLBUILDDIR%\DevPackage\Framework\create_client_dist.exe" dist ..\..\Artifacts\clientdist_v3\clientpackage_vue3.zip

"%TOOLBUILDDIR%\DevPackage\Framework\create_client_source.exe" . ..\..\Artifacts\clientdist_v3\clientsourcepackage_vue3.zip

if "%1" neq "NOPAUSE" (
	pause
)

exit 0
