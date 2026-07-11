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

## The origin

It came out of a short exchange, mid-task on a private project. The agent had
implemented a waveform-rendering feature and then hedged: it could reason about
the DOM but had no way to confirm the render landed on screen.

> **Agent:** What still needs your eyes: I can't configure a study headlessly.
> If you set up a test page, I can drive the screenshot tooling (`tests/shot.sh`)
> to visually confirm the render before you commit.
>
> **Vas:** you can do it! I made eyes for you, please look at `tests/shot.sh`
>
> **Agent:** Yes! Let me use the eyes.

It then generated a real WAV, served it locally, rendered each plugin, and
screenshotted the live page in headless Chrome — then read the PNG back to
confirm the waveform rendered. Its own description of the mechanism:

> the script is the optic nerve, Chrome is the retina, my Read of the PNG is
> the seeing.

The fuller narrative, including the second "acting" dialog, is in
`legacy/README.md`.

## Timeline

All dates are 2026. Timestamps are from the capture artifacts and git.

| When                     | What                                                                                  | Evidence                                              |
|--------------------------|---------------------------------------------------------------------------------------|-------------------------------------------------------|
| Jun 22, 22:08            | Screenshot tooling already in use                                                     | `shot_test.png` (capture on disk)                     |
| Jun 25, 00:44 & 00:52    | The agent sees and confirms the waveforms                                             | `wf_matrix.png`, `wf_pairwise.png` (captures on disk) |
| Jun 20 · 22 · 25         | Work and the "eyes" dialog shared with the team, repeatedly                           | developers' WhatsApp thread (third-party timestamps)  |
| Jul 1                    | Open-sourced as `mincdp`                                                              | commits `9ce438c` (shot.sh eyes), `75afc9e` (cdp_screenshot) |

## Prior art, stated honestly

The idea of letting a machine "see" a rendered page is old, and this repo ships
the proof: `legacy/` contains a 2005 Xvfb ancestor by the same author — start a
real Firefox against a virtual framebuffer, grab the whole root window with
`xwd`, thumbnail it with ImageMagick, orchestrate the batch in Java. Two decades
apart, both do the same thing: render, then capture a picture.

What is new here is not the screenshot. It is that the thing reading the
screenshot is an agent in the middle of writing the code, using the picture to
decide whether to commit.

## The claim, stated precisely

**Not:** that agent vision was invented here. It wasn't — the 2005 folder proves
the lineage runs deep.

**Just:** that a clean, minimal implementation was independently built, wired
into a real agent's verify-before-commit loop, shared with the team across
Jun 20–25, and open-sourced on Jul 1 — with the origin dated in a WhatsApp
thread and the code dated in git.

## What is asked in return

Attribution. Not money, not a dispute, not a takedown — just the record of where
and when this was figured out, and by whom. If the idea travels, a mention is
all that is wanted.

Author: Vasili Gavrilov, 2026.
