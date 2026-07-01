#!/bin/sh
# capture.sh -- capture a live web page (or any GUI on the display) to a
# thumbnail, headless, via Xvfb. 2005 proof of concept (see README.md).
#
# Launches a browser on the virtual framebuffer display, waits for the page to
# render, grabs the whole root window with xwd, and resizes it to a thumbnail
# with ImageMagick's convert. Because it snapshots the root window, the same
# technique captures any application on the display, not just a browser.
#
# Usage: sh capture.sh -u URL [-o OUTFILE]
# Needs: Xvfb running on the display (see startXvfb.sh), a browser, xwd, convert.

DISPLAY_NUM=":1.0"
SIZE=128x96
BROWSER=firefox
URL=
OUT=

while getopts u:o:h flag; do
    case "$flag" in
        u) URL="$OPTARG" ;;
        o) OUT="$OPTARG" ;;
        h) echo "usage: sh capture.sh -u URL [-o OUTFILE]"; exit 0 ;;
        *) exit 2 ;;
    esac
done
[ -n "$URL" ] || { echo "capture: -u URL required" >&2; exit 2; }
[ -n "$OUT" ] || OUT="capture-$(date +%s%N).gif"

# launch the browser on the virtual display; give the page time to render
"$BROWSER" --display="$DISPLAY_NUM" "$URL" >/dev/null 2>&1 &
sleep 5

# snapshot the whole root window, then resize to a thumbnail
TMP="${OUT%.gif}.xwd"
xwd -display "$DISPLAY_NUM" -root -out "$TMP"
convert -size "$SIZE" "$TMP" -resize "$SIZE" "$OUT"
rm -f "$TMP"

# tidy up the browser we launched
pid=$(ps ax | grep "$BROWSER" | grep -v grep | grep -v /bin/sh | awk '{print $1}')
[ -n "$pid" ] && kill $pid 2>/dev/null

echo "$OUT"
