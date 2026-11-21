# mvane roadmap

A custom firmware for the M-VAVE FM-1 that brings together MPE, duplex USB audio, a Coral Flower-like chord mode,
ProgGenie progressions with smooth voice leading, an always-on recorder that captures minutes after the fact,
parameter locks, file transfer with a web editor, and eventually a Vane voice (WindCore). Patches and takes should
mean the same thing on the hardware and in a plugin.

The one principle behind the layout: **musical logic lives in `core/`, portable C99 with no platform calls.** The
firmware (`app/`), the desktop tools and a future plugin all compile the same files, so a take or a patch behaves
the same everywhere.

## Where things stand (0.2, 2026-11-21)

0.1 ran on Alex's FM-1 on the day (installed with M-Patcher): "more satisfying than expected". 0.2 answers that first
session; it passes the host simulator and has not been played yet.

| Piece | State |
| --- | --- |
| `core/chords` | Chords from any pitch-class set (eight built in, including Alex's PCO and Noodling sets from the Exquis); ten types as scale steps above the root (stacked thirds, sus, quartal, 11, fifths, ...) and a chromatic V/; CLOSE, OPEN and WIDE voicings, a separate bass, voice leading from the previous chord; names from the notes |
| `core/harmony` | Whole-loop voice leading (`harmony_voice_loop`), kept for ProgGenie phrases |
| `core/rec` | Always-on history (32,768 events, 32-bit each); it sleeps, so a pause longer than 4 s costs at most 4 s of playing time; "last phrase" and "everything" captures with held notes and controller values carried across; a player; the take file (`.mvt`), CRC |
| `app/synth.c` | 8 voices, band-limited saw, key-tracked 18 dB/oct lowpass (dark to bright), attack and release, glide; MPE in: per-note bend (RPN bend range), slide (CC74 per member channel), channel and poly pressure |
| `app/` | NOTES and CHORDS (white keys degrees, black keys latched types, HOLD on F#5); MPE out over USB; knobs recorded as CC; REC, PLAY, SAVE; CPU load and xruns on screen; USB audio at full level apart from MASTER |
| Files over USB | fm1-base's companion services: `settings` and `take.mvt`; `tools/mvt2mid.py` makes a MIDI file of a take |

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
- Quantization ("full", as Alex uses it): chord changes land on a grid of the internal or external clock.
- A take replayed through other sounds: presets (M6) switchable while PLAY runs.
- Pitch-class sets from the Exquis's scale files (`<SCALE ... m2="1" .../>`) loaded over USB; more voicing styles
  (spread over three octaves, drop-2-and-4, bass an octave lower), strum and arpeggio from pressure or a knob.
- Study Coral Flower's source (GPL-3.0) for what else to keep.

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

### M8: a Vane voice
- A mono WindCore lead first, as `docs/starter-notes.md` planned: take the DSP out of Vane, render it at
  44.1 kHz on the desktop, then measure it alone and with chords plus USB duplex on the device. fm1-base's second
  core (`cpu2`) is the reserve if one core isn't enough.
- Delay lines go in the pool, so the recorder's history and WindCore compete for the same RAM: decide the split with
  measurements.

## Open questions for Alex
- Where to find VIBE FM's SDK or converter (TheFirmwareHub's documentation, or its author).
- "Full" quantization in Coral Flower: to the bar, the beat, or the next chord's slot?
- Which voicing shapes you reach for on the Exquis, to add as styles.
