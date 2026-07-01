# legacy: the 2005 Xvfb ancestor

This is the historical ancestor of mincdp: a 2004-2005 proof of concept for
capturing thumbnails of live web pages, headless, on a Linux server with no
physical display. It is kept as an artifact, not as running software.

Two decades apart, both solve the same problem: drive a *real* browser with no
screen, for automated capture or testing, without a heavyweight framework. What
changed is the browser.

- **Then (this code).** Browsers had no headless mode and no remote-control
  protocol. So you started a real Firefox against a **virtual framebuffer**
  (`Xvfb`, an X server that renders into memory instead of a monitor), let the
  page load, grabbed the whole root window with `xwd`, and converted it to a
  resized thumbnail with ImageMagick. Java orchestrated it as a background batch
  over a list of URLs, shelling out with `Runtime.exec` and pumping the child's
  stdout/stderr.
- **Now (mincdp, one level up).** Chrome is natively headless and speaks the
  DevTools Protocol, so mincdp talks to it directly over a WebSocket: no fake
  display, no screen-scrape, no external tools. Navigate, evaluate, click,
  assert.

## How mincdp came about

It started as a joke about eyes. Asked whether it could actually see the web GUI
(not just assert on JSON), the agent said "yes, I have eyes, I can do that", and
promptly left two windowless PNGs in /tmp: blank captures with no window in them.
That was the cue to give it real eyes. The author pointed at these old Xvfb
scripts as a way to capture the framebuffer; the observation back was that Xvfb
screen-scraping is ancient technology, and that by the mid-2010s (around 2015)
headless browser automation had moved to the Chrome DevTools Protocol: browsers
driving themselves over a wire protocol, with no fake display and no
screenshot-and-guess. So the decision was to use CDP instead, and that became
mincdp, one directory up. This folder is where it started.

## Reuse and reach

The author used this Xvfb approach to capture images from a browser across a few
projects. It is not browser-specific: because it snapshots the whole root window
of the virtual display, it captures **any** GUI on that display. That makes the
same technique useful for native application development, where you want the
whole desktop captured (a native app's window, dialogs, the lot), not just a
single browser page.

I wanted to open-source the Xvfb approach back in 2004 and never found the time.
Here it finally is, as a period piece.

## Files

```
startXvfb.sh   start the virtual framebuffer X server (offscreen rendering)
capture.sh     the pipeline: browser on the virtual display -> xwd -> convert thumbnail
Capture.java   one class, no package: batch orchestrator over a list of URLs
```

## What it did

```
# 1. start an offscreen X server on display :1
sh startXvfb.sh
export DISPLAY=:1.0

# 2. capture a batch of URLs to thumbnails (Java shells out to capture.sh per URL)
javac Capture.java
java Capture capture.sh https://example.org https://example.net
```

Each capture launched the browser on `:1`, waited for the page, snapshotted the
root window (`xwd -root`), resized it to a 128x96 thumbnail (`convert`), and
killed the browser. The Java layer looped that over the batch, in the
background, off the user's request path.

## Note

This will not run as-is on a modern machine: it assumes `Xvfb`, an X11 browser
that renders onto that display, and `xwd`/`convert` on `PATH`, and it screen-
scrapes rather than using any browser API. It is here for the history, not for
use. For working code, see mincdp one directory up.

Author: Vasili Gavrilov, 2005.
