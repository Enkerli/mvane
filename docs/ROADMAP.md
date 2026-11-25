# mvane yard roadmap

A custom firmware for the M-VAVE FM-1 that brings together MPE, duplex USB audio, a Coral Flower-like chord mode,
ProgGenie progressions with smooth voice leading, an always-on recorder that captures minutes after the fact,
parameter locks, file transfer with a web editor, and eventually a Vane voice (WindCore). Patches and takes should
mean the same thing on the hardware and in a plugin.

The one principle behind the layout: **musical logic lives in `core/`, portable C99 with no platform calls.** The
firmware (`app/`), the desktop tools and a future plugin all compile the same files, so a take or a patch behaves
the same everywhere.

## Where things stand (0.11, 2026-11-22)

What it does, control by control: [MANUAL.md](MANUAL.md); every sound: [SOUNDS.md](SOUNDS.md). In short: chords from
any pitch-class set on the white keys, qualities latched (and combined) on the black keys as on Coral Flower, single
notes, four voicing styles led smoothly, a separate bass; an always-on history captured after the fact, eight take
slots, takes as MIDI on the page and the command line; YARD (mvane yard's own light synth: saw, pulse, triangle,
noise, three filter modes, PLUCK) and Clipper's 337 sounds, in families by use and load (CHORDS, LEADS, HITS; EZ,
HRD), on both cores, trimmed under the limiter; your own YARD and Clipper sounds saved on the panel (EDIT on the
SOUND page) and moved as files; MPE in and out, breath, PHONES MAX for the headphones; a landing page with the USB
tools (https://enkerli.github.io/mvane-yard/).

## Submitting to the Firmware Hub (https://thefirmwarehub.com/submit/)

| What the hub asks | Where it stands |
| --- | --- |
| Open source, the source at each release's tag | GPL-3.0, public |
| A GitHub release with the `.fwsc` (pre-releases too) | v0.9-beta, v0.10-beta; built and signed from source (zvenson/fm1-build: "Built from source on GitHub") |
| Its own identity (`FM-1_` number, not the one it started from) | `FM-1_53` + the version's six digits (the form's check, 2026-11-22: free from 0.00 to 9.99); `FM-1_17`, fm1-base's template's, until 0.11-beta, is inside LOOM-8's range |
| The update loader in the package | felucca-bsp's, in every fm1-base package |
| Both flash chips (`0x856014`, the newer `0xB36014`) | fm1-base's `fm1_flash_ok.h` (Alex's unit is a newer one) |
| `fmhub.json` in the root (what, kinds, image, sounds ...) | written: `chords`, `docs/media/card.png` (240 x 240), the page as its editor, the manual as its cheat sheet |
| Sounds: what ships | `none – all synthesised`: Clipper's synthesized sets only; Felucca's recorded sets (never compiled here) taken out of the vendored copy |
| Tested on a real FM-1: installed from V15 and from another firmware, and left again | Alex, 0.11-beta and 0.11.1 with M-Flasher 0.10.0: to and from M-VAVE's stock FM-1 016, the official V15 and Coral Flower 0.7.1, saving checked at every step; 0.12-beta (`FM-1_53001200`), 2026-11-22: V15 to it and back twice, then to Coral Flower, all clean |
| The form: name, your name, an address (never shown), one line, how it was tested | **Alex** |

**Submitted 2026-11-22** with v0.12-beta. The hub's check passed everything (source, licence, `fmhub.json`, sounds,
the release and its `.fwsc`, identity `FM-1_53001200`): "waiting for Sven", who reviews every firmware himself.
VIBE FM is not a gate: the SDK's issue #3 asks for the converter's log.

## Next, sorted (2026-11-23)

Alex: low-hanging fruit first, then "basic Vane" (wavetables, attack transients), then waveguides and WindCore; and
Alex's own ideas already here, which the first sort left out: the demo MIDI preloaded, the ARP's next steps, bassline
generation, ProgGenie; then six more from Alex the same day (program changes, a random ARP by degree weights, a loose
chord quantize, lead sheets, the overdub's playflow, the plugin of its own). The assessment behind the Vane items:
[VANE.md](VANE.md). `app/synth.c` is shared by the FM-1 and the native port, so YARD's additions land in both.

**Alex's notes of 2026-11-25 (0.17)**: done: the overdub from the tap (the pass so far copied after the mark,
`rec_mark_splice`); the ARP's VELO (the contrast kept, at least 32) and FEEL (accents, a little randomness); a sound's
three layers, the same for every engine (EDIT the engine's own, ENV the envelope, FX the effects with SPACE and ROOM
together); favourites (SAVE held; FAVOURITES first on PRESETS); the same effects on many sounds (the page,
`yard.py sounds --fx`, `ysb.py fx`); a "Download all" ZIP loaded back on the page. Next from them:
- **YARD's effects**: route YARD through Clipper's buses (chorus, delay, reverb, SPACE) so FX means the same on every
  sound; its CPU on the FM-1 to measure first (YARD's point is being light).
- **House effects**: one set of sends and SPACE over every sound as it loads, Clipper's 337 too ("across the board"
  without making each one yours: the user's bank holds 32); a new settings record (the last byte of room went to the
  ARP's VELO and FEEL).
- **Headroom**: with FX up, sounds get louder without distortion (Alex, on headphones): measure the effects' peaks
  against the limiter (`make loud` with the sends up) before raising anything; a LOUD step of +9 DB if it holds.
- **Native**: VELO and FEEL as DAW parameters (the port plays the defaults, 40% and 30%); ENV and FX pages there too.

**Small, a session or less each**
1. **Native: MIDI in on a free channel while a recording plays** (the FM-1's 0.13.2, for the Diosynth's overdubs).
2. **Native: SAVE's copy spread over blocks** (now one block, up to 17% of it in Node).
3. **The loop's length to its last note's end**, not to where REC was tapped (both).
4. **YARD's ONSET. Done (0.15.N):** Vane's ten synthesised transients (TONGUE CLICK CHIFF KNOCK PICK BUZZ PING SQUEAK
   SCRATCH TICK, `gen_inharmonic.py`'s recipes) as a burst per note over its envelope, ONSET and ONSET LVL on YARD's
   third EDIT page (CC 27, 28), kept with the sound; no samples, both places (`app/tests/onset.txt`, `make onset-cal`;
   `dist/onset-demo.wav` to hear). Next, by ear: its tone (the band) and length as values, pitched ones.
4b. **Loudness matched. Clipper's done (0.15.N):** Alex, by ear: "patches vary widely in overall loudness, some
   almost inaudible, with the same MIDI". Measured (`make loud`: the same phrase, its loudest 400 ms K-weighted):
   Clipper's sounds spread from -40.6 to -11.8 dB, plucks and keys quietest, because each trim only kept the
   hardest playing (velocity 127, full pressure, chords of six) under the limiter. `make clp-trim` now trims by
   loudness: ordinary playing (velocity 100, pressure 64: Alex's recordings, median velocity 87, pressure 31, at most
   112) to -26 dB, the hardest playing (pressure 100) limited 6 dB at most; now -32.3 to -23.6 dB, ordinary playing
   limited under 0.4 dB for nine sounds in ten. The loudest monos (808s, ACID) come down about 14 dB.
   `dist/loudness-demo.wav`: eight sounds, before then after. **YARD's, done (0.16):** its factory sounds spread 16 dB
   (CLAV -35.6, BASS -32.3, SAW -19.9; a mono voice about 6 dB under a chord); now a LEVEL per sound (the ONSET page's
   third value, CC 29, -12 to +12 dB, kept with the sound; natively a DAW parameter), the factory values measured to
   -26 dB, all 13 within 0.2 dB of it.
   **Then the level itself (0.16, Alex: "sustained volume remains an issue ... unusable" in a quiet room; Chords EZ
   and YARD's quieter than before):** every sound at -26 dB was the matching's doing, under an output whose ceiling
   was about -11 dBFS (Clipper's limiter, then its scaling: the headroom from the analog-distortion work). Both
   engines now 8 dB up after their limiters (x 2.5: the ceiling about -3 dBFS), so the matching holds and the
   limiter works no more than before (ordinary playing: none for half the sounds, under 0.4 dB for nine in ten): the
   sounds at -18 dB, Afro Keys with Alex's FX -16.8. PHONES MAX is there if the headphones distort again.
   **Then headphones (0.16):** USB and analog recorded together (Alex, 2026-11-24): the analog output is flat
   within 0.3 dB from 31 Hz to 8 kHz, its polarity inverted, and drives a line input to full scale; headphones on the
   same jack are what is weak. **LOUD** (GLO on SOUND, K3; `app/loud.h`, both places): a compressor on the output,
   +3 DB (2:1) or +6 DB (4:1), a held chord about 3 and 5 dB up, peaks under full scale (`make loud-test`,
   `app/tests/loud_out.txt`).
5. **Program changes followed** (Alex): a program change on MIDI in picks the sound (YARD's factory and user sounds,
   then Clipper's, in the PRESETS order; bank select for the rest), and the recorder keeps it, so a recording replays
   its changes of sound; sending one when the sound changes on the device, as an option. Today none are read.
6. **The overdub's playflow made clear** (Alex): what REC does in each state (playing, looping, overdubbing, undo),
   shown on the screen as it happens (LOOP, +DUB, the layer count), the MIDI-in overdub's channel shown, the manual's
   page as a small diagram; then whatever playing shows to change (a count-in, punch in and out, a layer muted).
6b. **The panel's rough edges, from the first user test. Done (0.16), but two left as they are by design: SELECT
   steps what the layer lists (on EDIT, the chord type), and HOME flips GLO and EDIT to their other half.** (`tests/usertest/REPORT-2026-11-23.md`): labels that collide
   or are cut (PHONES MAX and SPREAD, "ON/OF", "SAVE: KEEP THE SOUND"); the chord line saying "C MINOR M7" over a
   Cm7 (QUANT FULL: the quality as it sounds); USB AUDIO not wrapping where PLAY does; SELECT on EDIT changing the
   chord type when you only look; a saved YARD sound named after its source (CLAV 1, not USER 1); the slot grid
   showing which slots hold a recording; the slot warning's wording (the history still has it); labels left lit
   after a layer closes. Then the test again (`tests/usertest/README.md`), with the new manual.
7. **The demo MIDI preloaded** (Alex): a demo recording in the firmware's slots on a first start (MPE chords, glide,
   combined qualities, a lead, a wind take; 1980s-demo cheesy is fine), as a `.mvt` in flash, on both.
8. **The FM-1's history at 64 KB as an option** (16,384 events): 64 KB of pool for what follows.

**A few sessions each**
9. **The ARP, tweaked** (Alex): its degrees from the expression (pressure or slide: how far up the run, which
   degree), rhythms that are not even (lengths, rests, accents; Serpe-like notation for a pattern as text), steps of
   two or three notes and the full chord on chosen steps (comping), steps through the scale's degrees above the held
   notes; and **a RANDOM mode in the current scale** (SCALE, done in 0.16, unweighted), each degree weighted (the root most, then the fifth, the third,
   and so on: a histogram of degree frequency, a default from the corpus, editable on the page), a run never leaving
   the scale and the key it follows.
10. **Quantize to the current chord, loosely** (Alex): a QUANT setting between FULL (the scale) and CHORD (the held
    chord's notes only): notes on the beat or held long pulled to the chord's tones, quick ones between left as
    passing tones in the scale; the looseness on a value (how long a note may stay off the chord), for MIDI in as
    for the keys.
10b. **CCs for the knobs and buttons, assigned so they make sense** (Alex, 2026-11-23): one map, documented on the
    panel map and the page: each knob on each page and layer sends (and answers) a CC that follows MIDI's habits
    where there is one (74 brightness, 71 resonance, 73 and 72 attack and release, 5 glide, 64 HOLD as sustain, 1
    and 11 for macros), the rest in a block of their own (today's 20 to 29 for YARD's values, 16 to 19 Clipper's
    macros, 12 and 91 to 94 the sends); the buttons that are states (ARP, LOOP, HOLD, the play mode) as CCs too, so
    a DAW can record and automate them; the same numbers natively as the DAW's parameters; and a choice of channel
    (MPE's master, or one of its own) with a way to turn sending off. Today the play pages' knobs, the sound's values and
    the sends are CCs, recorded and sent; the settings (GLO, the CHORD layer, the ARP, SEQ) and the buttons are not.
11. **Bassline generation** (Alex): the ARP's machinery on the bass alone, from the chord and the scale (roots,
    fifths, passing tones, approach notes into the next chord), on its own channel; with multitimbral, a bass sound
    of its own.
12. **Wavetables, natively**: Vane's AKWF tables (CC0) as a YARD oscillator, the morph on a knob and on pressure or
    slide.
13. **A simple waveguide**: plucked string and blown pipe (Karplus-Strong, a one-delay flute), polyphonic, both places:
    the FM-1's budget for delay lines measured before WindCore's.
14. **Wavetables on the FM-1**, compact (256-sample frames, 16-bit, three or four levels, built for the table in use).
15. **The recorded transients** (VSCO-2-CE, VCSL: CC0): natively; on the FM-1 if Alex wants them there (about 100 KB
    of flash, and the Hub's sounds line becomes a CC0 statement).

**Bigger**
16. **ProgGenie on the device** (Alex; M3 below): its progressions in C (`core/proggen`: the corpus's transition tables,
    compact, in flash), suggesting the next chord on the black keys or a pad, the voice leading as now.
17. **Lead sheet input** (Alex): a progression from ProgGenie or a lead sheet (chord symbols and bars, as text: a
    ChordPro-like or iReal-like line, through the web page or a file) loaded as a chart; the device then steps through
    it (on the beat, on a pad, or on each new key pressed), the keys playing its chords and scales, the ARP and the
    bass following it; and recordings keeping the chart's place.
18. **WindCore in C** (from `Vane/WindCore/WindCore.h`): native first, compared with Vane's render; then on the FM-1
    as a mono WIND engine, single precision, judged by ear at one times the rate, probably on two cores.
19. **mvane yard as an indie plugin of its own** (Alex; 3 below), beyond the VIBE FM card: the same C, with room the
    FM-1's screen lacks: explanatory text for each control and mode, a separate chord display (a piano roll of the
    voices: what moved, what held, the chart's next chord), other inputs (Serpe-like UPI patterns typed in, the
    computer keyboard, a lead sheet), the same sounds and recordings as the FM-1.
20. Then what was already listed: ENV and LFO as drawn qurves (13 below), editable chord qualities (14), multitimbral
    (2).

## After the submission (proposed, 2026-11-22)

In order, each to judge by ear on the FM-1:
1. **What the hub's review and the first players say**: fixes first.
2. **Bass, chords, lead (multitimbral)**: a sound and a MIDI channel each; generative basslines, comping, the lead
   played; the EZ sounds and the second core make room for three.
3. **mvane yard as its own plugin, app and AUv3** (`core/` is already portable; YARD is plain C), the same sounds and
   takes as the FM-1, without VIBE FM.
4. **The demo take** in the firmware: MPE chords and a wind-controller part, 1980s-demo cheesy; and takes that show
   off MPE pitch bend (each note bent its own way), possibly as simplified MIDI files (Alex, 2026-11-22).
   **MPE glide. Done in 0.11-dev:** each moving voice's own glide, through its note's pitch bend (SPREAD: the
   highest voice up to four times the lowest's GLIDE); next, by ear: the order (low to high, or the widest leap
   longest), the curve.
5. **Progressions on the device** (M3, ProgGenie) and music-suite's voice leading in C.
6. **Chord memory and pads** (M2b), **parameter locks and gestures** (M4), **a compact take** (thinned controllers).
7. **The sound list curated**: engines dropped or folded (Alex: variety over count), which frees flash and RAM.
8. **CPU**: a per-bus effect gate (about 15% of a light sound back while playing), the hot engine code in RAM if the
   xruns at a change of sound come back.
9. **VIBE FM**: the converter runs again (2026-11-22), and its platform now has every call mvane yard makes: the second core's, the USB host's, the microsecond ticks. Our weak stand-ins for them were second definitions to it (it builds the app in one unit): those for the second core and the USB host's state went in 0.12.7, the last two (plat_ticks24, plat_usb_host) in 0.13.3. Next: a release with it, the hub's conversion, its result in the plugin.
10. **Try it in the browser, from the landing page** (Alex, 2026-11-22): fm1-base's WebAssembly emulator (the host
    build, as the VIBE route needs anyway) on the page, the panel and keys on screen, takes and sounds from the
    computer. Not installing from the browser: with the bricked-module stories around, the page points to
    **M-Flasher** (as M-VAVE did on its forum) with its backup first, and keeps the `.fwsc` a download.
11. **Takes that loop and overdub** (Alex, 2026-11-22). **LOOP done in 0.12.2:** SEQ's K2, the take again on time
    at its end; and a take follows the KEY and SCALE while it plays (each note to the same degree). Next:
    - **Loop length**: to the beat, not to where REC was tapped (trim the tail to the last note's end, or snap it to
      the bar the first notes suggest; a tap tempo).
    - **Overdub from a wind controller** (Alex, 2026-11-22, the Diosynth in POLY+BR: "a channel conflict"). 0.13.N:
      breath per channel in YARD and Clipper (a voice follows its channel's, else the latest); MIDI in moved to a
      free member channel while a recording plays, a new one each overdub (all fifteen taken: the first layer's);
      Clipper's POLY+BR no longer forced to one legato voice (its EWI rule, kept for MONO+BR).
    - **Overdub. Done in 0.12.3:** REC during a loop; from the top of the next pass the take's own playing goes into
      the history with what is played over it, and each pass becomes the take (`rec_mark`, `rec_capture_from`), one
      undo (REC held). Next, by ear: starting at once (the pass under way, its first part rebuilt from the take);
      more than one undo; layers each on their own channel set for MIDI out; a layer's sound of its own
      (multitimbral).
    - **The take's own scale** stored with it (the take file's header), so a take saved in C major plays in C minor
      from its first note when the scale has changed since.
12. **The ARP** (Alex, 2026-11-22). **A basic one done in 0.12.5**: ARP tapped OFF / ON / LATCH (not a key), held
    its layer; UP, DOWN, UP-DOWN, DOWN-UP, UP+DOWN, RANDOM, AS PLAYED, CHORD; 1/4 to 1/32 with triplets; GATE;
    OCTAVES; a tempo of its own; each note on its source's MPE channel (its pressure and bend follow). Next:
    - **Its degrees from the expression**: pressure or slide choosing how far up the run goes, or which degree.
    - **Rhythms that are not isorhythmic**: notes of different lengths, rests, accents; Serpe-like rhythm notation
      (a pattern as a short text) for its patterns.
    - **Simultaneous notes**: steps of two or three voices, the full chord on chosen steps: a comping engine; and the
      same with the bass alone: a bassline engine (with multitimbral, item 2).
    - **In the scale**: steps through the scale's degrees above the held notes (not only the notes themselves).
    - **Clock**: MIDI clock in and out, tap tempo, the loop's length as the bar (item 11).
13. **ENV and LFO, the two buttons left** (Alex, 2026-11-22: not as an envelope and an LFO, "Not in my house"):
    drawn **qurves** (as in DrawnQurve) shaping a parameter over a note or a bar; waveshapes; later wavetables for YARD.
14. **EDIT's chord qualities editable** (Alex, 2026-11-22: EDIT on PLAY shows the black keys' qualities and SELECT
    steps through the type, but the qualities themselves are fixed): each black key's intervals set on the panel and
    on the page, kept as a quality set with the bank, as the scales are.
15. **Discoverability** (Alex, 2026-11-22): done in 0.12.4, USB HOST in the title while hosting (Alex could not
    reach the FM-1 from the computer in host mode) and GLO's screen pointing to its other half (PHONES MAX); next, a
    pass over every layer's screen for what is hidden.

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
Found by Alex: it was HOLD. A chord kept by HOLD across a change of sound (worst from a poly sound to a mono one)
was faded out by Clipper while mvane yard went on thinking it sounded, so the next chord only sent the notes that
differed. **0.9:** a change of sound releases what sounds first and plays a held chord again on the new sound; mono
sounds are flagged; each sound's cost is measured and tagged; Clipper's effect buses skip once long silent.
Earlier note: the sounds that wait for a top-row key do not do it in the simulator (every sound plays from a bottom-row key
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
- **Chord qualities combined** and **single notes** (Alex, 2026-11-22). Done in 0.10-dev, as Coral Flower's Lock:
  keys pressed together latch together, merged by its rule (`chord_build_qualities`, tested in `core/chords_test.c`
  and `app/tests/combos.txt`); the only latched key tapped again unlatches, and the white keys play single notes.
  Not yet: the combination kept in the settings (a new settings record), and Coral Flower's held (non-Lock) mode.

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

**Families** (0.10-dev, Alex 2026-11-22): PRESETS runs through CHORDS LIGHT, CHORDS, HITS LIGHT, HITS, LEADS,
OTHER, by how each sound behaves (`make clp-bank`: held 2.5 s, sustained or not; poly or mono; Clipper's own kind
where it is clear) and its cost; [SOUNDS.md](SOUNDS.md) lists them. Next: the curated selection within them
(dropping engines and sounds), arpeggios for HITS.

Alex, after 0.9 on the device (2026-11-22): the most interesting **light polyphonic** sounds are in WHEEL, PHASE and
SAMPLE; the other light ones are mono or semi-percussive. MID covers a wide variety: favour **variety of sound** over
the number of sounds when choosing what stays. Removing engines makes room on the device (flash, RAM).

Each sound's cost on screen (its CPU, measured, as a bar or a word) would make performance clear. A curated list of sounds by kind (pads, keys, strings, bells, leads, organs, choirs, basses) on PRESETS, the rest in
an "ALL" list, is the likely shape. Dropping engines also frees flash and RAM (WAVE could come back).

### M2e: expression on every sound (Alex, 2026-11-22)

The FM-1's keys send pressure, and most sounds did not answer it. `make clp-expr` (tools/clp_expr.c) measures each
of Clipper's sounds: a chord held with no pressure, full pressure, and slide at the top, compared by level and
brightness. As the factory made them, **155 of 337 sounds ignore full pressure** and 145 ignore slide, mostly because
Clipper sends both to the cutoff and those engines read none. In 0.10-dev, the sounds measured deaf get matrix slots
when they load (`clp.c clp_expr_fill`, `gen/clp_expr.h`): pressure to the engine's tone (CUT, DCW, TIMB, MOD, MLVL,
BRIT, BUZZ ...) and to the level (up to +3.5 dB), slide to its tone (CUT, VOWL, TOP, FB, X, D ...), each half its
range, all at rest at 0. **Now 93 ignore pressure.** What is left needs each engine's own handling:

- WHEEL (11 of 14): its drawbars and drive are set once a block for the whole organ, so no voice's matrix reaches
  them. Pressure could switch the rotor to fast (the organist's habit), or the part's maximum pressure drive the
  drawbars.
- PHASE (10 of 16): the DCW is already at its top in most sounds (the cutoff adds to it); pressure could open it from
  lower, or move WAVE2.
- WSEQ (11 of 16), TERRAIN (8 of 10), GRAIN (10 of 10), SAMPLE (3 of 4), FMX's slide (16 of 16): to read engine by
  engine.
- The level slot is often flattened by the master chain (compressor, limiter): it works best with a timbre change.

An engine whose voices split between the two cores takes the matrix's CUT or SHAPE, never an EDIT value: a slot on an
EDIT value keeps every voice on one core (found on the device, 2026-11-22; `make clp-split` guards it). With that,
96 sounds ignore pressure (MACRO answers less, FMX much more).

The amounts (half the range) are a first guess, to judge by ear; a batch edit in the web editor (M6) could set
them per sound.

### M2f: YARD, mvane yard's own synth, elaborated (Alex, 2026-11-22)

The saw (`app/synth.c`: a band-limited saw, a key-tracked 18 dB lowpass at control rate, 12 voices, MPE, breath) plays
a chord at about 12% of the FM-1, a third to a sixth of Clipper's sounds, and Alex finds it satisfying in context,
if VA-plain on its own. Grow it while keeping it light: a second oscillator or a sub (pulse, PWM), detune, a noise
for breath, the filter's resonance and modes, an envelope to the cutoff, expression mapped deliberately (pressure to
level and brightness, slide to timbre), its voices on the second core (half each: about 6%), a few factory sounds
(pads, keys, leads), and, later, the WindCore voice beside it (M8). Each addition measured (`make synth-cmp`'s
renders, the FM-1's CPU) and heard.

**Done in 0.10-dev:** DETUNE (a second saw, up to 30 cents), SUB (a square an octave down), RES, PLUCK (one macro:
the sustain lower, a filter contour from five octaves up) and DECAY; the voices' upper half on the second core (a
chord spreads over both halves; `make yard-race`: ThreadSanitizer, within one step of 24 bits); nine sounds (SAW, PAD,
ENSEMBLE, KEYS, PLUCK, BRASS; LEAD, BASS, WIND mono), first on PRESETS; the SOUND page TONE ATTACK DECAY RELEASE,
the FX layer DETUNE SUB RES PLUCK. `make yard-render` writes them to listen to: on a Mac, DETUNE costs about a third
more than SAW, BRASS half; mono sounds a third. SAW sounds as before (133 dB under). Next, by ear: the sounds' voicing
and levels, a pulse or PWM, noise for breath, expression mapped deliberately.

Alex on YARD's sounds (2026-11-22, cda2ed7): all light, fairly generic, doing the job; they could replace some of
Clipper's.

### M2g: levels and the master limiter (Alex, 2026-11-22)

A JAZZ ORGAN recording (e2f8b22, USB AUDIO FULL, effects off) had "noise on the line": its peaks sat at exactly
-11.2 dBFS, Clipper's master limiter's threshold (fx.c LIM_T, 18000 in Q15), which worked all the time. Clipper's
levels suit a few notes; mvane yard's chords are six: with a chord of six at full level, the median polyphonic sound
was limited 11.5 dB (a quarter over 17 dB; mono sounds, a note, about 1 dB). **Done:** each sound's trim before the
master and the limiters (`make clp-trim`, gen/clp_trim.h, fx.c CLP_TRIM): 295 sounds trimmed, none limited over
1.5 dB now. Chord sounds are 3 to 8 dB quieter on average; the ceiling is the same.

Also from Alex: Clipper's default reverb sounds too metallic (FX held, SELECT changes the model); choose a warmer
default. YARD's sounds have no effects of their own: their character is different, which is interesting.

### Toward the Firmware Hub (Alex, 2026-11-22)

"Getting close to submitting." Before that:
- **VIBE FM converts the newest release.** The hub says 0.9-beta does not ("you get 0.4"). Since 0.4 mvane yard
  took up the second core, the USB host, a lost USB and the microsecond ticks; the converter's platform may lack
  them. 0.10-dev gives weak stand-ins in the WebAssembly build (app.c, VIBE FM): one core and no USB host where the
  platform has none. To check with a release the hub can try.
- **Clipping, deeper**: GOSPEL still clipped. Trims measured played hard (velocity 127, full pressure: the expression
  slots' level swell; chords in quick succession): none over about 2 dB.
- **Xruns at a change of sound**: PK, SW and n AT SW on the bottom line, to read on the FM-1 (OPTIMIZATION.md).
- **The costs again**: EZ and HRD, measured on two cores; the families CHORDS EZ, LEADS EZ, HITS EZ, CHORDS HRD,
  LEADS HRD, HITS HRD, OTHER, YARD's sounds among them.
- **The page**: every action says what it did, in view wherever the page is scrolled.

Alex on 0.10-beta (2026-11-22):
- **VIBE FM still gets 0.4.** The converter is not in the public SDK (github.com/Vibe-FM-SDK/vibe-fm-sdk: its
  `vibe-check` rejects forbidden imports, blocks over budget while loading, crashes under random events). Ask the hub
  (zven@thefirmwarehub.com, or an issue on the SDK) for the converter's log of 0.10-beta.
- **Patch files only for YARD's sounds**: Clipper's came in 0.11-dev (below).
- **Clipping, even on SAW.** YARD's output peaks at -10 dBFS played hard (velocity 127, full pressure, six voices):
  nothing reaches its soft clipper (from -6 dBFS). Alex hears the same on other firmwares: the FM-1's analog stage,
  or something intermittent. Next: a USB recording of it (the digital signal: clean there means after the DAC).
- **SUB** two octaves down (0.11).
- **Xruns**: not at a change of sound (PK 0, SW 10); one while a file came from the computer (flash writes stop the
  audio for each sector erased). The bottom line now tells them apart (n SW, n FL).
- **Versions**: releases 0.11, 0.12 ... 0.99 (fm1-base's identity takes two digits a part: no 0.100; then 1.0, or
  0.99.1). CI's builds: 0.11.N+COMMIT, N the commits since the last release (Alex: so they count up).
- **SAW's glitches are after the DAC**: Alex's USB recording is clean (peaks -10.4 dBFS, nothing flattened). The
  firmware's lever is the analog level (app.h APP_GAIN: MASTER's curve, the speaker's only with USB AUDIO FULL): a
  setting to cap it, if the glitches come with level.

Then, from Alex's list:
- **YARD's sound itself editable. Done in 0.11-dev:** WAVE (SAW, PULSE, TRIANGLE: the pulse two band-limited saws a
  width apart), PW (pressure narrows it: PWM under the fingers), NOISE (with breath, the breath's), FILTER (LOW,
  BAND, HIGH from the same SVF); a second FX page (SELECT); HOLLOW, CLAV, FLUTE, REED, and WIND now a triangle with
  breath noise. Each at 0 leaves a sound as before (SAW bit for bit). Settings MVA8 (0.10's MVA7 read); the bank's
  YARD entries have room (0.10's read with the four at 0). No LFO (Alex: "not in my house"): movement comes from
  playing (pressure narrows the pulse). **Sound editing on EDIT** (Alex): EDIT follows the knobs' page, the chords'
  layer on PLAY, the sound's editing on SOUND (YARD's two pages now). Next: the same for Clipper's sounds (its
  engine's eight values, its envelope), once YARD's is tried on the FM-1.
- **Clipper's sounds as patch files. Done in 0.11-dev:** a factory sound and up to 14 parameters changed over it
  (any of the 126, by id), its reverb's model; FX and SAVE on the panel keeps the macros, sends and reverb; in the
  same bank as YARD's (version 2, 3088 bytes; 0.10's still reads), the page's card, `yard.py sounds`, JSON with names
  (`make clp-names`: web/clp_names.json). Next: more of a Clipper sound editable on the panel, more than 14 changes
  if a sound needs it, YARD's oscillators (below).
- **A demo take in the firmware**: a short MIDI phrase showing it off (MPE chords, glide, combined qualities, a lead),
  1980s-demo cheesy is fine, and a short wind-controller take (breath, a mono lead), maybe in the same file; in a
  slot at the first start.
- **Multitimbral: bass, chords, lead** (Alex: not a groovebox). Coral Flower had its own sound for the bass; a third
  for a lead. The bass could follow generative basslines, the chords get comping, the lead come from playing. A
  sound per part, a MIDI channel per part; the second core and the light (EZ) sounds make room for it.
- **mvane yard as its own plugin and app**: a plugin (VST3, AU, CLAP), a standalone app, and an AUv3 on iPad, with
  the same sounds, takes and files as the FM-1, without VIBE FM (beside M7).
- **Review the roadmap**, then submit to the hub.

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
- **Patch files** (Alex, 2026-11-22: "now that we have a bunch of patches"). **YARD's, done in 0.10-dev:** a bank of
  32 user sounds in the store (OBJ_YARD, `yard.ysb`), FX and SAVE on the panel, the page's card, `yard.py sounds`,
  JSON to share (`tools/ysb.py`, `web/ysb.js`, tested against each other). Next: the plugin's side, then a Clipper
  sound as edited (its engine and preset, the parameters changed from it, the matrix, the
  macros and sends).
- **A compact take ("efficiency mode")** (Alex, 2026-11-22): the baseline phrase is 24,164 events for 123 notes.
  Dense controller streams (bend, pressure, slide every few ms) could be thinned where a straight line between kept
  points stays within a tolerance (as a line simplification), on capture or on export, keeping the gesture while
  saving most events; offered as a mode beside the exact history.
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

## Optimization

A source-level audit of 0.9-beta, its candidates ranked and checked: [OPTIMIZATION.md](OPTIMIZATION.md). Done
in 0.10-dev: the event queue (lost events counted and repaired), the saw's coefficient tables. Next: the effect
buffers' memory map. Alex's first readings (2026-11-22, the baseline phrase from a slot): about 9% at rest, a light
poly sound (SAMPLE SAW ENSEMBLE) 37-38% while it plays and 12% for a few seconds after, a heavy one (HYBRID GLASS
PAD) 48-54%. In line with 0.9; no gain yet where Clipper plays (the saw's tables only help the saw).

## Panel conventions

The 0.x panel is improvised (FX for voicing, LFO for voice count, EDIT for bass, GLO for USB level, SELECT for the
scale). Before 1.0, map controls the way FM-1 players expect from M-VAVE's firmware and the popular custom ones
(Clipper, Coral Flower): the printed labels, shift layers and pages, so that nothing surprises.

## Name

**mvane yard** (Alex, 2026-11-21): "mvane" alone read like a typo for M-VAVE. Package identity unchanged then (`FM-1_17...`); `FM-1_53` from 0.12 (the hub's check: 17 is inside LOOM-8's range).

Where it comes from (Alex, 2026-11-22): first hills, terrain, textures, a landscape with a weathervane; then a
playground, a schoolyard. This synth is meant for playful exploration, and *yard* says that. The *vane* is less tied
to the image, yet does not disturb it.

## Open questions for Alex
- Where to find VIBE FM's SDK or converter (TheFirmwareHub's documentation, or its author).
- "Full" quantization in Coral Flower: to the bar, the beat, or the next chord's slot?
- Which voicing shapes you reach for on the Exquis, to add as styles.
