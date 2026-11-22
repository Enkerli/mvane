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
| High | `app/synth.c` | Table or interpolation for `tan(pi fc / SR)` and the cutoff's `exp2` | CPU |
| High | `app/app.c` | Count overflows of the render's event queue; recover (all notes off, state resent) | Reliability |
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
- **The event queue.** `synth_send` drops an event when the 256-entry ring is full (`app.c`, `if (w - sq_r >= 256u)
  return;`). A lost note-off leaves a note on, a lost bend or pressure leaves stale expression. Add a counter (on
  screen beside XR), then recovery.
- **The history.** 128 KiB (32,768 events), a choice: dense MPE (bend, slide, pressure) fills it faster than notes.
  Offer sizes and measure what a minute of dense playing keeps.
- **Block length.** `synth_render` works in an `acc[256]` and renders at most 256 frames. fm1-base calls the render
  with `APP_BLOCK` (256, `app.h`), so it holds today; make it explicit (a static assert on `APP_BLOCK`, or chunks).

## Sequence

1. A baseline: worst-block CPU, xruns and memory over representative Clipper sounds and fixed expressive MIDI.
2. The memory map: effect buffers, engine arenas, the recorder; what never coexists.
3. Coefficient tables for the saw: its output before and after (pitch, cutoff tracking), its CPU.
4. The event queue: counters, recovery, a test with dense controller traffic and note ownership.
5. The two cores again: other splits of voices and effects, by the worst block.

For WindCore later, the memory budget matters most: tens of KB back could mean several physical-model voices
instead of one.
