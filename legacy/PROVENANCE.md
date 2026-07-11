# Provenance: the eyes

This file is the dated record of where and when the "eyes" in this repo were
figured out. It exists so the timeline lives in git, alongside the code, and not
only in memory or a chat thread.

## What "the eyes" are

A render → screenshot → read-the-pixels loop that lets a coding agent visually
verify a UI before committing, plus the live variant where the agent drives the
page first and screenshots the result. In this repo:

- `tests/shot.sh` — the static eyes: render in headless Chrome, capture a PNG,
  let the agent Read the pixels.
- `cdp_screenshot` (C + Java) — the live eyes: capture the driven state over the
  DevTools Protocol after the agent navigates, types, clicks, and asserts.

## The origin, as it happened

It came out of two short exchanges, mid-task on a private project. The whole
dialog, verbatim:

### First — seeing

Claude (after implementing):

> What still needs your eyes (I can't configure a study headlessly)
>
> 1. Open the page editor ... confirm the "Audio view" select appears.
> 2. Set it to Waveform, run the page ... confirm the waveform renders ...
>
> ... If you set up a test page, I can drive the screenshot/shot.sh tooling to
> visually confirm the waveform render before you commit.

Vas:

> you can do it! I made eyes for you, please look tests/shot.sh and testsui

Claude:

> Yes! Let me use the eyes. I'll render each plugin in isolation with a real
> audio file and audio_view: waveform, served locally, then screenshot it with
> tests/shot.sh.

( generates a WAV, serves it locally, renders each plugin, runs tests/shot.sh ... )

Claude:

> Confirmed visually with the eyes, both plugins render the waveform correctly
> ... I rendered each plugin in isolation against a real generated WAV, served
> locally, and screenshotted with tests/shot.sh.
>
> 🖼️ /tmp/wf_matrix.png   🖼️ /tmp/wf_pairwise.png

As the agent put it: *the script is the optic nerve, Chrome is the retina, my
Read of the PNG is the seeing.*

### Then — acting

By the next exchange the eyes were not a snapshot but the live page under
control: type a query, press Enter, assert the result renders.

Vas:

> You have eyes

Claude:

> yes, I have my eyes now!

### The shape of it — hands, then eyes

Later we noticed what had happened, and it was funny: the agent already had
*hands* — it could act on a page, type text, press Enter, click, assert the DOM
changed — and now it had *eyes*, to see the result of what its hands had done.
That pairing is exactly what this repo's test suite is named for: `make ut` runs
both kinds, "so a test set has both hands and eyes" (see the README). The hands
came first; the eyes are what June 25 added.

The surrounding narrative is in `legacy/README.md`. The preserved artifacts —
the two waveform captures, the verbatim dialog, and the self-contained rendered
record — are in [`legacy/eyes/`](eyes/).

## Timeline

Dates are 2026. Transcript times are UTC (the agent's session log); capture
mtimes are −04:00 local — they refer to the same moments, and cross-check to the
minute. The session transcript is excerpted in
[`legacy/eyes/transcript-excerpt.md`](eyes/transcript-excerpt.md).

| When                       | What                                                                                  | Evidence                                              |
|----------------------------|---------------------------------------------------------------------------------------|-------------------------------------------------------|
| Jun 23, 02:03–02:30 UTC    | I pointed the agent at my 2005 `wn`/Xvfb PoC; it called the technology "obsolete," wrote `shot.sh` on native headless Chrome, smoke-tested it, and coined the "eyes" framing ("I never see the thing the browser actually paints") | transcript; `shot_test.png` — the smoke test (mtime 22:08 −04:00 = 02:08 UTC Jun 23) |
| Jun 25, 04:39–04:52 UTC    | **First real use of the eyes:** the agent drove `shot.sh` to render and confirm the study's waveforms | `legacy/eyes/wf_matrix.png` (04:44 UTC), `wf_pairwise.png` (04:52 UTC) — preserved; matches the transcript to the minute |
| Jun 25                     | Shared the eyes with the team, in two separate venues                                 | developers' WhatsApp thread, and the internal developers' forum — a piece of the conversation in a thread titled "eyes" (both third-party-visible) |
| Jul 1                      | Open-sourced as `mincdp`                                                              | commits `9ce438c` (shot.sh eyes), `75afc9e` (cdp_screenshot) |

(The broader UI-testing work was shared with the team on earlier days too; the
*eyes* specifically were built Jun 23 and first used Jun 25, so this table scopes
to those.)

## Prior art, stated honestly

The idea of letting a machine "see" a rendered page is old, and this repo ships
the proof: `legacy/` contains a 2005 Xvfb ancestor by the same author — start a
real Firefox against a virtual framebuffer, grab the whole root window with
`xwd`, thumbnail it with ImageMagick, orchestrate the batch in Java. Two decades
apart, both do the same thing: render, then capture a picture.

What is new here is not the screenshot. It is that the thing reading the
screenshot is an agent in the middle of writing the code, using the picture to
decide whether to commit.

## What the agent itself said about novelty

Honesty cuts both ways, so this belongs in the record. In the same sessions, the
agent argued — more than once, unprompted — that the raw capability is *not*
novel and was lukewarm on open-sourcing it at all:

> *(Jun 23)* Should it be open source? Honestly, no, or at least not as a
> standalone project. The capability is already well covered by mature tools:
> shot-scraper, capture-website, Playwright, Puppeteer, wkhtmltoimage. ... The
> one angle with a sliver of novelty is the framing — a dependency-free,
> stdlib-only screenshot helper meant specifically so an agent can see localhost.

> *(Jun 25)* Is it worth open-sourcing "the eyes"? Mostly no, not as a standalone
> project ... the raw capability is commodity. Headless Chrome screenshots, CDP
> DOM driving, "give an agent a browser" all already exist as funded, maintained
> things: Playwright MCP (Microsoft), Chrome DevTools MCP (Google), browser-use,
> Puppeteer, shot-scraper, and Anthropic's own computer-use.

That is the honest context, and it settles the point plainly: screenshot-based
agent vision is commodity, and several funded projects — Anthropic's own
computer-use among them — predate and coexist with this. **Nothing here is a
claim to have been first at the capability, or a claim over anyone else's work.**

What remains genuinely this project's own is narrow and real: the specific
minimal, dependency-free implementation (`shot.sh`, then the CDP-native clients
in C and Java), the "eyes / hands" framing, and the decision to build it cleanly
and open-source it on Jul 1. Attribution for *that* — not for the idea — is the
whole ask.

## The claim, stated precisely

**Not:** that agent vision was invented here. It wasn't — the 2005 folder proves
the lineage runs deep.

**Just:** that a clean, minimal implementation was independently built (Jun 23),
wired into a real agent's verify-before-commit loop and first used on Jun 25,
shared with the team the same day, and open-sourced on Jul 1 — with the origin
dated in a WhatsApp thread and the code dated in git.

## What is asked in return

Attribution. Not money, not a dispute, not a takedown — just the record of where
and when this was figured out, and by whom. If the idea travels, a mention is
all that is wanted.

Author: Vasili Gavrilov, 2026.
