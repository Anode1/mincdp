# mincdp

Drive a real headless Chrome for tests without Selenium or a version-matched
chromedriver. Two tiny, dependency-free clients that speak the Chrome DevTools
Protocol (CDP) straight to the browser over a WebSocket, in the language of your
stack:

- **C** — `c/cdp.h`, a single header-only library (~410 lines, libc + POSIX
  sockets only).
- **Java** — `java/Cdp.java`, a single file (JDK `java.net.http` only, nothing
  to vendor).

Same idea, same small command set, one demonstration each against the same page.

## Why it exists

CDP is the wire protocol Puppeteer and Playwright use under the hood. Talking it
directly means no chromedriver middleman and no external dependency to vendor and
keep version-matched to Chrome. For a fixed set of internal pages, headless, one
browser, that is a good trade: the client is small enough to read in a sitting,
and if Chrome ever ships a breaking CDP change you fix the one file rather than
wait for an upstream driver release.

It is deliberately **not** a general framework. It knows only the commands a
click-and-assert test needs: navigate, evaluate JS, type text, press a key. If
you need cross-browser (Firefox/Safari) or a rich interaction API, use
Playwright; this earns its place only where minimalism and zero dependencies do.

## Quick start

Needs a headless Chrome/Chromium on `PATH` (and `curl`, and `cc` / `javac`).

```
make ut          # the whole regression suite (see Tests)
make demo-c      # build c/demo and run it against page.html
make demo-java   # build java/Demo and run it against page.html
make demo        # both
make shot        # screenshot page.html so you can SEE it
```

Each demo prints a series of assertions and a `demo: N passed, 0 failed` line. It
first inspects the page's internals (title, attribute, text, computed style,
geometry), then drives the real input path: type "mincdp works", press Enter, and
assert the page echoes it. Set `CDP_DEBUG=1` to see every CDP frame.

## Tests

`make ut` runs the full regression, in two kinds, so a test set has both hands
and eyes:

- **codeut** -- "regular" browser-free unit tests of the client's pure helpers
  (base64, JSON escaping), several examples each. No Chrome needed, so it always
  runs (`tests/codeut.c`).
- **uiut** -- the client driving real headless Chrome, in two kinds:
  - *page internals + hands* -- the demos inspect the rendered DOM (presence,
    attribute, text, computed style, geometry), then interact (type, press Enter,
    assert the DOM changed). Eyeless: all through `Runtime.evaluate`
    (`c/demo.c`, `java/Demo.java`).
  - *eyes, for agents* -- `tests/shot.sh` screenshots the page with Chrome's
    `--screenshot` and asserts a valid, non-trivial PNG. The PNG is also there to
    be looked at: open it, or have an agent Read it.

Each layer reports `PASS` / `FAIL` / `SKIP` (a missing toolchain SKIPs, exit 77);
the suite fails only if a non-skipped layer fails:

```
mincdp regression:
  codeut (units)       PASS
  uiut hands: C        PASS
  uiut hands: Java     PASS
  uiut eyes: shot      PASS
```

The demos double as the interaction tests; `make demo-c` / `demo-java` run them
on their own.

You start Chrome; the client attaches. `demo.sh` does that wiring (launch a
headless Chrome with `--remote-debugging-port`, run the demo, tear down), and it
invokes both demos with the same contract, so one launcher and one `page.html`
serve both languages:

```
PROG... 127.0.0.1 <chrome-port> file://<abs>/page.html
```

## The API

Both clients expose the same shape (C names shown; Java is the camelCase twin):

| Call | Does |
|------|------|
| `cdp_open(host, port)` / `Cdp.attach` | attach to the first page target of a running Chrome |
| `cdp_navigate(url)` | `Page.navigate` (does not wait for load) |
| `cdp_eval_bool(js, &out)` / `evalBool` | `Runtime.evaluate` a boolean expression |
| `cdp_wait_bool(js, ms)` / `waitBool` | poll a boolean expression until true or timeout |
| `cdp_insert_text(text)` / `insertText` | `Input.insertText` into the focused element |
| `cdp_key("Enter")` / `key` | `Input.dispatchKeyEvent` keyDown+keyUp |
| `cdp_close()` / `close` | detach |

Responses are matched by id and read with targeted substring checks, not a JSON
parser. That is the trick that keeps both clients dependency-free: every command
here returns a boolean or a short ack, so looking for `"value":true` /
`exceptionDetails` is enough. A command that returned rich JSON would need a real
parser; none does.

### Embedding the C client

`c/cdp.h` is a single-header library. In exactly one translation unit:

```c
#define CDP_IMPLEMENTATION
#include "cdp.h"
```

and just `#include "cdp.h"` elsewhere for the declarations. See `c/demo.c`.

## How it compares to Selenium

Same category as a Selenium test (out-of-process automation: your code in one
process, a real browser in another, talking over a wire protocol), minus the
chromedriver middleman and the external dependency. Closer in spirit to a
minimal, hand-rolled Puppeteer.

| Selenium | mincdp |
|----------|--------|
| `WebDriver driver` | `cdp` handle |
| `driver.get(url)` | `cdp_navigate(url)` |
| `findElement` / waits | `cdp_wait_bool(sel-exists)` |
| `((JavascriptExecutor)driver).executeScript` | `cdp_eval_bool(js)` |
| `element.sendKeys` | `cdp_insert_text` |
| key press | `cdp_key("Enter")` |

What differs:

- **Selenium** talks the WebDriver protocol to a separate `chromedriver` binary
  that then drives Chrome: three processes, and you must keep chromedriver's
  version matched to Chrome's. It is a jar plus a versioned native binary to
  vendor and update.
- **mincdp** talks CDP directly to Chrome over a WebSocket, no middleman. Each
  client is one file using only its language's standard library.

The trade, honestly: Selenium and Playwright give a big, polished, cross-browser
API (rich waits, action chains, a grid). mincdp gives the handful of commands a
headless smoke test needs, in code you can read end to end. Pick accordingly.

## Layout

```
page.html          self-contained demo target (no server, no network)
demo.sh            launch Chrome, run a demo, tear down (shared by both)
Makefile           make ut / demo-c / demo-java / demo / shot / clean
c/cdp.h            the C client (single-header library)
c/demo.c           the C demonstration = the uiut interaction test (hands)
java/Cdp.java      the Java client (single file)
java/Demo.java     the Java demonstration = the uiut interaction test (hands)
tests/run.sh       the regression runner (make ut): codeut + uiut, PASS/FAIL/SKIP
tests/codeut.c     browser-free unit tests of the client's pure helpers
tests/shot.sh      the eyes: screenshot page.html to a PNG (Chrome --screenshot)
```

## Lineage

`legacy/` holds the 2005 ancestor of this project: an Xvfb-based screenshot
capture proof of concept, from before browsers had a headless mode or a remote
protocol. It is where mincdp started. See `legacy/README.md`.

## License

MIT. See `LICENSE`.

## Trademarks

All product names, logos, and brands are property of their respective owners.
Chrome is a trademark of Google; Selenium, Puppeteer, and Playwright are the
names of their respective projects. They are used here only for identification
and comparison, and no affiliation or endorsement is implied.
