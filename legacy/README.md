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

It came out of two short conversations about eyes.

The first was about *seeing*. Mid-task the agent hedged: "What still needs your
eyes: I can't configure a study headlessly. If you set up a test page, I can drive
the screenshot tooling (tests/shot.sh) to visually confirm the render before you
commit." Vas replied: "you can do it! I made eyes for you, please look at
tests/shot.sh." The agent: "Yes! Let me use the eyes", and it screenshotted the
live page in headless Chrome and read the PNG's pixels directly. As it put it then:
"the script is the optic nerve, Chrome is the retina, my Read of the PNG is the
seeing." Those first eyes were static, a frozen screenshot: the direct heir of the
Xvfb screen-scrape in this folder, headless Chrome now in place of Xvfb plus
Firefox plus xwd, the same "render, then capture a picture" idea.

The second was about *acting*. Asking to test the GUI for real, the agent got
Vas's "You have eyes", and answered "yes, I have eyes now!". By then the eyes were
not a snapshot but the live page under control: type a query, press Enter, assert
the result renders. Xvfb screen-scraping is ancient technology, and by the
mid-2010s (around 2015) headless browser automation had moved to the Chrome
DevTools Protocol: browsers driving themselves over a wire protocol, no fake
display, no screenshot-and-guess. That is the eyes mincdp gives, one directory up.
This folder is where the lineage starts.

## Reuse and reach

The author used this Xvfb approach to capture images from a browser across a few
projects. It is not browser-specific: because it snapshots the whole root window
of the virtual display, it captures **any** GUI on that display. That makes the
same technique useful for native application development, where you want the
whole desktop captured (a native app's window, dialogs, the lot), not just a
single browser page.

The modern, working heir of exactly this is ais's Flutter UI test: Xvfb plus
`xdotool` (X input) and ImageMagick `import` (screenshots), driving a real native
GUI headlessly. Same idea, current tools:
https://github.com/Anode1/ais/tree/main/app/flutter/uitest

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
