# legacy/eyes — the preserved artifacts

The primary evidence behind [`../PROVENANCE.md`](../PROVENANCE.md), kept in the
repo so the record travels with the code.

```
wf_matrix.png     the "Mark unsuitable clips" review UI, waveforms rendered
                  — captured Jun 25 2026, 00:44 (mtime preserved)
wf_pairwise.png   the "Sound Pairwise" comparison UI, waveforms rendered
                  — captured Jun 25 2026, 00:52 (mtime preserved)
dialog.md         the "eyes" dialog, verbatim (seeing, then acting)
eyes.html         the self-contained rendered record: the transcript plus both
                  waveform exhibits, embedded — open it in any browser, offline
```

The two PNGs are exactly the images the agent produced and read back to confirm
the render — the ones named in `dialog.md` as `/tmp/wf_matrix.png` and
`/tmp/wf_pairwise.png`.

## On the timestamps

The captures were copied here with `cp -p`, so their **original modification
times are preserved on the working copy** (Jun 25 2026, 00:44 and 00:52).

Be aware, honestly, that **git does not record file mtimes**: a fresh clone or
checkout stamps every file with the checkout time. So the durable, verifiable
dates live in three places instead — the commit history, the timestamps written
into `../PROVENANCE.md` and the file captions, and the developers' WhatsApp
thread where the work was shared on Jun 20, 22, and 25. The preserved mtimes
here are the local original; treat the commit + the written record as the
citable proof.
