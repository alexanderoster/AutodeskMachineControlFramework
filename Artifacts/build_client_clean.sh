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
git log -n 1 --format="%H" -- "Client" > "build_client/Client/dist/_githash_client_vue2.txt"
git log -n 1 --format="%H" -- "Client" > "Artifacts/clientdist/_githash_client_vue2.txt"
CLIENTDIRHASH=$(cat "build_client/Client/dist/_githash_client_vue2.txt")

cat > build_client/Client/src/AMCGitHash.js <<EOF
export function getClientGitHash ()
{
  return "$CLIENTDIRHASH";
}
EOF




cd build_client/Client

TOOLBUILDDIR="../../build_clientdist_tools"
mkdir -p "$TOOLBUILDDIR"
git rev-parse --verify --short HEAD > "$TOOLBUILDDIR/githash.txt"
git log -n 1 --format="%H" -- "Client" > "$TOOLBUILDDIR/clientdirhash.txt"
cmake -S ../.. -B "$TOOLBUILDDIR"
cmake --build "$TOOLBUILDDIR" --target create_client_dist --config Release
cmake --build "$TOOLBUILDDIR" --target create_client_source --config Release


npm install

# Work around node-ipc crashing when os.networkInterfaces fails in sandboxed environments.
NODE_IPC_DEFAULTS="node_modules/@achrinza/node-ipc/entities/Defaults.js"
if [ -f "$NODE_IPC_DEFAULTS" ]; then
  python3 - "$NODE_IPC_DEFAULTS" <<'PY'
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = handle.read()
original_data = data

needle = "const networkInterfaces = os.networkInterfaces();"
broken = (
    "let networkInterfaces = null;\\\\n"
    "    try {\\\\n"
    "        networkInterfaces = os.networkInterfaces();\\\\n"
    "    } catch (error) {\\\\n"
    "        networkInterfaces = null;\\\\n"
    "    }"
)
replacement = (
    "let networkInterfaces = null;\n"
    "    try {\n"
    "        networkInterfaces = os.networkInterfaces();\n"
    "    } catch (error) {\n"
    "        networkInterfaces = null;\n"
    "    }"
)
if needle in data:
    data = data.replace(needle, replacement, 1)
elif broken in data:
    data = data.replace(broken, replacement, 1)

if data != original_data:
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(data)
PY
fi

npm run build

cd ../..

cd build_client/Client

"$TOOLBUILDDIR/DevPackage/Framework/create_client_dist" dist ../../Artifacts/clientdist/clientpackage_vue2.zip

"$TOOLBUILDDIR/DevPackage/Framework/create_client_source" . ../../Artifacts/clientdist/clientsourcepackage_vue2.zip

echo
echo "Created packages in Artifacts/clientdist/:"
ls -l ../../Artifacts/clientdist
echo

exit 0
