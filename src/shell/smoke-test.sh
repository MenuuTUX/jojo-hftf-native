#!/bin/bash
# Launches build/JojoShell.app, waits 3 seconds, quits it, and checks that no process is left behind.
# A stub game folder (empty index.tsv) is passed with -gameFolderPath so the first-launch chooser does not block.
# That override is an argument-domain default and is not written to the user's preferences.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/build/JojoShell.app/Contents/MacOS/JojoShell"
PATTERN="JojoShell.app/Contents/MacOS/JojoShell"
WAIT_SECONDS=3

if [ ! -x "$BIN" ]; then
    echo "FAIL: $BIN not found. Run build.sh first."
    exit 1
fi

if pgrep -fl "$PATTERN" >/dev/null; then
    echo "FAIL: a JojoShell process is already running:"
    pgrep -fl "$PATTERN"
    exit 1
fi

TMP="$(mktemp -d)"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT
: > "$TMP/index.tsv"

"$BIN" -gameFolderPath "$TMP" > "$TMP/app.log" 2>&1 &
PID=$!
echo "launched pid $PID"

sleep "$WAIT_SECONDS"

if ! kill -0 "$PID" 2>/dev/null; then
    echo "FAIL: app exited within ${WAIT_SECONDS}s"
    cat "$TMP/app.log"
    wait "$PID" 2>/dev/null
    echo "exit status: $?"
    exit 1
fi
echo "running after ${WAIT_SECONDS}s, sending SIGTERM"

kill -TERM "$PID" 2>/dev/null
for _ in $(seq 1 50); do
    kill -0 "$PID" 2>/dev/null || break
    sleep 0.1
done
if kill -0 "$PID" 2>/dev/null; then
    echo "app ignored SIGTERM, sending SIGKILL"
    kill -KILL "$PID" 2>/dev/null
fi
wait "$PID" 2>/dev/null
STATUS=$?

# Check the process table too, in case the app spawned a child.
sleep 0.5
if pgrep -fl "$PATTERN" >/dev/null; then
    echo "FAIL: process left behind:"
    pgrep -fl "$PATTERN"
    pkill -KILL -f "$PATTERN" 2>/dev/null
    exit 1
fi

echo "PASS: launched, ran for ${WAIT_SECONDS}s, quit (wait status $STATUS), no process left"
exit 0
