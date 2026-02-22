#!/bin/bash

basepath=$(cd "$(dirname "$0")" && pwd)
echo "$basepath"
cd "$basepath"

mkdir -p ../build_client_v3/ClientV3/public
mkdir -p ../build_client_v3/ClientV3/src/common
mkdir -p ../build_client_v3/ClientV3/src/modules
mkdir -p ../build_client_v3/ClientV3/src/dialogs
mkdir -p ../build_client_v3/ClientV3/src/directives
mkdir -p ../build_client_v3/ClientV3/dist
mkdir -p clientdist_v3

cp ../ClientV3/public/* ../build_client_v3/ClientV3/public/ 2>/dev/null || true
cp ../ClientV3/src/*.* ../build_client_v3/ClientV3/src/ 2>/dev/null || true
cp ../ClientV3/src/common/* ../build_client_v3/ClientV3/src/common/ 2>/dev/null || true
cp ../ClientV3/src/modules/* ../build_client_v3/ClientV3/src/modules/ 2>/dev/null || true
cp ../ClientV3/src/dialogs/* ../build_client_v3/ClientV3/src/dialogs/ 2>/dev/null || true
cp ../ClientV3/src/directives/* ../build_client_v3/ClientV3/src/directives/ 2>/dev/null || true
cp ../ClientV3/*.js ../build_client_v3/ClientV3/ 2>/dev/null || true
cp ../ClientV3/*.mjs ../build_client_v3/ClientV3/ 2>/dev/null || true
cp ../ClientV3/*.json ../build_client_v3/ClientV3/ 2>/dev/null || true
cp ../ClientV3/index.html ../build_client_v3/ClientV3/ 2>/dev/null || true

cd ..
git log -n 1 --format="%H" -- "ClientV3" > "build_client_v3/ClientV3/dist/_githash_client_vue3.txt"
git log -n 1 --format="%H" -- "ClientV3" > "Artifacts/clientdist_v3/_githash_client_vue3.txt"
CLIENTDIRHASH=$(cat "build_client_v3/ClientV3/dist/_githash_client_vue3.txt")

cat > build_client_v3/ClientV3/src/common/AMCGitHash.js <<EOF_GITHASH
export function getClientGitHash ()
{
  return "$CLIENTDIRHASH";
}
EOF_GITHASH

cd build_client_v3/ClientV3

TOOLBUILDDIR="../../build_clientdist_tools_v3"
mkdir -p "$TOOLBUILDDIR"
git rev-parse --verify --short HEAD > "$TOOLBUILDDIR/githash.txt"
git log -n 1 --format="%H" -- "ClientV3" > "$TOOLBUILDDIR/clientdirhash.txt"
cmake -S ../.. -B "$TOOLBUILDDIR"
cmake --build "$TOOLBUILDDIR" --target create_client_dist --config Release
cmake --build "$TOOLBUILDDIR" --target create_client_source --config Release

npm install
npm run build

cd ../..

cd build_client_v3/ClientV3

"$TOOLBUILDDIR/DevPackage/Framework/create_client_dist" dist ../../Artifacts/clientdist_v3/clientpackage_vue3.zip

"$TOOLBUILDDIR/DevPackage/Framework/create_client_source" . ../../Artifacts/clientdist_v3/clientsourcepackage_vue3.zip

echo
echo "Created packages in Artifacts/clientdist_v3/:"
ls -l ../../Artifacts/clientdist_v3
echo

exit 0
