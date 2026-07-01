#!/bin/sh
# demo.sh -- launch a headless Chrome, run a demo against page.html, tear down.
# The demo program is passed as arguments and is invoked as:
#     PROG... 127.0.0.1 <chrome-port> file://<abs>/page.html
# so the C and Java demos share one launcher and one page. Examples:
#     sh demo.sh c/demo
#     sh demo.sh java -cp java/out Demo
# Exit: the demo's exit code (0 = passed), or 77 if Chrome is absent.

[ $# -ge 1 ] || { echo "usage: sh demo.sh PROG [ARGS...]"; exit 2; }
DIR=$(cd "$(dirname "$0")" && pwd)
PAGE="file://$DIR/page.html"

BR=$(command -v google-chrome-stable || command -v google-chrome \
     || command -v chromium || command -v chromium-browser)
[ -n "$BR" ] || { echo "demo: no chrome/chromium on PATH -- SKIP"; exit 77; }
command -v curl >/dev/null 2>&1 || { echo "demo: curl not found -- SKIP"; exit 77; }

DDIR=$(mktemp -d)
CPORT=$(( 21000 + ($$ % 2000) ))
CH=
cleanup() { [ -n "$CH" ] && kill "$CH" 2>/dev/null; wait 2>/dev/null; rm -rf "$DDIR" 2>/dev/null; }
trap cleanup EXIT

"$BR" --headless --disable-gpu --no-sandbox --disable-extensions --no-first-run \
      --no-default-browser-check --remote-debugging-address=127.0.0.1 \
      --remote-debugging-port="$CPORT" --user-data-dir="$DDIR" about:blank \
      >"$DDIR/chrome.log" 2>&1 &
CH=$!

i=0; while [ $i -lt 50 ]; do curl -s -o /dev/null "http://127.0.0.1:$CPORT/json/version" && break; i=$((i+1)); sleep 0.1; done
if ! curl -s -o /dev/null "http://127.0.0.1:$CPORT/json/version"; then
    echo "demo: chrome debug port not up on $CPORT"; exit 1
fi

"$@" 127.0.0.1 "$CPORT" "$PAGE"
