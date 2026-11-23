# mvane yard on VIBE FM, native

A port of the FM-1 firmware to the VIBE FM SDK (<https://github.com/Vibe-FM-SDK/vibe-fm-sdk>), following the Hub's
guide ([FROM-FM1.md](https://thefirmwarehub.com/lab/docs/port-from-fm1/#step-by-step)): one `.wasm` that plays as a
VST3 / AU / CLAP plugin (VIBE FM VST), in the browser (the Playground) and on the VIBE FM hardware. It shares the FM-1
firmware's sources: `app/synth.c` (YARD) now, `core/` (chords, the recorder) next, so a fix lands in both.

## Build

Needs the SDK (a clone; `VIBE_SDK`, by default `../vibe-fm-sdk-ref/sdk`) and Zig (0.13.0 as the SDK says; 0.17.1
works too).

    make vibe        # build/mvane-yard.zip in the SDK's folder, then vibe-check (PASSED)
    make vibe-cmp    # YARD in the .wasm against YARD as the FM-1 computes it: bit for bit at 44.1 kHz

Drop `build/mvane-yard.zip` (in the SDK's folder) onto VIBE FM VST's window, or the `.wasm` onto the Playground
(<https://play.thefirmwarehub.com/>).

## The steps (FROM-FM1.md)

1. **Done:** a skeleton that makes the sound. YARD on the keys, MIDI and the DAW's notes, each on its own channel
   (MPE: its bend, pressure, slide); encoders 1–4 TONE ATTACK DECAY RELEASE, A the sound (YARD's thirteen); HOME the
   controls. The host's sample rate (`synth_rate`); the audio in its own flow (the events queue MIDI messages, the
   audio alone touches YARD). YARD's code is built with precise maths (`#pragma clang fp`), as the FM-1 builds it:
   at 44.1 kHz the same samples (`make vibe-cmp`).
2. The controls: NOTES and CHORDS (`core/chords.c`), the black keys' qualities, the scale; the six keys right of
   ENTER as SOUND · ENV · LFO · ARP · FX · SEQ; F1–F6.
3. The pages, on the full 480 × 320.
4. FOCUS (DATA + ENTER reaches every control).
5. The DAW: YARD's parameters (automatable), the ARP and the loops on the song position.
6. Storage: settings, your sounds and scales (`yard.ysb`), recordings (`.mvt`), as on the FM-1.
7. The recorder (`core/rec.c`) on the audio's clock: REC, loops, overdub.
8. Clipper's sounds (its engines are built for 44.1 kHz: at that rate, resampled, or their tables at the host's).
