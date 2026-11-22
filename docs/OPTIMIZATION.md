# Optimization audit (0.9-beta)

A source-level review of commit b1f0429 (the `v0.9-beta` release), received 2026-11-21, with notes from checking it
against the code. Its candidates are not measured improvements yet: the FM-1's JieLi core does not behave like a
desktop, so each change needs a measurement on the device (CPU and XR on the screen, xruns over a fixed phrase).

## Already in 0.9 (keep)

- Clipper's voices on both cores, falling back to one (`fm1.toml` cpu2, `clp_cores`).
- Clipper's effect buses skipped after 20 s of silent sends (`clp.c` clp_fx_gate, `fx.c` CLP_FX_GATE).
- The recorder: a fixed ring, no allocation, a take never copied (`core/rec.c`).
- mvane yard's saw: silent voices skipped, filter and expression at control rate (every 16 samples).
- CPU load and missed deadlines on screen.

## Candidates, ranked

| Priority | Where | Change | For |
| --- | --- | --- | --- |
| High | `app/clipper/src/fx.c` | Map the effect buffers' lifetimes; share what never runs at once | Pool RAM |
| Done | `app/synth.c` | Table or interpolation for `tan(pi fc / SR)` and the cutoff's `exp2` | CPU |
| Done | `app/app.c` | Count overflows of the render's event queue; recover (all notes off, state resent) | Reliability |
| Medium | `app/clipper/clp.c` | A finer effect gate (per bus, a shorter quiet time once tails are measured) | Idle CPU |
| Medium | `app/clipper/src/fx.c`, `voice.c` | Profile the split between the cores, the worst block not the mean | Latency |
| Medium | `app/synth.c` | Cache envelope and glide coefficients between changes | CPU |
| Conditional | `app/app.c`, `core/rec.c` | A configurable history (64 KB: 16,384 events) | Pool RAM for WindCore |

## Findings, checked

- **Effect buffers.** `fx.c` reserves the delay (32 KiB at `CLP_DLY_LEN` 16384), chorus (4 KiB), the room reverb's
  lines and diffusers (about 19.8 KiB), the plate (`pl_buf`, about 36.5 KiB) and the shimmer's octave line
  (`ps_buf`, 2 KiB): about 94 KiB, whatever the sound uses. Delay, chorus and a reverb can run together, so they cannot
  simply share. But the room and the plate are one or the other (`fx_buses` fades from one to the other and clears the
  one it leaves): `rev_line` / `rev_ap` and `pl_buf` could be one buffer, about 20 KiB back. Needs a compatibility
  matrix first.
- **The saw's control loop.** Per voice every 16 samples: `fm_exp2f` for the bend, `fm_exp2f` for the cutoff,
  `fm_tanf` for the filter's coefficient; at 12 voices about 99,000 a second (counted, not measured). A table for the
  coefficient is the safest; the bend wants pitch accuracy, to test with slow MPE bends on held notes.
  **Done (0.10-dev):** the cutoff is worked out in octaves (the pitch's `log2`, cached and computed again only while a
  note glides, plus the bend, the tone, pressure, slide and breath), and `g` and the one-pole's `c1` are read from a
  table of 16 points an octave from 20 Hz to 16 kHz, interpolated (1.25 KB of RAM). The bend's `exp2` stays exact,
  skipped while the bend is still. Per voice every 16 samples: one `exp2` while bending, one `log2` while gliding,
  otherwise neither, and no `tan`. `make synth-cmp` renders a scenario (a TONE sweep, slow out-of-phase bends with
  slide and pressure, high bright notes) with the tables and with `-DSYNTH_EXACT`: the difference is 55 dB under the
  signal, 42 dB at the cutoff's top (a few cents of cutoff near 16 kHz), and on a Mac the saw takes 12% less CPU.
  The FM-1's gain is to measure with the baseline phrase; a 32-point table gains only 5 dB more, so the remaining
  difference is not the table's.
- **The event queue.** `synth_send` drops an event when the 256-entry ring is full (`app.c`, `if (w - sq_r >= 256u)
  return;`). A lost note-off leaves a note on, a lost bend or pressure leaves stale expression. Add a counter (on
  screen beside XR), then recovery. **Done (0.10-dev):** each lost event is counted (Q on the screen's bottom line);
  `synth_send` keeps what each channel should have (notes on, velocity, bend, pressure, CC 1, 2, 11, 64, 74) whether
  or not the event fits, and once the render has emptied the queue `sq_recover` silences the engine (status 0 in the
  queue) and sends that state again, notes last. `app/tests/queue.txt` floods the queue (`flood N`, the app's own host
  command) and checks that a chord let go during the overflow goes silent and one played during it sounds, on the saw
  and on a Clipper sound; without the recovery the notes stick.
- **The history.** 128 KiB (32,768 events), a choice: dense MPE (bend, slide, pressure) fills it faster than notes.
  Offer sizes and measure what a minute of dense playing keeps.
- **Block length.** `synth_render` works in an `acc[256]` and renders at most 256 frames. fm1-base calls the render
  with `APP_BLOCK` (256, `app.h`), so it holds today. **Done:** `SYNTH_BLOCK` in `synth.h`, and `app.c` fails to
  compile if `APP_BLOCK` exceeds it.

## The baseline, on the device

`tests/baseline.mid` (made by `tools/baseline_mid.py`) is a fixed 30-second MPE phrase to play into the FM-1 over USB
from a DAW, so that readings before and after a change compare:

- 0-10 s: five six-note chords, each note on its own member channel, no expression;
- 10-20 s: the same with each note's bend, pressure and slide every 10 ms (about 1,800 messages a second);
- 20-30 s: a held chord with sixteenth-note arpeggios and expression on eight channels every 20 ms, then 600 pressure messages at
  one instant (the queue at its busiest: Q may count, and nothing should stay stuck).

For each sound (the saw, then STRINGS, HYBRID, FM6, JUNO and one LIGHT sound such as SAMPLE), from a fresh boot:
CPU at rest, the highest CPU seen in each part, XR and Q at the end, and anything heard (crackle, stuck notes, steps
in a bend). The screen shows the mean load, not the worst block, so XR is the stronger sign.

## Readings on the device

Alex, 2026-11-22, the baseline phrase played from a slot (build 5f48553, with the expression slots):

| Sound | Engine, cost | CPU | Notes |
| --- | --- | --- | --- |
| at rest | | 18%, then 9% | the effect gate: buses off after 20 s of silent sends |
| SAW ENSEMBLE | SAMPLE, light poly | 37-38%, 24 to 42% | XR 1 |
| PLUCK | HYBRID, mid poly | about 50%, 29 to 54% | |
| GLASS PAD | HYBRID, heavy poly | 58% rising to 78% | 48-54% on 0.9: the regression below |
| CLIPPER (init) | HYBRID, light poly | 33% | |
| 80S HITS | JUNO, light, short | 11 to 54% | XR 2 |

Q rose by 344 a run: the phrase's 600 pressure messages at one instant, less the queue's 256. Quick successions of
notes or chords raise the load most (voices starting while others release).

Alex, 2026-11-22, the same phrase on ae3d64e (the second core back for every sound that splits; SAMPLE and WHEEL
splitting too):

| Sound | CPU | Q (since power-on) |
| --- | --- | --- |
| HYBRID CLIPPER (init) | mostly 25%, dips to 17% | 155 |
| HYBRID CLIP WIDE (two SUPERs) | mostly 71%, dips to 43% | 499 |
| HYBRID DARK PAD | 50% | 587 |
| JUNO 80S HITS | the hits 40%, 11% between, the arpeggios 25% | 707 |
| STRINGS STR MACHINE | 47%, peaks 52% | 1051 |
| TERRAIN (first sound, fresh start) | 41% | 344; maybe a glitch in the sound (the patch?) |
| YARD (the saw) | 1% at rest, mostly 12%, dips to 5% | 688 |

HYBRID CLIPPER went from 33% to 25%, 80S HITS from up to 54% to about 40%: the second core at work. The saw is the
lightest sound by far (one core, no effect chain), a base to build on (roadmap). Q counts the phrase's 600 messages at
one instant less what the render drained meanwhile (88 to 344 a run).

**The effects and the CPU** (Alex, 2026-11-22: turning CHORUS off gains nothing, all four sends up costs nothing
visible). Measured here on HYBRID CLIPPER with a chord, a block: 28.7 us with the effect buses gated off (every send
0 for 20 s), 33.0 us with the sends just set to 0, 33.1 us with CHORUS 24, 34.4 us with all four at 127. So the buses
cost about 15% of a light sound and their sends little more; the gate only acts after 20 s of silence. A per-bus gate
(a bus off once its send is 0 and its tail has died, in well under a second) would give most of that 15% back while
playing: the audit's "finer effect gate". Alex's taste, for the defaults: a little chorus (2 to 4), SPACE rather than
the REVERB send, usually no DRIVE or DELAY.

**0.10's cost analysis, again** (Alex: some LIGHT sounds were close to MID or HEAVY). `make clp-cost` now plays
a chord of six with each sound's trim and expression slots, its second core's half timed apart (the FM-1 runs it
alongside): a block's cost is about the longer half. Fit to Alex's readings: about 1.25% of the FM-1 a microsecond
here, plus 22%. Two tiers: EZ (15 us or less, about 40% with such a chord: 116 sounds) and HRD (221).

**Xruns at a change of sound.** On this computer the block a sound loads in costs no more than its others (median
of five: +3 us), nor does a chord's first block. What the FM-1 adds is its instruction cache: a new engine's code
comes from the flash on its first blocks. To tell, the bottom line now shows PK (the worst block since the latest
change of sound, past its first second) and SW (the worst in that first second), and XR counts the xruns within a
second of a change (n AT SW). If SW runs well over PK, the change is it: the engine's hot code in RAM
(`.c1_text`), or a shorter crossfade of the old sound, are the ways out.

**The regression, found and fixed.** Clipper splits a sound's voices between the cores only for voices whose EDIT
values the matrix leaves alone (voice.c: such a voice writes the part's values for its call, so it renders on this
core after the other's). The expression slots of 5f48553 pointed HYBRID's pressure at its CUT (E1) and JUNO's slide
at its CUT (E6): those sounds lost the second core entirely (`tools/clp_split.c`: GLASS PAD from 2,880 voice-blocks
on core 1 to none). Now an engine that splits takes the matrix's CUT or SHAPE, carried per voice; `make clp-split`
(in CI) fails when any sound splits less with the slots. 185 of 337 sounds split; the rest are mono sounds (one
voice) and the five engines Clipper keeps on one core: SAMPLE, VOICE, WHEEL, GRAIN, PHYS (72 sounds, among them the
light polyphonic ones Alex likes). The screen shows x2 after CPU while the second core takes jobs, x1 lit when not.

**SAMPLE and WHEEL on both cores** (0.10-dev). Clipper kept them on one core without saying why; their renders
write only their own voice (SAMPLE reads its zone and the flash's data; WHEEL's bars and rotors are set by
`drawbar_block` before the voices, then only read), and fm1-base's second core may read the flash. Added to
`voice.c vs_ok`. `make clp-race` (in CI) renders each sound with the second core's job on a real second thread under
ThreadSanitizer: every one of the 337 sounds is the same bit for bit as on one core, and no race is reported (a
planted one is). Their expression slots moved off EDIT values (SAMPLE: CUT; WHEEL: the level only, its drawbars
being the part's). 202 of 337 sounds split now; SAW ENSEMBLE, SAMPLE CHOIR and the organs hand about half their
voice-blocks to the second core. Still on one core: VOICE, GRAIN, PHYS, and mono sounds.

Next: the load of quick successions (voice starts, release tails, stealing).

## Sequence

1. A baseline: worst-block CPU, xruns and memory over representative Clipper sounds and fixed expressive MIDI.
2. The memory map: effect buffers, engine arenas, the recorder; what never coexists.
3. Coefficient tables for the saw: its output before and after (pitch, cutoff tracking), its CPU.
4. The event queue: counters, recovery, a test with dense controller traffic and note ownership.
5. The two cores again: other splits of voices and effects, by the worst block.

For WindCore later, the memory budget matters most: tens of KB back could mean several physical-model voices
instead of one.
