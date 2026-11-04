#!/bin/bash
set -e

basepath=$(cd "$(dirname "$0")" && pwd)
echo "$basepath"
cd "$basepath"

if [ -d "../build_client_svelte" ]; then
	echo "Error: build_client_svelte directory already exists. Please remove it before running this script."
	exit 1
fi

mkdir -p ../build_client_svelte
mkdir -p clientdist

# Copy the entire Svelte project (source only, no node_modules or dist)
echo "[$(date +%T)] Copying Svelte project sources..."
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

# AMCF_NPM_REGISTRY overrides the configured npm registry, e.g. https://registry.npmjs.org/ if a mirror is unreachable.
NPM_REGISTRY_OPTION=()
if [ -n "${AMCF_NPM_REGISTRY:-}" ]; then
	NPM_REGISTRY_OPTION=("--registry=${AMCF_NPM_REGISTRY}")
fi

echo "[$(date +%T)] Node $(node --version), npm $(npm --version), registry $(npm config get registry "${NPM_REGISTRY_OPTION[@]}")"

# --loglevel=http lists every package request and --foreground-scripts shows install scripts (e.g. svelte-kit sync),
# so a stalled download is visible. Audit and funding requests are skipped, as they can hang behind proxies.
echo "[$(date +%T)] Installing npm packages..."
npm install --no-audit --no-fund --loglevel=http --foreground-scripts "${NPM_REGISTRY_OPTION[@]}"

echo "[$(date +%T)] Building Svelte client..."
npm run build
echo "[$(date +%T)] Svelte client built."

cd ../..

# Build create_client_dist in its own lightweight build directory
CLIENTDIST_BUILDDIR="build_client_dist"
if [ ! -f "$CLIENTDIST_BUILDDIR/CMakeCache.txt" ]; then
	echo "Configuring create_client_dist tool..."
	mkdir -p "$CLIENTDIST_BUILDDIR"
	cmake -S BuildScripts/ClientDist -B "$CLIENTDIST_BUILDDIR"
fi
echo "[$(date +%T)] Building create_client_dist tool..."
cmake --build "$CLIENTDIST_BUILDDIR" --config Release

CLIENTDIST_EXE="$CLIENTDIST_BUILDDIR/create_client_dist"

# Package the build output into a client ZIP
echo "[$(date +%T)] Packaging client ZIP..."
"$CLIENTDIST_EXE" build_client_svelte/Client/dist Artifacts/clientdist/clientpackage_svelte.zip

echo
echo "Created svelte package in Artifacts/clientdist/:"
ls -l Artifacts/clientdist/clientpackage_svelte.zip
echo

exit 0
