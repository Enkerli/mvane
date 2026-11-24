#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Every check, as CI and the release builder run them: the core on the desktop, the firmware in the host simulator,
# the file transfer (tools/yard.py against the simulator), the page's conversions. From the repository's root.
set -eu
cd "$(dirname "$0")/.."
python3 tools/sync_check.py       # the FM-1 and the native port agree (ARP modes, YARD values, DAW numbers)
make check
make synth-cmp
make clp-split
make clp-race
make yard-race
./build.sh --host
app/build/host/mvane-yard_host app/tests/basic.txt app/build/host > build/host.log
app/build/host/mvane-yard_host app/tests/migrate.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/queue.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/combos.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/yard.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/glide.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/scale.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/transpose.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/overdub.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/arp.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/dub_ch.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/breath.txt app/build/host >> build/host.log
app/build/host/mvane-yard_host app/tests/onset.txt app/build/host >> build/host.log
if grep -q '^FAIL' build/host.log; then grep '^FAIL' build/host.log; exit 1; fi
python3 tools/yard_test.py
node web/take_test.mjs
node web/ysb_test.mjs
node web/zip_test.mjs
echo "run_tests: all checks passed"
