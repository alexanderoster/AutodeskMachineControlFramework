@echo off

set basepath=%~dp0
echo %basepath%
cd %basepath%

mkdir ..\build_client\core
mkdir ..\build_client\core\common
mkdir ..\build_client\core\modules
mkdir ..\build_client\Client
mkdir ..\build_client\Client\public
mkdir ..\build_client\Client\src
mkdir ..\build_client\Client\src\modules
mkdir ..\build_client\Client\src\dialogs
mkdir ..\build_client\Client\dist

REM Shared core JS (framework-agnostic)
copy ..\Client\core\common\*.* ..\build_client\core\common
copy ..\Client\core\modules\*.* ..\build_client\core\modules

REM Vue 2 specific files
copy ..\Client\vue2\public\*.* ..\build_client\Client\public
copy ..\Client\vue2\src\*.* ..\build_client\Client\src
copy ..\Client\vue2\src\modules\*.* ..\build_client\Client\src\modules
copy ..\Client\vue2\src\dialogs\*.* ..\build_client\Client\src\dialogs
copy ..\Client\vue2\vue.config.js ..\build_client\Client
copy ..\Client\vue2\babel.config.js ..\build_client\Client
copy ..\Client\vue2\package.json ..\build_client\Client

cd ..
git log -n 1 --format="%%H" -- "Client/core" "Client/vue2" >"build_client\Client\dist\_githash_client_vue2.txt"
git log -n 1 --format="%%H" -- "Client/core" "Client/vue2" >"Artifacts\clientdist\_githash_client_vue2.txt"
SET /p CLIENTDIRHASH=<"build_client\Client\dist\_githash_client_vue2.txt"

echo export function getClientGitHash ()> build_client\core\common\AMCGitHash.js
echo {>> build_client\core\common\AMCGitHash.js
echo   return "%CLIENTDIRHASH%";>> build_client\core\common\AMCGitHash.js
echo }>> build_client\core\common\AMCGitHash.js

cd build_client\Client

set TOOLBUILDDIR=..\..\build_clientdist_tools
if not exist "%TOOLBUILDDIR%" (mkdir "%TOOLBUILDDIR%")
git rev-parse --verify --short HEAD >"%TOOLBUILDDIR%\githash.txt"
git log -n 1 --format="%%H" -- "Client/core" "Client/vue2" >"%TOOLBUILDDIR%\clientdirhash.txt"
cmake -S ..\.. -B "%TOOLBUILDDIR%"
cmake --build "%TOOLBUILDDIR%" --target create_client_dist --config Release
cmake --build "%TOOLBUILDDIR%" --target create_client_source --config Release

call npm install

REM Work around node-ipc crashing when os.networkInterfaces fails in sandboxed environments.
if exist "node_modules\\@achrinza\\node-ipc\\entities\\Defaults.js" (
	node -e "const fs=require('fs');const path='node_modules/@achrinza/node-ipc/entities/Defaults.js';let data=fs.readFileSync(path,'utf8');const original=data;const needle='const networkInterfaces = os.networkInterfaces();';const broken='let networkInterfaces = null;\\n    try {\\n        networkInterfaces = os.networkInterfaces();\\n    } catch (error) {\\n        networkInterfaces = null;\\n    }';const replacement='let networkInterfaces = null;\n    try {\n        networkInterfaces = os.networkInterfaces();\n    } catch (error) {\n        networkInterfaces = null;\n    }';if(data.includes(needle)){data=data.replace(needle,replacement);}else if(data.includes(broken)){data=data.replace(broken,replacement);}if(data!==original){fs.writeFileSync(path,data,'utf8');}"
)

call npm run build

cd ..\..\

cd build_client\Client

"%TOOLBUILDDIR%\DevPackage\Framework\create_client_dist.exe" dist ..\..\Artifacts\clientdist\clientpackage_vue2.zip 

"%TOOLBUILDDIR%\DevPackage\Framework\create_client_source.exe" . ..\..\Artifacts\clientdist\clientsourcepackage_vue2.zip 

if "%1" neq "NOPAUSE" (
	pause
)

exit 0
