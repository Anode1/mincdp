#!/bin/sh
# agent.sh -- launch a headless Chrome, let Claude drive it toward a goal, tear down.
# Mirrors demo.sh: same one launcher, one Chrome. The agent starts on page.html and
# is then steered by the model. Usage:
#     export ANTHROPIC_API_KEY=sk-ant-...
#     sh agent.sh "type 'mincdp works' into the box and press Enter"
# Exit: the agent's exit code (0 = reached DONE), or 77 if Chrome/curl/the key is absent.

[ $# -ge 1 ] || { echo "usage: sh agent.sh GOAL..."; exit 2; }
DIR=$(cd "$(dirname "$0")" && pwd)
PAGE="file://$DIR/page.html"

[ -n "$ANTHROPIC_API_KEY" ] || { echo "agent: ANTHROPIC_API_KEY not set -- SKIP"; exit 77; }

BR=$(command -v google-chrome-stable || command -v google-chrome \
     || command -v chromium || command -v chromium-browser)
[ -n "$BR" ] || { echo "agent: no chrome/chromium on PATH -- SKIP"; exit 77; }
command -v curl >/dev/null 2>&1 || { echo "agent: curl not found -- SKIP"; exit 77; }

[ -x "$DIR/c/agent" ] || cc -std=c99 -Wall -Wextra -O2 -o "$DIR/c/agent" "$DIR/c/agent.c"

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
    echo "agent: chrome debug port not up on $CPORT"; exit 1
fi

"$DIR/c/agent" 127.0.0.1 "$CPORT" "$PAGE" "$@"
