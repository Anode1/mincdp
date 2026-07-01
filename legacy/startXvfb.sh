#!/bin/sh
# startXvfb.sh -- start a virtual framebuffer X server (Xvfb): a real X server
# that renders into memory instead of a monitor, so a browser or any GUI app can
# run and be captured on a headless machine. 2005 proof of concept (see README.md).
#
# Pick a resolution/depth to match what you intend to capture. Writes the pid to
# xvfb.pid. After this, point clients at it with:  export DISPLAY=:1.0
Xvfb :1 -screen 0 640x480x16 -nolisten tcp 2>xvfb.log &
echo $! > xvfb.pid
echo "Xvfb started on :1 (pid $(cat xvfb.pid)); export DISPLAY=:1.0 to use it"
