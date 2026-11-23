#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The native VIBE FM firmware on a Linux x86-64 runner (CI, the release): Zig 0.13.0 (the SDK's) and the VIBE FM SDK,
# each at a fixed version, checked; then vibe/build.sh, the SDK's vibe-check, and the package The Firmware Hub takes
# for its native card: mvane-yard-native.zip (mvane-yard-native.wasm, .json, .png at the top), the .wasm
# beside it.   sh vibe/ci.sh OUTDIR [VERSION]
set -eu
OUT="$1"; VERSION="${2:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T="${RUNNER_TEMP:-/tmp}/mvane-vibe"; mkdir -p "$T" "$OUT"
ZIG_URL=https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz
ZIG_SHA=d45312e61ebcc48032b77bc4cf7fd6915c11fa16e4aad116b66c9468211230ea
SDK_URL=https://github.com/Vibe-FM-SDK/vibe-fm-sdk.git
SDK_REV=c6c435566928950239d8d584bd6dc44d9316472e   # SDK 28
if [ ! -x "$T/zig-linux-x86_64-0.13.0/zig" ]; then
  curl -fsSL -o "$T/zig.tar.xz" "$ZIG_URL"
  echo "$ZIG_SHA  $T/zig.tar.xz" | sha256sum -c -
  tar -xJf "$T/zig.tar.xz" -C "$T"
fi
if [ ! -d "$T/sdk" ]; then
  git clone -q "$SDK_URL" "$T/sdk"
fi
git -C "$T/sdk" checkout -q "$SDK_REV"
ZIG="$T/zig-linux-x86_64-0.13.0/zig" VIBE_VERSION="$VERSION" sh "$ROOT/vibe/build.sh" "$T/sdk"
(cd "$T/sdk" && node tools/vibe-check.mjs build/mvane-yard-native.wasm | tail -1 | grep -q '^PASSED')
cp "$T/sdk/build/mvane-yard-native.zip" "$OUT/mvane-yard-native.zip"
cp "$T/sdk/build/mvane-yard-native.wasm" "$OUT/mvane-yard-native.wasm"
echo "vibe/ci.sh: $OUT/mvane-yard-native.zip, $OUT/mvane-yard-native.wasm (vibe-check PASSED)"
