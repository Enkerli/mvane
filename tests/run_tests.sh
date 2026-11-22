#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Every check, as CI and the release builder run them: the core on the desktop, the firmware in the host simulator,
# the file transfer (tools/yard.py against the simulator), the page's conversions. From the repository's root.
set -eu
cd "$(dirname "$0")/.."
make check
./build.sh --host
app/build/host/mvane-yard_host app/tests/basic.txt app/build/host > build/host.log
app/build/host/mvane-yard_host app/tests/migrate.txt app/build/host >> build/host.log
if grep -q '^FAIL' build/host.log; then grep '^FAIL' build/host.log; exit 1; fi
python3 tools/yard_test.py
node web/take_test.mjs
echo "run_tests: all checks passed"
