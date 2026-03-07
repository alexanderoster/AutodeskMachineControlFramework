#!/bin/bash

basepath=$(cd "$(dirname "$0")" && pwd)
echo "$basepath"
cd "$basepath"

mkdir -p ../build_client_svelte

# Copy the entire Svelte project (source only, no node_modules or dist)
rsync -a --exclude='node_modules' --exclude='dist' --exclude='.svelte-kit' ../Client/svelte/ ../build_client_svelte/Client/
cp -r ../Client/core ../build_client_svelte/core

cd ..
git log -n 1 --format="%H" -- "Client/core" "Client/svelte" > "build_client_svelte/Client/_githash_client_svelte.txt"
git log -n 1 --format="%H" -- "Client/core" "Client/svelte" > "Artifacts/clientdist/_githash_client_svelte.txt"

cd build_client_svelte/Client

TOOLBUILDDIR="../../build_clientdist_tools"
mkdir -p "$TOOLBUILDDIR"
git rev-parse --verify --short HEAD > "$TOOLBUILDDIR/githash.txt"
cmake -S ../.. -B "$TOOLBUILDDIR"
cmake --build "$TOOLBUILDDIR" --target create_client_dist --config Release

npm install
npm run build

cd ../..

cd build_client_svelte/Client

"$TOOLBUILDDIR/DevPackage/Framework/create_client_dist" dist ../../Artifacts/clientdist/clientpackage_svelte.zip

echo
echo "Created svelte package in Artifacts/clientdist/:"
ls -l ../../Artifacts/clientdist/clientpackage_svelte.zip
echo

exit 0
