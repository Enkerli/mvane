# mvane roadmap

A custom firmware for the M-VAVE FM-1 that brings together MPE, duplex USB audio, a Coral Flower-like chord mode,
ProgGenie progressions with smooth voice leading, an always-on recorder that captures minutes after the fact,
parameter locks, file transfer with a web editor, and eventually a Vane voice (WindCore). Patches and takes should
mean the same thing on the hardware and in a plugin.

The one principle behind the layout: **musical logic lives in `core/`, portable C99 with no platform calls.** The
firmware (`app/`), the desktop tools and a future plugin all compile the same files, so a take or a patch behaves
the same everywhere.

## Where things stand (0.1, 2026-11-21)

Built and tested in fm1-base's host simulator. The device package has not been built or played yet.

| Piece | State |
| --- | --- |
| `core/harmony` | Four-voice rootless voicings over a bass note; live voice leading (`harmony_voice_live`) and whole-loop voice leading (`harmony_voice_loop`); diatonic degrees in major and minor, plus secondary dominants |
| `core/rec` | Always-on history (32-bit events, pauses of any length), "last phrase" and "everything" captures with held notes and controller values carried across the boundary, a player, the take file format (`.mvt`), CRC |
| `app/` | 8-voice saw synth with pressure-driven brightness and per-voice glide; NOTES and CHORDS modes; MPE out over USB (lower zone, 15 member channels, configured on connect); K1 tone, K2 release, K3 glide sent and recorded as CC74, CC72 and CC5, so knob gestures replay with the take; REC captures, PLAY replays, SAVE keeps the settings and the take in flash |
| Duplex USB audio | Comes with fm1-base: on by default (`usb_audio_in`, `usb_audio_out`); untested on this firmware |
| Files over USB | fm1-base's companion services: `settings` and `take.mvt`, read and write; `tools/mvt2mid.py` makes a MIDI file of a take |

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
- 0.1 uses 128 KB of pool for the history (32,768 events) and 64 KB for the take (16,384 events).
- 581 KB app flash; data area 0x97000..0xDFFFF (about 292 KB): two 68 KB take copies (A/B) live there now.
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
- Study Coral Flower's chord mode (GPL-3.0, source on GitHub) and list what to keep: layouts, inversions, strum,
  hold/latch, spread.
- Chord memory and latch; strum and arpeggio from the pressure or a knob.
- Voicing families (drop-2, spread, close), register limits, bass on/off, all through `core/harmony`.
- Black keys: today the dominant of the degree to their left; add borrowed chords (iv, bVII) as an option.

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
- The firmware already runs on a Mac in fm1-base's host simulator, and in the browser as WebAssembly. A plugin built
  on the same C (the `core/` modules plus the synth) plays the same patches and takes by construction.
- Two routes to compare: a plugin that wraps `core/` and the synth directly (in music-suite's plugin shells, next to
  Vane), or the existing FM-1 plugins if their formats can be read. To decide: which "plugin versions of the FM-1"
  you mean, and whether they load third-party patches.

### M8: a Vane voice
- A mono WindCore lead first, as `docs/starter-notes.md` planned: take the DSP out of Vane, render it at
  44.1 kHz on the desktop, then measure it alone and with chords plus USB duplex on the device. fm1-base's second
  core (`cpu2`) is the reserve if one core isn't enough.
- Delay lines go in the pool, so the recorder's history and WindCore compete for the same RAM: decide the split with
  measurements.

## Open questions for Alex
- Which FM-1 plugins: M-VAVE's own, RemoteFM1, something else?
- Coral Flower features you rely on most, so M2 starts from the right ones.
- Whether a take should default to "last phrase" (now: 3 s of silence ends a phrase) or "everything".
