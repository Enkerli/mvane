# Toward Vane on mvane yard: what fits, and where (2026-11-23)

Alex asked for "basic Vane" next (wavetables, attack transients), then waveguides, and WindCore if it can be adapted:
this is the feasibility assessment, from measurements on this Mac and the FM-1's figures as the CI build reports them.
The order it suggests is in [ROADMAP.md](ROADMAP.md) ("Next, sorted").

## What there is to spend

| | The FM-1 (0.15, today's CI build) | VIBE FM, native |
| --- | --- | --- |
| Flash for code and tables | the app image 414,932 B of about 581 KB: **about 166 KB free** | the `.wasm` is 1.4 MB; no practical limit |
| RAM, `.data` + `.bss` | 85,056 B of 98,304: **about 13 KB free** | up to 64 MB |
| RAM, the pool (big buffers) | 326,736 B of 344,064: **about 17 KB free**; the history takes 128 KB of it | |
| CPU | one YARD voice about 2% of a core (a six-voice chord about 12%, Alex's reading); the second core runs half the voices | a six-note chord on Clipper's heaviest sounds about 1% in Node on this Mac; the VIBE's own figure to come |

The FM-1's limit is **memory before CPU**: anything with buffers (a waveguide's delay lines, a wavetable's levels) needs
pool RAM that is spoken for. The way to free it is known (OPTIMIZATION.md): a configurable history (64 KB instead of
128: 16,384 events, still 4 to 15 minutes of playing with pressure), and Clipper's effect buffers shared when they can
never run at once. Natively, there is no such limit, and the port shares the FM-1's C sources, so anything written in
portable C lands in both.

## Attack transients: low-hanging, both places

Vane's transient layer has two kinds of sample:

- **Ten synthesised ones** (`Assets/transients/gen_*.wav`: Tongue, Key Click, Air Chiff, Wood Knock, Pick Noise, Reed
  Buzz, Metallic Ping, Finger Squeak, Bow Scratch, Snare Tick), made by `gen_inharmonic.py` from seeded noise through
  filters and fast envelopes. They need **no samples on the device**: the same recipe can run per note, in YARD, as a
  noise burst through a band-pass with its own short envelope. Cost: a noise source and one filter per voice for its
  first 20 to 80 ms; memory: a few bytes per voice. The Hub's `"sounds": "none – all synthesised"` stays true.
- **Five recorded ones** (flute, clarinet, cello, trumpet, recorder staccato attacks), CC0 from Versilian Studios
  (VSCO-2-CE, VCSL). On the FM-1 they would cost flash (about 100 KB as 16-bit mono; less at 22 kHz) and change the
  Hub's line to `"CC0: Versilian Studios (VSCO-2-CE, VCSL)"`, which it accepts. Natively they cost nothing that matters.

**Proposal:** YARD gets an ATTACK layer (the type and its level, pitched or not), the synthesised kinds first on both,
the recorded ones natively, and on the FM-1 only if Alex wants them there.

## Wavetables: natively first, then a compact FM-1 version

Vane's tables are the Adventure Kid set (AKWF, CC0: Violin, Oboe, Pluck, Stringbox, Perfect Waves) and a built-in
harmonic stack, each a run of 2048-sample single cycles kept at 11 band-limited levels (`Source/Synth/Wavetable.h`),
the morph sweeping across frames.

- **Natively: straightforward.** The same build (a DFT of each frame, a level per octave of harmonics) runs at load;
  the tables are under 120 KB as WAV, a few MB as levels, nothing for the port. A YARD oscillator choice WAVETABLE,
  POS (the morph) on a knob and on pressure or slide.
- **On the FM-1: possible, smaller.** As Vane keeps them, one table is about 1 MB of floats. A version made for it:
  256-sample frames, 16-bit, three or four levels, built for the one table in use when a sound loads; about 24 to
  32 KB of pool for a 16-frame table, so it waits on the history's 64 KB option. The tables themselves in flash: 16
  frames of 256 at 16 bits is 8 KB each, so all six fit easily in the 166 KB free.

## Waveguides: one mono voice, if the memory is found

WindCore (`Vane/WindCore/WindCore.h`, about 680 lines of C++, public since 2026-11-12) is a mono instrument: a reed
with an exact flow solve each step (a square root, an exponential), a cone with four fractional taps, a mouthpiece
chamber, tone-hole and body filters, at twice the sample rate (`kOS = 2`), its delay positions in double precision.

- **Measured** on this Mac, 200 seconds of a held note at 44.1 kHz: WindCore 1.13 s, one YARD voice about 0.053 s.
  **One WindCore voice costs about as much as 21 YARD voices.**
- **On the FM-1** that is roughly 40% of a core, before porting it the way YARD is written: single precision (the
  FM-1's FPU has no double; doubles there run in software, many times slower), the fast maths YARD uses instead of
  `std::exp` and `std::sqrt`, and perhaps one times the sample rate instead of two (about half the cost, a change of
  sound to judge by ear). A mono WIND engine beside a light chord sound looks possible on two cores; WindCore with
  Clipper's heavy chords does not.
- **Memory:** its delay buffer is 8192 floats (32 KB), more than the pool has free; sized for the lowest note it plays
  (a tenor's Ab2 needs about 1,000 samples at two times) and in 16-bit, it is a few KB. Again the history's 64 KB
  option makes the room.
- **Natively:** easily within the CPU, but the SDK builds C (freestanding: no `<cmath>`, no C++ library), so WindCore
  needs the same C port: a C file shared by the FM-1 and the port, as `app/synth.c` is. That C port is the one real
  piece of work either way, and it is also the FM-1 version.
- **What it would need first:** its tuned data (`curves.txt`, `cal.txt`, the character controls' trims) carried over
  as tables, and a check that the C port renders what Vane renders (a comparison like `make vibe-cmp`).

A simpler waveguide comes before it on the FM-1: a plucked string or a blown pipe (Karplus-Strong, a one-delay flute)
is a few lines per voice and a buffer of the note's period, polyphonic at a few percent each: a way to learn the FM-1's
budget for delay lines before WindCore's.

## The verdict

| | Natively | On the FM-1 |
| --- | --- | --- |
| Synthesised attack transients | yes, now | yes, now |
| Recorded attack transients (CC0) | yes, now | yes, about 100 KB of flash and a sounds line |
| Wavetables (Vane's AKWF set) | yes, now | yes, compact, after the 64 KB history option |
| A simple waveguide (string, pipe) | yes | yes, a few voices, after the history option |
| WindCore | yes, after a C port | a mono voice, probably on two cores, after the C port and the history option; judged by ear at one times the rate |

Natively is where Vane's voices can land first and whole; the FM-1 follows with the same code, made to fit.
