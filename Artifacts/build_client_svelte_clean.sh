#!/bin/bash
set -e

basepath=$(cd "$(dirname "$0")" && pwd)
echo "$basepath"
cd "$basepath"

mkdir -p ../build_client_svelte
mkdir -p clientdist

# Copy the entire Svelte project (source only, no node_modules or dist)
rsync -a --exclude='node_modules' --exclude='dist' --exclude='.svelte-kit' ../Client/svelte/ ../build_client_svelte/Client/

# Shared core JS (common, modules, theme)
cp -r ../Client/core ../build_client_svelte/core

cd ..
git log -n 1 --format="%H" -- "Client/core" "Client/svelte" > "build_client_svelte/Client/_githash_client_svelte.txt"
git log -n 1 --format="%H" -- "Client/core" "Client/svelte" > "Artifacts/clientdist/_githash_client_svelte.txt"
CLIENTDIRHASH=$(cat "build_client_svelte/Client/_githash_client_svelte.txt")

cat > build_client_svelte/core/common/AMCGitHash.js <<HASHEOF
export function getClientGitHash ()
{
  return "${CLIENTDIRHASH}";
}
HASHEOF

cd build_client_svelte/Client

npm install
npm run build

cd ../..

# Build create_client_dist from the existing cmake tree if needed
CLIENTDIST_EXE="build_linux64/DevPackage/Framework/create_client_dist"
if [ ! -f "$CLIENTDIST_EXE" ]; then
	echo "Building create_client_dist tool..."
	cmake --build build_linux64 --target create_client_dist --config Release
fi

# Package the build output into a client ZIP
"$CLIENTDIST_EXE" build_client_svelte/Client/dist Artifacts/clientdist/clientpackage_svelte.zip

echo
echo "Created svelte package in Artifacts/clientdist/:"
ls -l Artifacts/clientdist/clientpackage_svelte.zip
echo

exit 0
