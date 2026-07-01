#!/bin/sh
# shot.sh -- the "eyes": screenshot page.html with headless Chrome to a PNG,
# using Chrome's built-in --screenshot (no CDP needed). It doubles as a
# regression check (the page paints a non-trivial PNG) and as a way for a human
# or an agent to SEE the rendered GUI -- open the PNG, or Read it back.
#
# Usage:  sh tests/shot.sh [OUT.png]       (default /tmp/mincdp-shot.png)
# Exit:   0 = a valid, non-trivial PNG was written; 1 = failure; 77 = SKIP.

OUT=${1:-/tmp/mincdp-shot.png}
DIR=$(cd "$(dirname "$0")/.." && pwd)
PAGE="file://$DIR/page.html"

BR=$(command -v google-chrome-stable || command -v google-chrome \
     || command -v chromium || command -v chromium-browser)
[ -n "$BR" ] || { echo "shot: no chrome/chromium on PATH -- SKIP"; exit 77; }

DDIR=$(mktemp -d)
trap 'rm -rf "$DDIR"' EXIT

"$BR" --headless --disable-gpu --no-sandbox --hide-scrollbars --window-size=800,600 \
      --screenshot="$OUT" --user-data-dir="$DDIR" "$PAGE" >"$DDIR/log" 2>&1

magic=$(od -An -tx1 -N4 "$OUT" 2>/dev/null | tr -d ' ')
bytes=$(wc -c < "$OUT" 2>/dev/null || echo 0)
if [ "$magic" = "89504e47" ] && [ "$bytes" -gt 1000 ]; then
    echo "shot: ok -- $OUT ($bytes bytes)"
    exit 0
fi
echo "shot: FAIL -- no valid PNG (magic=$magic bytes=$bytes)"
exit 1
