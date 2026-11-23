# mvane yard on VIBE FM, native

A port of the FM-1 firmware to the VIBE FM SDK (<https://github.com/Vibe-FM-SDK/vibe-fm-sdk>), following the Hub's
guide ([FROM-FM1.md](https://thefirmwarehub.com/lab/docs/port-from-fm1/#step-by-step)): one `.wasm` that plays as a
VST3 / AU / CLAP plugin (VIBE FM VST), in the browser (the Playground) and on the VIBE FM hardware. It shares the FM-1
firmware's sources: `app/synth.c` (YARD) now, `core/` (chords, the recorder) next, so a fix lands in both.

## Build

Needs the SDK (a clone; `VIBE_SDK`, by default `../vibe-fm-sdk-ref/sdk`) and Zig (0.13.0 as the SDK says; 0.17.1
works too).

    make vibe        # vibe/build.sh: build/mvane-yard.zip in the SDK's folder, then vibe-check (PASSED)
    make vibe-cmp    # YARD in the .wasm against YARD as the FM-1 computes it: bit for bit at 44.1 kHz

Drop `build/mvane-yard.zip` (in the SDK's folder) onto VIBE FM VST's window, or the `.wasm` onto the Playground
(<https://play.thefirmwarehub.com/>).

## Releases

Every release carries **`mvane-yard-vibe.zip`** (a fixed name: `mvane-yard.wasm`, its `.json` with the release's
version, `mvane-yard.png` at the top) and `mvane-yard-vibe.wasm` beside it: The Firmware Hub's native card takes
them (Sven, 2026-11-23). `vibe/ci.sh` builds them on Linux with Zig 0.13.0 and the SDK at a fixed commit, both
checked, then runs vibe-check; the CI does the same on every push (the zip in the run's artifact). The card's
picture, `vibe/mvane-yard/icon.png` (960 × 540): `tools/vibe_icon.py`.

## The steps (FROM-FM1.md)

1. **Done:** a skeleton that makes the sound. YARD on the keys, MIDI and the DAW's notes, each on its own channel
   (MPE: its bend, pressure, slide); encoders 1–4 TONE ATTACK DECAY RELEASE, A the sound (YARD's thirteen); HOME the
   controls. The host's sample rate (`synth_rate`); the audio in its own flow (the events queue MIDI messages, the
   audio alone touches YARD). YARD's code is built with precise maths (`#pragma clang fp`), as the FM-1 builds it:
   at 44.1 kHz the same samples (`make vibe-cmp`).
2. **Done:** the controls, the VIBE's own (no imitated FM-1 keys):
   - **keys**: NOTES, or CHORDS as on the FM-1 (Coral Flower's): the 15 white keys the scale's degrees, higher up
     the keyboard; the 10 black keys the qualities (M6 M7 M9 S2 S4 m6 m7 m9 M m), latched (pressed together:
     combined; the one latched alone again: none, single notes). `core/chords.c` builds and voices them, each voice
     on its own MPE channel, common tones held. OCT moves the chords' register with the keyboard.
   - **K1–K6**: SOUND (YARD's values, F6 their next four), PLAY (QUANT KEY SCALE GLIDE), CHORD (VOICING VOICES BASS
     BANK), ARP and SEQ (steps to come), SCALE (the set's number, a digit an encoder); encoders 1–4 the page's values.
   - **F1 F2 F3**: NOTES / CHORDS, HOLD, POLY / MONO / +BREATH. **A** the sound, **B** the scale (bright to dark).
   - MIDI and the DAW's notes as notes on their channels; QUANT FULL snaps them to the scale.
   - `vibe/chords.json` (wasmtest) plays and draws Cmaj7, C6, D6, Dmaj9 (M7 + M9), the CHORD page, NOTES.
   - **The pads** (Alex: the recording slots), a 4 × 4 grid: rows 1–2 the eight slots (a tap goes to it; on the
     open one, PLAY), row 3 REC (tapped, held, as the button) PLAY STOP LOOP, row 4 SAVE UNDO HOLD ARP; lit as they
     stand (the slots holding a recording dim, the open one bright, PLAY while it plays, REC while it overdubs).
     `vibe/pads.json`: kept, looped, overdubbed, undone, saved, another slot and back, all on the pads.
   - Not yet: ENV and LFO (the house's K2 and K3 are PLAY and CHORD here for now).
3. **Done:** the pages, drawn whole each time on 480 × 320 in the house layout: the page in its colour, NOTES /
   CHORDS and the sound; K1–K6 as tabs, the open one lit; the page's own (PLAY: what sounds on a five-octave keyboard,
   the scale's tones marked, the tonic in colour, the bass in red; CHORD: the chord, its voicing, the ten qualities,
   the latched lit; SOUND: the sound, its three pages of values; SCALE: the number in big digits, its name, its twelve
   tones from the key); the key, scale, QUANT and play mode; encoders 1–4's values with bars; F1–F6's captions
   (F1 says NOTES or CHORDS, F3 the play mode, F2 lit while HOLD). `vibe/pages.json` (wasmtest) shoots every page;
   the longest draw 0.7 ms in Node.
4. **Done:** FOCUS (SDK 10): every control on the screen reachable with DATA (it walks, in reading order: the sound,
   the six pages, the page's own, the scale, the four values, the F keys) and ENTER (presses, or edits: DATA turns
   it then). CHORD's ten qualities are buttons (ENTER: as a black key tapped, Coral Flower's latch), SCALE's eleven
   tones above the key too (ENTER adds or takes one). DATA has no other job here, so it always walks (`VF_ALWAYS`);
   in the plugin and the browser a click on a control does the same (`vf_clicks`). `vibe/focus.json` walks to M7 and
   presses it.
5. **Done:** the DAW. 26 parameters, their ids permanent (automation hangs on them): Editor tab A, YARD (the sound
   by name, the play mode, its thirteen values: WAVE and FILTER as their three choices), tab B, the harmony (NOTES /
   CHORDS, the key, QUANT, the scale's number, voicing, voices, bass, the black keys' bank, HOLD). A change from the
   DAW is applied as the panel would (a sound brings its values, which go back to the DAW); after every event, what
   moved on the panel goes to the DAW (`params_push`). The DAW's tempo and place (`vibe_transport`) show on ARP and
   SEQ, and a green > in the header while it plays: the ARP and the loops will follow them (steps 7, 8).
   `vibe/daw.json` (wasmtest) sets the key, the sound and the scale from outside, with a DAW at 96 BPM.
6. **Done:** storage, in the FM-1's own files. `settings`: the FM-1's 52-byte record (now `app/settings.h`, shared
   by the two): the mode, the key, the scale, the sound and its values as left, QUANT, the chord's voicing, the play
   mode, the octave are read and written; the FM-1's own (PHONES MAX, the ARP's, MPE OUT ...) are kept as they came,
   so the file moves between the two. `yard.ysb`: the bank, version 3, byte for byte (0.10's and 0.11's grow to it);
   your YARD sounds come after YARD's thirteen on A (and in the DAW's Sound), your scales after the named list on B;
   the Clipper part is kept as it is, for when Clipper comes. **F4 KEEP**: on SOUND these values as yours (USER n),
   on SCALE the number in your list (as the FM-1's FX + SAVE and SEL + SAVE). Saved a second after a change, and at
   once on VIBE_EV_SAVE (the DAW keeps its project). `vibe/store1.json` then `vibe/store2.json` (wasmtest, `"store"`):
   keep a sound and a scale, change the key; the second run finds them; `tools/ysb.py` reads the bank. Recordings
   (`.mvt`) come with the recorder (step 7).
7. **Done:** the recorder, the FM-1's (`core/rec.c`), all in the audio flow, which alone writes the history: what
   the events queue is played and kept there on the audio's clock (frames as ms); the player plays a recording
   there too, 32 frames at a time, in the key and scale now set (a note to the same degree); LOOP; overdub (REC
   while it loops: a layer a pass, REC held undoes the last); the eight slots as the FM-1's files `take1.mvt` ...
   `take8.mvt` (the SEQ page: SLOT, LOOP, F4 SAVE; a slot left with a recording not saved warns first). **REC**
   tapped: the last phrase, held: everything; **PLAY**, **STOP**. The events only send commands and read what the
   audio publishes; to save, the audio copies the recording into a file image and plays from it, the events write
   that file. `vibe/rec1.json` then `rec2.json`: two chords kept with REC, played, looped with an overdub (three
   layers), saved; found again. Not yet: MIDI in moved to a free channel while a recording plays (the FM-1's 0.13.2,
   for a wind controller's overdubs); SAVE's copy spread over blocks (now one: 17 % of a block at most in Node).
8. **Done:** the ARP, the FM-1's line for line, in the audio flow: it takes the notes that would sound (the keys,
   the chords' voices, MIDI in) and plays them a step at a time on their channels (MPE follows), into YARD and the
   history (a recording keeps it, plays back as it was). The ARP page (K4): MODE (UP, DOWN, UP-DOWN, DOWN-UP,
   UP+DOWN, RANDOM, AS PLAYED, CHORD), RATE (1/4 .. 1/32, triplets), GATE, OCTAVES; F6 MORE: TEMPO, LATCH; K4 again
   or F4: on, off. Its own tempo; while the DAW plays, the DAW's, each step on the DAW's grid (`vibe/arp_daw.json`:
   at 90 BPM, 1/16, the steps at 0.632, 0.800, 0.968 s ... against 0.633, 0.800, 0.967 on the grid). Seven DAW
   parameters (Editor tab C); the settings' ARP fields are the FM-1's. `vibe/arp.json`: on, a chord, LATCH.
9. **Done:** Clipper's sounds, its own unit (`vibe/mvane-yard/clipper.c` around `app/clipper`, the FM-1's vendored
   copy, unchanged; `vibe/build.sh`: the SDK's build with Clipper's include paths). It runs at its own 44.1 kHz,
   32 frames at a time into a ring read out at the host's rate (cubic Hermite; at 44.1 kHz its samples as they are),
   one core (the FM-1's second core answered "none"). A steps through YARD's thirteen, your YARD sounds, Clipper's 337
   by family, your Clipper sounds; the engine switches with the sound (the one sounding let go), the notes, the
   player and the ARP reach whichever plays. SOUND on a Clipper sound: its macros BRITE TIMBR MOTN SPACE, F6 its
   sends DRIVE CHORUS DELAY REVERB (as the FM-1's CCs, recorded); F4 KEEP: its macros, sends and reverb as yours,
   in the bank's Clipper part, as the FM-1's FX + SAVE. The settings keep a Clipper sound by the FM-1's codes.
   `vibe/clipper.json` (48, 44.1 and 96 kHz: the same level), `vibe/clipper_keep1.json` then `clipper_keep2.json`
   (GOSPEL+ with its reverb send kept, read back by `tools/ysb.py`, found again). A six-note chord on Clipper's
   heaviest: about 1 % of the audio's time in Node on this Mac; the VIBE's own figure to come.
