#!/bin/sh
# Build mvane with fm1-base's build tool: ./build.sh (the .fwsc), ./build.sh --host, ./build.sh --web, ./build.sh --release 0.1
set -e
cd "$(dirname "$0")"
PY="${PYTHON:-python3}"
[ -x base/.venv/bin/python ] && PY=base/.venv/bin/python
exec "$PY" base/tools/build.py app "$@"
