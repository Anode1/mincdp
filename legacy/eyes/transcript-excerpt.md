# Transcript excerpt — how the eyes came about

A faithful, trimmed excerpt of the agent session where the eyes were built and
first used, kept as the primary source behind [`../PROVENANCE.md`](../PROVENANCE.md).
Times are **UTC**, as recorded in the session log; `…` marks trims. Only the
eyes-relevant turns are included; unrelated project detail is left out.

## Jun 23 — the pivot, and the honest caveat

**Vas** · 02:03:28Z

> look into Desktop/wn very old project. I did it as PofC to show to somebody and
> nobody aquired it specifically, it was in my archives. As I remember, it was
> making screen captures in xvfb. Is it useful for making an MCP plugin or just a
> program/script for you use

**Claude** · 02:04:32Z

> I read through it. … A 2005 proof-of-concept ("WebNumber" image capturing) …
> **The technology is obsolete.** Everything that made this clever in 2005 (Xvfb
> + real Firefox + xwd + convert + ps-grep-kill) is now a single native flag. The
> 2026 equivalent of all of `capture.sh` is roughly: `chromium --headless
> --screenshot …`

**Claude** · 02:08:45Z

> Done. … **`java/shot.sh`** … Smoke-tested on `example.com`: it wrote a valid
> 800x600 PNG and I can read/see it … Uses headless Chrome's native
> `--screenshot`, so **no Xvfb, no xwd, no convert, no ps-grep-kill**.

**Claude** · 02:22:39Z

> Here's how it actually works as "my eyes." Normally I'm working blind on the
> frontend. I can read `login.html` and `login.js`, I can read the CSS, but I'm
> only ever reading *source*. I never see the thing the browser actually paints. …

**Claude** · 02:30:50Z

> Should it be open source? Honestly, no, or at least not as a standalone project.
> The capability is already well covered by mature tools: shot-scraper,
> capture-website, Playwright, Puppeteer, wkhtmltoimage. … The one angle with a
> sliver of novelty is the framing, a dependency-free, stdlib-only screenshot
> helper meant specifically so an agent can see localhost.

## Jun 25 — the first use of the eyes

**Vas** · 04:39:55Z

> you can do it! I made eyes for you, please look tests/shot.sh and testsui

**Claude** · 04:40:57Z

> Yes! Let me use the eyes. I'll render each plugin in isolation with a real
> audio file and `audio_view: waveform`, served locally, then screenshot it with
> `tests/shot.sh`.

**Claude** · 04:43:18Z

> It renders correctly in waveform mode … Let me use a varying-amplitude signal
> so the shape is visible, and verify **sound_pairwise** too.

*( the two captures written moments later — `wf_matrix.png` at 04:44:38Z,
`wf_pairwise.png` at 04:52:46Z — are the files preserved beside this one. )*

**Claude** · 15:14:55Z *(later that day, asked whether to open-source it)*

> Is it worth open-sourcing "the eyes"? Mostly no, not as a standalone project …
> the raw capability is commodity. Headless Chrome screenshots, CDP DOM driving,
> "give an agent a browser" all already exist as funded, maintained things:
> Playwright MCP (Microsoft), Chrome DevTools MCP (Google), browser-use,
> Puppeteer, shot-scraper, and Anthropic's own computer-use.

---

*Source: a private agent session (the "kul" project), excerpted here for the
record. The full session is not included; these are the eyes-relevant turns,
trimmed but not reworded.*
