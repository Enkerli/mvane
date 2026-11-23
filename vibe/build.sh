#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# mvane yard's VIBE FM firmware: the SDK's tools/build.sh (its flags, its link, its package), with the include paths
# Clipper's sources need (app/clipper and its src, gen, hal: they include each other by name, as on the FM-1).
#   sh vibe/build.sh [SDK]   (SDK: the VIBE FM SDK's folder, by default ../vibe-fm-sdk-ref/sdk) -> SDK/build/mvane-yard.zip
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$(cd "${1:-$ROOT/../vibe-fm-sdk-ref/sdk}" && pwd)"
DIR="$ROOT/vibe/mvane-yard"; ID=mvane-yard
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
rm -f "build/$ID.zip" "build/$ID.png"
[ -f "$DIR/icon.png" ] && cp "$DIR/icon.png" "build/$ID.png"
(cd build && zip -q -r "$ID.zip" "$ID.wasm" "$ID.json" $( [ -f "$ID.png" ] && echo "$ID.png" ))
echo "packed build/$ID.zip – drop it onto the VIBE FM VST window"
