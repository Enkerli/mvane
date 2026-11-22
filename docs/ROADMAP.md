# mvane yard roadmap

A custom firmware for the M-VAVE FM-1 that brings together MPE, duplex USB audio, a Coral Flower-like chord mode,
ProgGenie progressions with smooth voice leading, an always-on recorder that captures minutes after the fact,
parameter locks, file transfer with a web editor, and eventually a Vane voice (WindCore). Patches and takes should
mean the same thing on the hardware and in a plugin.

The one principle behind the layout: **musical logic lives in `core/`, portable C99 with no platform calls.** The
firmware (`app/`), the desktop tools and a future plugin all compile the same files, so a take or a patch behaves
the same everywhere.

## Where things stand (0.6, 2026-11-21)

What 0.6 does, control by control: [MANUAL.md](MANUAL.md). In short: notes or chords (any of eight pitch-class sets,
two chord banks, three voicing styles, 3 to 6 voices plus a bass, smooth voice leading, HOLD), mvane yard's saw or
Clipper's 379 factory sounds (22 engines, vendored in `app/clipper/`), MPE in and out, breath and mono legato, an
always-on history captured after the fact (never copied), eight take slots in the flash, the takes as MIDI files on a
page and from the command line, and a VIBE FM plugin build (TheFirmwareHub's converter). Device build: 370 KB of
flash, 61 of 96 KB RAM, 327 of 344 KB pool.

## Alex's first session (0.1 on the FM-1, 2026-11-21)

- Audio went to USB, speaker and headphones at once. Headphones mute the speaker (in hardware). The headphone
  sound had glitchy artefacts. **0.2:** the saw's anti-aliasing step used a reciprocal computed once a block, which
  goes wrong while a voice glides (fixed); CPU load and xruns are now on screen, to tell a render overrun from
  anything else. To check on the device: are the artefacts gone, and does XRUNS stay at 0?
- USB and speaker usually have separate levels. **0.2:** USB audio at full level, MASTER for the speaker and
  headphones (GLO toggles; on by default).
- The history kept running while idle. **0.2:** it sleeps.
- Capture should be as long as wanted, since memory is about events. **0.2:** a take can hold nearly the whole
  history (30,000 events). REC tap stays "last phrase"; REC hold "everything" stays too ("really neat").
- The tone knob stayed bright. **0.2:** the filter follows the note from under the fundamental to seven octaves
  above it, 18 dB/oct; attack on K4 for softer starts.
- The synth took MPE but not per-note bend, and slide wasn't per note. **0.2:** both.

**Second session (0.2 on the FM-1).** Crackling got worse, and went away with the volume down; no xruns, CPU 11%
even with 11th chords. Cause: MASTER was applied inside the render before a hard clamp at full scale, so a five-voice
chord at full volume clipped. **0.3:** each voice at about half the old level, and a soft limiter (clean to half
scale, smooth to 0.95) instead of the clamp; the worst case measured (six voices plus bass, velocity and pressure
full, MASTER up) peaks at -4 dBFS. MPE works; slide was too subtle (the Exquis's Y axis rests at 64 and goes both
ways). **0.3:** slide is bipolar around 64, four octaves of cutoff either way. Tone is better but could go further.
**0.3:** a wider, S-shaped range. Chords: "don't dislike them, but they sound quite different from Coral Flower";
richer chords wanted. **0.3:** 3 to 6 upper voices plus the bass (LFO), STACK reaching 11 and 13. The VIBE FM
converter failed on `memset`/`memcpy`/`memcmp` (see M7).

**Third session (0.3 on the FM-1).** Distortion still came with velocity. In the simulator the output is clean at
velocity 127 (energy off the chord's harmonics about -51 dB, no clipping), and Alex's recording isn't clipped either,
so the likely cause is the FM-1's analog output running out of headroom: 0.3 peaked at -4.5 dBFS in the worst case.
**0.4:** about 5 dB lower (worst case -9.3 dBFS), velocity over about 8 dB of level instead of 12, a little more of it
in brightness; pressure, slide and bend smoothed in two stages and the filter updated every 16 samples. Coral Flower's
top row, per its display: M6 M7 M9 S2 S4 m6 m7 m9 M m; its quantization goes off, basic, full. **0.4:** those ten as
a second bank of fixed qualities on the black keys (ENV switches banks; on by default). Quartal (the scale bank) is
"likely what I want". Coral Flower was itself inspired by Telepathic Instruments' Orchid. Name: **mvane yard**.

**Fourth session (0.4 on the FM-1).** "Getting somewhere": the Coral Flower qualities sound as they should. A tiny
bit of distortion remains through headphones at full volume; acceptable, to keep in mind (the headphone stage's
headroom: a per-output level or a limiter on the headphone path if it matters later). The VIBE FM conversion now
compiles everything and stops at the link: its platform has no `plat_file_*`. **0.4.1:** under WebAssembly (not
fm1-base's own emulator) the take stays in RAM, so the plugin build captures and replays but doesn't keep a take
between sessions; next, `vibe_host_store` for that.

**Fifth session (the plugin and files).** The VIBE FM conversion worked and the plugin plays as expected. The take
came into Bitwig as 15 clips (a clip per MPE channel). **0.5:** Download as MIDI puts everything on one channel (each
note's pressure as poly aftertouch, slide and bend on channel 1), MPE channels kept as an option; eight take slots in
the flash (SEQ / HOME), on the page and in `yard.py`; mono legato and breath (CC2 / CC11) on SEL. Next asked: more
sounds, starting with the FM-1's tried and true engines and patches (Coral Flower's VA, then its FM; Clipper's), so a
looping take can be tried through different patches, with slide and pressure routed in each; later a minimal Vane
(its wavetable engine first).

**Sixth session (0.6: Clipper's sounds).** Works well with the Diosynth (breath). Asked, in Alex's words and
order:
- **Rethink the knobs and buttons.** Done in 0.7 ([PANEL.md](PANEL.md)); QUANT is in place, not acting yet. SEQ / HOME are not how FM-1 firmwares move through clips: SELECT does, in the
  SEQ context. In play, K1-K4 should make sense together: one page as Coral Flower's (QUANTIZE, KEY, SCALE, GLIDE),
  another for the synth's macros (Clipper's BRITE, TIMBR, MOTN, SPACE). Proposal in [PANEL.md](PANEL.md).
- **Keep both chord banks** and a clear way to switch (ENV does it in 0.6, without saying so on screen: see
  "Discoverable").
- **Files in the plugin**: settings above all. VIBE FM's platform imports `vibe_host_store`, likely the firmware's
  store kept with the DAW project; to test. Fallback: SysEx in a MIDI clip (Bitwig is weak with SysEx).
- **Streamline Clipper's engines and sounds**: fewer models, similar ones merged (below).
- **Display a chord's structure as a piano roll** (the voices on a small roll: what moved, what held).
- **Standalone / plugin build, file-compatible** with the device (beside VIBE FM: M7).
- **USB MIDI host**: the Diosynth straight into the FM-1's USB-C. In 0.7: GLO held, SELECT (the Diosynth has a battery).
  First try (2026-11-21): stuck on LOOKING, no device. The Diosynth WS-1 (AshunSoundMachines, 0a67:7025) is fine for
  felucca-bsp's parser: a 285-byte configuration (the limit is 320), audio control and two audio streaming interfaces,
  then USB-MIDI on interface 3 with bulk endpoints of 512 (high speed), which the parser treats as 64 at full speed;
  self-powered, 20 mA. So the likely cause is power: a device waits for the host's 5 V before it connects, and the
  FM-1 gives none. Next: through a hub with its own power supply (one chip: fm1-base takes one hub, not a hub inside a
  hub as Melodee 1.6 does), or an OTG adapter that brings 5 V to the device. fm1-base has it (`usb_host = true`,
  `plat_usb_host`): the FM-1 hosts a USB-MIDI device or one behind a powered hub. The FM-1 gives no power, so the
  Diosynth needs its own battery or a powered hub; while hosting, the computer's USB (audio, MIDI, files) is off.
- **Document everything** (now: [MANUAL.md](MANUAL.md)).
- **More discoverable UI** (soon): what each control does on screen when it is touched, the chord banks named.
- **Smooth voice leading as in music-suite**: its reference is `packages/theory/src/voiceLeading.ts`, Tymoczko's
  minimal taxicab voice leading between pitch-class sets (the dynamic program over cyclic rotations, doublings
  allowed), with three modes in `packages/proggen/src/generate.js` `voiceProgression`: none (home position), loose
  (home position moved to the nearest register), strict (minimal taxicab motion). mvane yard's `core/chords` searches
  voicings within a style's limits and scores each note's distance to the previous chord's nearest: close in spirit to
  strict, but not the same algorithm. To do: port voiceLeading.ts to C in core/ against music-suite's shared vectors
  (`vectors/voice-leading.json`), and offer the three modes.
- **Submit to the FM-1 Firmware Hub**, eventually: <https://thefirmwarehub.com/submit/> (fm1.toml's install, repo and
  screenshot fields, a release build, firmware.json).

**Seventh session (0.7).** Clipper's sounds: some don't play at once (a top-row key first, then the bottom rows sound);
xruns on some; many near 50% CPU, mono ones too, where Coral Flower's VA stayed under 20% with ten voices. Drop ACID,
NOISE, LOFI for now (done in 0.8); several of the other proposed drops Alex likes; tired of most subtractive sound,
some analog-style engines repetitive: HYBRID may cover JUNO and ANALOG; STRINGS good here. Be clearer about each
sound's cost. VIBE FM saves the plugin's state; the converter failed on 0.7 (`duplicate symbol: bootguard`, Clipper's
core.h global: renamed in 0.8, clp.o now exports clp_* only). Coral Flower's QUANT is to the scale, not in time
(0.8, as its spec: OFF chromatic roots, BASIC degrees, FULL diatonic tones). Alex's voicing, especially for sevenths:
3rd and 7th in the left hand, root and 5th in the right, two pairs of fifths (0.8: PAIRS). Hubs: Alex's USB-C hubs
may not pass host to device apart from a computer; further investigation.
**0.8** also runs Clipper on both cores (fm1-base's second core, as Clipper's own split of the voices).
Open: the sounds that wait for a top-row key do not do it in the simulator (every sound plays from a bottom-row key
at velocity 60 straight after a change): to find on the FM-1 (which sounds, NOTES or CHORDS, how soft the press).

**How Alex plays Coral Flower** (the model for CHORDS): arbitrary keys for a succession of arbitrary chords that end up
going well together; full quantization; some glide; chord types on the black keys ("top lock"), latched; HOLD on the
top row's last key (F#5). Then a simple recorded sequence is replayed with different presets, preferably ones
without a strong attack. Wanted beyond it: no startup chime (mvane has none), USB audio, other pitch-class sets (PCO
and Noodling), more chord types and voicings, very broad open ones especially; the separate bass is useful.

## The platform choice

fm1-base was chosen for duplex USB audio, the host simulator and browser emulator, the companion services (files and
parameters over USB) and the documented memory budgets. It is MPL-2.0 file by file, so this app can be GPL-3.0,
which lets us borrow from Clipper and Coral Flower (both GPL-3.0).

**Fallback: Clipper.** If fm1-base gets in the way (the audio engine budget, MPE input details, or a Clipper engine we
want whole), rebasing onto Clipper is a real option. Because `core/` is platform-free, a move costs the `app/`
glue, not the musical work. Triggers for switching, so the decision is made on evidence:

- the synth plus USB duplex doesn't fit the render budget on fm1-base, where Clipper's engines do;
- we want Clipper's sound engines (HYBRID, FMX, FM6) and their patch formats more than a new engine;
- the web editor's Clipper compatibility is worth more than fm1-base's companion services.

**Licence catch:** fm1-base's default Bluetooth links JieLi's closed libraries, which a GPL firmware shouldn't
include. Bluetooth here means fm1-base's own stack (`ble_stack = "own"`) or none. 0.1 has none.

## Memory budget (fm1-base's numbers, to be measured on this build)

- 96 KB RAM for `.data`/`.bss`, a 336 KB pool for big buffers (the template without radio uses 68 KB of pool).
- 0.2 uses 128 KB of pool for the history (32,768 events) and 117 KB for the take (30,000 events). 0.1 measured
  270 of 344 KB of pool with a 64 KB take, so about 323 KB now: little room left in the pool for WindCore's delay
  lines until takes move to flash pages (M1).
- 581 KB app flash; data area 0x97000..0xDFFFF (about 292 KB): two 120 KB take copies (A/B) live there now.
- What 32,768 events buys: notes alone, roughly an hour of noodling; notes plus pressure, which dominates, about
  8-30 minutes depending on how much pressure moves. Pressure is thinned to 50 updates a second per key with a
  2-step deadband; in CHORDS mode one pressure stream serves the whole chord (MPE master channel).

## Milestones

### M0: first light on the device (now)
- [ ] Build the `.fwsc` (CI, a Linux PC, or a container on the Mac: see README).
- [ ] Install with M-Patcher; back up the current firmware first (Coral Flower's installer backup exists in Downloads).
- [ ] Play both modes. Check MPE out in Bitwig and in Vane (MPE on). Check that a take survives a power cycle.
- [ ] Measure: render time with 8 voices (`plat_cpu_pct`, `plat_xruns`), flash and RAM from the build log.
- [ ] USB duplex: record the FM-1 in Bitwig while playing a backing track through it; the capture should exclude
      the backing track.

### M1: the recorder you can rely on
- Longer captures: keep the take in the data area in pages, not one 64 KB block. That allows several takes and
  takes as long as the history (the data area can hold about 250 KB of takes).
- Record **gestures**, not only notes: chord key, degree and voicing choice as their own events, so a captured
  chord take can be re-voiced or transposed later. MIDI stays the export.
- Overdub and loop: PLAY loops the take; REC during playback layers onto it.
- MIDI in recorded with the controller's timing (`plat_midi_in_t24`), read in the main loop now.
- History length on screen in minutes, and a warning before it starts dropping its oldest events.

### M2: chord mode, Coral Flower-like
- Done in 0.2: latched types on the black keys, HOLD on F#5, pitch-class sets, CLOSE/OPEN/WIDE, bass on/off.
- Quantization as Coral Flower's: off, basic, full (Alex uses full): chord changes land on a grid of the internal or
  external clock.
- A take replayed through other sounds: presets (M6) switchable while PLAY runs.
- **Any pitch-class set by its number**, music-suite's convention (bit 0 the root, C major 2741 = `0xAB5`; Alex's
  PCO is 1723, Noodling 1267): entered on the device, sent as a companion parameter, or read from the Exquis's scale
  files (`<SCALE ... m2="1" .../>`). 0.4 shows the current set's number on screen. More voicing styles
  (spread over three octaves, drop-2-and-4, bass an octave lower), strum and arpeggio from pressure or a knob.
- Study Coral Flower's source (GPL-3.0) for what else to keep.

### M2b: chord memory and pads (Alex, second session)
- A progression made and voiced on the device (smooth voice leading, drop voicings, any style), saved, then played
  from the **bottom row: 16 chord pads**, the octave buttons paging to more sets, like the chord-pad plugins Alex uses.
  The top row varies things (inversion, spread, type, strum).
- Pads keep their exact voicing; editing one re-voices its neighbours only on request.
- The arpeggiator generates patterns from the pad's chord: basslines plus comping.
- Saved as files over USB (M6) and shared with the plugin (M7).

### M2c: why it sounds different from Coral Flower
- Read Coral Flower's chord engine (GPL-3.0) for its voicings, register and doublings, and render the same
  progression through both (VIBE FM runs Coral Flower) for an A/B by ear.

### M2d: streamlining Clipper's engines and sounds

0.6 carries 22 of Clipper's 23 engines and their 379 sounds (WAVE left out for RAM). Proposal, to agree with Alex:
keep the ones that suit chords, keys and leads, merge or drop the rest.

| Keep | Why |
| --- | --- |
| HYBRID | Clipper's own voice: pads, strings, keys, leads, bells |
| JUNO | Juno pads and strings, its chorus |
| STRINGS | string machines |
| ANALOG | warm pads, basses, leads |
| FM6 / FMX | FM keys, bells, pads (merge into one list: same family) |
| DIGITAL | electric pianos, bells (merge with FM?) |
| WHEEL | organs |
| VOICE | choirs |
| PHYS | plucks, bells, bowed (some are percussion: FX) |
| MACRO | chords, swarm, additive |

| Drop or fold in | Why |
| --- | --- |
| ACID, NOISE, LOFI, TRIO | basslines, noises, chiptune: a groovebox's |
| GRAIN, SAMPLE | synthesized sources, few sounds |
| VPS, TERRAIN, PHASE | experimental timbres; a few sounds could join a "TEXTURES" list |
| WSEQ, VECTOR | wave sequencing, vector: interesting, heavier to drive without their editor |

Each sound's cost on screen (its CPU, measured, as a bar or a word) would make performance clear. A curated list of sounds by kind (pads, keys, strings, bells, leads, organs, choirs, basses) on PRESETS, the rest in
an "ALL" list, is the likely shape. Dropping engines also frees flash and RAM (WAVE could come back).

### M3: ProgGenie on the device
- Port ProgGenie's engine to C in `core/proggen`: corpus transition tables (compact, in flash) and the curation
  weights (`packages/proggen/src/curation.js` in music-suite). Phrases come out as chord events that
  `harmony_voice_loop` voices for the whole loop, last chord to first included.
- Keys pick or nudge the next chord; a thumbs-up or thumbs-down on a change updates its weight, kept in flash and
  shared with the plugin as a file.

### M4: parameter locks and gestures
- Every knob change is already an event in the history. Next: per-step locks in a take (a knob turned while the
  take plays writes into it), and "latch this gesture": capture one knob's movement over a phrase as a loopable lane.
- Glide, tone, release first; then whichever synth parameters exist by then.

### M5: MPE in
- Clipper's MPE (GPL-3.0) as a reference: member-channel bend, pressure and CC74 from an Exquis, zone
  configuration, bend range. Map it per voice in the synth, and record it as is.
- Breath (CC2) from a wind controller as a gain and brightness source.

### M6: patches, files and the web editor
- Done (2026-11-21, after the plugin conversion worked): the take over USB, both ways. `web/index.html` (Connect,
  piano roll, Download as MIDI or .mvt, send a MIDI or .mvt file as the take, settings backup and restore) and
  `tools/yard.py` (the same from the command line), on fm1-base's companion services; tested against the browser
  emulator and the host simulator. Published on GitHub Pages: https://enkerli.github.io/mvane-yard/ (`.github/workflows/pages.yml`, on each push to `web/`).
- Next: several takes (a list the page shows and names), the take's tempo for the MIDI file once there is a clock,
  and the history itself as a file (everything played since power-on, not only what REC kept).
- A versioned patch format: engine ID, stable parameter IDs, expression routes. JSON in the editor, compact binary on
  the device.
- A web editor on WebMIDI and fm1-base's companion services: patches, takes (with MIDI export in the page), settings,
  backup. fm1-base's `web/companion` page is the starting point; Clipper's editor is the model for the UI.
- **Batch operations** in the editor: "route MPE slide (CC74) to shape/timbre on every patch", "set glide on all",
  run against the whole library on the computer, then sent back. That is easier and safer on the computer than on
  the device.

### M7: one patch, two places (the plugin)
- Alex mostly uses **VIBE FM VST** (from TheFirmwareHub), also m1vst (FM1VST) and Virtual FM-1. VIBE FM loads
  firmwares converted to WebAssembly: `~/Library/Application Support/VIBE FM/Firmware/` holds `clipper.wasm` and
  `coralflower.wasm` with a JSON manifest each (`"protocol": 7`, "converted (tools/fm1)"). Their exports are a
  `vibe_wasm_*` interface (init, audio, MIDI out, parameters, editor, draw, LEDs, store) and they import only
  `vibe_host_log` and `vibe_host_store`.
- So the most direct route: build mvane for VIBE FM, the same C as the hardware. Patches and takes then mean the same
  thing in both by construction. Needed: VIBE's SDK or the `tools/fm1` converter (TheFirmwareHub), and a mapping
  from fm1-base's `plat.h` to `vibe_wasm_*`. fm1-base's browser emulator (`--web`) is already a WebAssembly build of
  the same app, which should make the mapping small.
- Fallback: a plugin of our own around `core/` and `synth.c` in music-suite's plugin shells, next to Vane.
- First attempt (2026-11-21) failed in TheFirmwareHub's converter: `tools/fm1/dinghy/libc/string.h` conflicts with
  `memset`, `memcpy` and `memcmp`. Those definitions are felucca-bsp's (`base/bsp/chip/libc.c`, with `unsigned n`
  where the converter's libc has `size_t`), not mvane's: mvane never declared `memcmp`. Since 0.3, `core/` declares
  no library function at all. The remaining clash is between felucca-bsp and the converter, to report to
  TheFirmwareHub (any fm1-base firmware would hit it), or to work around with a build of our own for VIBE.
- Why Clipper and Coral Flower convert: Clipper's own `firmware/src/libc.c` has the same `unsigned n` signatures, so
  the converter must leave that file out by name. In fm1-base it can't, because `device/unity.c` does
  `#include "libc.c"` (felucca-bsp's). Workaround with a real chance: declare the three in felucca-bsp's
  `chip/libc.c` with `__SIZE_TYPE__ n`, which is every compiler's own `size_t` (`unsigned long` in WebAssembly,
  what the JieLi compiler expects on the device): a fix upstream (hugelton/felucca-bsp, then fm1-base's pin), or in
  forks that mvane yard's submodules point to until then. (Forks made on Alex's OK, Enkerli/felucca-bsp and
  Enkerli/fm1-base, then not needed: the next conversion got past the clash. They are untouched copies.)

### M8: a Vane voice
- A mono WindCore lead first, as `docs/starter-notes.md` planned: take the DSP out of Vane, render it at
  44.1 kHz on the desktop, then measure it alone and with chords plus USB duplex on the device. fm1-base's second
  core (`cpu2`) is the reserve if one core isn't enough.
- Delay lines go in the pool, so the recorder's history and WindCore compete for the same RAM: decide the split with
  measurements.

### M9: shaping a chord's spectrum (Alex, 2026-11-21)
- Each chord voice gets its own filter, and the filters morph together: a curve across the chord (by voice, pitch
  or register) that moves as one gesture, so the chord's overall spectrum is shaped rather than each note alone.
  Pressure, slide, a knob or a take's recorded gesture drives the morph.
- Especially in **just intonation**: chord voices tuned to simple ratios over the bass, so their partials coincide.
  Per-voice filters can then bring out or soften the shared partials, colouring the chord as one sound. Ties into
  Vane's tuning work and just-interval chording (Vane priorities: pitch input, JI chords).
- Needs: a tuning table per chord (12-TET or JI from the root or bass), voice pitches as ratios in the synth (MPE
  out then carries the JI offsets as per-note bend), and a filter per voice whose cutoff and resonance follow a
  chord-wide morph curve.

## Panel conventions

The 0.x panel is improvised (FX for voicing, LFO for voice count, EDIT for bass, GLO for USB level, SELECT for the
scale). Before 1.0, map controls the way FM-1 players expect from M-VAVE's firmware and the popular custom ones
(Clipper, Coral Flower): the printed labels, shift layers and pages, so that nothing surprises.

## Name

**mvane yard** (Alex, 2026-11-21): "mvane" alone read like a typo for M-VAVE. Package identity unchanged (`FM-1_17...`).

## Open questions for Alex
- Where to find VIBE FM's SDK or converter (TheFirmwareHub's documentation, or its author).
- "Full" quantization in Coral Flower: to the bar, the beat, or the next chord's slot?
- Which voicing shapes you reach for on the Exquis, to add as styles.
