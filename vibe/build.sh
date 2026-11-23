#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# mvane yard's VIBE FM firmware: the SDK's tools/build.sh (its flags, its link, its package), with the include paths
# Clipper's sources need (app/clipper and its src, gen, hal: they include each other by name, as on the FM-1).
# VIBE_VERSION: the version on the card (a release's); ZIG: the compiler (the SDK names 0.13.0).
#   sh vibe/build.sh [SDK]   (SDK: the VIBE FM SDK's folder, by default ../vibe-fm-sdk-ref/sdk) -> SDK/build/mvane-yard-native.zip
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$(cd "${1:-$ROOT/../vibe-fm-sdk-ref/sdk}" && pwd)"
DIR="$ROOT/vibe/mvane-yard-native"; ID=mvane-yard-native
ZIG="${ZIG:-zig}"
command -v "$ZIG" >/dev/null 2>&1 || [ -x "$ZIG" ] || { echo "zig not found (the SDK's README: Zig 0.13.0; ZIG=/path/to/zig)"; exit 1; }
cd "$SDK"
T="build/obj/$ID"; rm -rf "$T"; mkdir -p "$T"
C="$ROOT/app/clipper"
for f in "$DIR"/*.c sdk/src/vibe_sdk.c sdk/src/vibe_font.c; do
  "$ZIG" cc -target wasm32-freestanding -O2 -ffast-math -fno-builtin-memset -matomics -mbulk-memory \
    -Isdk/include -Isdk/src -I"$DIR/freestanding" -I"$C" -I"$C/src" -I"$C/gen" -I"$C/hal" \
    -Wall -Wno-unused-function -c -o "$T/$(basename "$f" .c).o" "$f"
done
"$ZIG" wasm-ld --no-entry --export-dynamic --export=__heap_base --export=__data_end \
  --shared-memory --max-memory=67108864 -z stack-size=262144 -o "build/$ID.wasm" "$T"/*.o
echo "built build/$ID.wasm ($(( $(wc -c < "build/$ID.wasm") / 1024 )) KB)"
cp "$DIR/firmware.json" "build/$ID.json"
if [ -n "${VIBE_VERSION:-}" ]; then            # (a release: its version on the card)
  python3 -c "import json,sys; p=sys.argv[1]; d=json.load(open(p)); d['version']=sys.argv[2]; json.dump(d,open(p,'w'))" "build/$ID.json" "$VIBE_VERSION"
fi
rm -f "build/$ID.zip" "build/$ID.png"
[ -f "$DIR/icon.png" ] && cp "$DIR/icon.png" "build/$ID.png"
if command -v zip >/dev/null 2>&1; then
  (cd build && zip -q -r "$ID.zip" "$ID.wasm" "$ID.json" $( [ -f "$ID.png" ] && echo "$ID.png" ))
else                                           # (no zip: Python's, the same files at the top)
  (cd build && python3 -c "import zipfile,sys,os; z=zipfile.ZipFile(sys.argv[1],'w',zipfile.ZIP_DEFLATED); [z.write(f) for f in sys.argv[2:] if os.path.exists(f)]" "$ID.zip" "$ID.wasm" "$ID.json" "$ID.png")
fi
echo "packed build/$ID.zip – drop it onto the VIBE FM VST window"
