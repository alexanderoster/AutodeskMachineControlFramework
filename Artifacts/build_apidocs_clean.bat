@echo off

set basepath=%~dp0
echo %basepath%
cd %basepath%

mkdir ..\build_apidocs

copy ..\OpenAPI\swagger*.* ..\build_apidocs
copy ..\OpenAPI\index.css ..\build_apidocs
copy ..\OpenAPI\index.html ..\build_apidocs
copy ..\OpenAPI\favicon.png ..\build_apidocs
copy ..\OpenAPI\oauth2-redirect.html ..\build_apidocs
copy ..\OpenAPI\AMCF_OpenAPI.json ..\build_apidocs

cd ..

git log -n 1 --format="%%H" -- "OpenAPI" >"build_apidocs\_githash_apidocs.txt"
git log -n 1 --format="%%H" -- "OpenAPI" >"Artifacts\apidocsdist\_githash_apidocs.txt"

go run BuildScripts\createClientDist.go build_apidocs Artifacts\apidocsdist\apidocspackage.zip 

if "%1" neq "NOPAUSE" (
	pause
)

exit 0
