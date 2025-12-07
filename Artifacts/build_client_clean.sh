#!/bin/bash

basepath=$(cd "$(dirname "$0")" && pwd)
echo "$basepath"
cd "$basepath"

mkdir -p ../build_client/Client/public
mkdir -p ../build_client/Client/src/common
mkdir -p ../build_client/Client/src/modules
mkdir -p ../build_client/Client/src/dialogs
mkdir -p ../build_client/Client/dist

cp ../Client/public/* ../build_client/Client/public/ 2>/dev/null || true
cp ../Client/src/*.* ../build_client/Client/src/ 2>/dev/null || true
cp ../Client/src/common/* ../build_client/Client/src/common/ 2>/dev/null || true
cp ../Client/src/modules/* ../build_client/Client/src/modules/ 2>/dev/null || true
cp ../Client/src/dialogs/* ../build_client/Client/src/dialogs/ 2>/dev/null || true
cp ../Client/*.js ../build_client/Client/ 2>/dev/null || true
cp ../Client/*.json ../build_client/Client/ 2>/dev/null || true

cd ..
git log -n 1 --format="%H" -- "Client" > "build_client/Client/dist/_githash_client.txt"
git log -n 1 --format="%H" -- "Client" > "Artifacts/clientdist/_githash_client.txt"
CLIENTDIRHASH=$(cat "build_client/Client/dist/_githash_client.txt")

cat > build_client/Client/src/AMCGitHash.js <<EOF
export function getClientGitHash ()
{
  return "$CLIENTDIRHASH";
}
EOF

cd build_client/Client

npm install
npm run build

cd ../..

cd build_client/Client

go run ../../BuildScripts/createClientDist.go dist ../../Artifacts/clientdist/clientpackage.zip

go run ../../BuildScripts/createClientSource.go . ../../Artifacts/clientdist/clientsourcepackage.zip

echo
echo "Created packages in Artifacts/clientdist/:"
ls -l ../../Artifacts/clientdist
echo

exit 0
