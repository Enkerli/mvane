# mvane yard manual (0.6)

mvane yard is a performance firmware for the M-VAVE FM-1: play notes or chords, through mvane yard's own synth or
Clipper's 379 factory sounds, with MPE and breath, while a recorder keeps everything you play. Keep what you liked
after the fact, replay it, save it in one of eight slots, and take it to your computer as a MIDI file.

The panel layout is a first draft: it will change (see [ROADMAP.md](ROADMAP.md), "Panel conventions").

## Install

Install `mvane-yard-X.Y.fwsc` with M-Patcher (or fm1-base's installer). Back up your current firmware first. Settings
start from their defaults after an update that changes their layout (0.6 does).

## The screen

| Where | What |
| --- | --- |
| Top left | NOTES or CHORDS; a message for a moment after you change something |
| Top right | The sound |
| Middle (CHORDS) | The chord sounding (or the last one, dimmed); below: key, scale, chord type, voicing, voices + BASS, the pitch-class set's number, HOLD |
| Middle (NOTES) | The notes held |
| Knob row | K1 to K4: tone, release, glide, attack |
| HISTORY | How much playing time the history holds; % FULL; ASLEEP when you have stopped playing; FULL: SAVE when it has reached a take not yet saved |
| TAKE n/8 | The take in the current slot, its length, PLAYING or NOT SAVED |
| Bottom | CPU load and XRUNS (missed audio deadlines: should stay 0), the play mode, the USB audio level |

## Controls

| Control | Does |
| --- | --- |
| **ALGORITHM** | NOTES (turn left) / CHORDS (turn right) |
| **PRESETS** | The sound: mvane yard's saw first, then Clipper's sounds (a fast turn moves eight at a time) |
| **SELECT** | The key, C to B |
| **SEL held + SELECT** | The scale (pitch-class set) |
| **SEL tapped** | POLY, MONO (legato), POLY+BREATH, MONO+BREATH |
| **K1 K2 K3 K4** | Tone, release, glide, attack. They are sent and recorded as CC74, CC72, CC5, CC73, so a take replays your knob moves. On Clipper's sounds they act through its standard CC map (74: the engine's cutoff or brightness) |
| **FX** | Voicing: CLOSE, OPEN, WIDE |
| **LFO** | Chord size: 3 to 6 upper voices (plus the bass) |
| **EDIT** | Bass on / off |
| **ENV** | The black keys' chord bank: fixed qualities (Coral Flower's) or types from the scale |
| **GLO** | USB audio level: FULL (MASTER for the speaker and headphones only) or MASTER |
| **OCT- / OCT+** | Octave |
| **REC** | Tap: keep the last phrase (everything since the last 3 s of silence). Hold: keep the whole history |
| **PLAY** | Play / stop the take |
| **SAVE** | Save the settings, and the take into its slot |
| **SEQ / HOME** | Next / previous take slot (1 to 8). With a take not saved, the first press warns; a second press within 3 s moves on and drops it |

## Playing

### NOTES

Keys play notes from F3, each on its own MPE channel with its pressure (so a DAW or an MPE synth hears each note's
pressure apart).

### CHORDS

- **White keys** are the scale's degrees, climbing the keyboard; higher keys are voiced higher.
- **Black keys** pick the chord type and stay latched. A chord that is sounding takes the new type at once.
  - Qualities bank (the default, as Coral Flower's top row): **M6 M7 M9 S2 S4 m6 m7 m9 M m**, on the white key's root
    whatever the scale says.
  - Scale bank (ENV): **STACK** (thirds of the scale: 7th, 9th, then 13th and 11th), **7**, **TRIAD**, **ADD9**,
    **SUS**, **6/9**, **QUARTAL**, **11** (no third), **FIFTHS**, **V/** (the dominant of the degree).
- **F#5**, the last black key, is **HOLD**: a chord keeps sounding after you let its key go; the next white key changes
  it; HOLD again stops it.
- Every chord has a separate bass (EDIT turns it off) and 3 to 6 upper voices (LFO).
- Voice leading: each chord is voiced from the one before, so the voices move as little as they can; notes the two
  chords share are held, and the moving voices glide (K3 sets how long).
- Voicings: CLOSE (within about an octave), OPEN (drop-2-like), WIDE (broad, every gap a fourth or more).

### Scales (pitch-class sets)

MAJOR (2741), MINOR (1453), DORIAN (1709), HARM MIN (2477), PENTA (661), PCO (1723), NOODLING (1267), OCTATONIC
(2925). The number is music-suite's: bit 0 is the root, so C major is `101011010101` = 2741.

### MPE, wind controllers

- MIDI in (USB or the TRS jack) plays the sound with MPE: per-note pitch bend (the bend range from RPN 0, 48 by
  default on member channels), slide (CC74 on a member channel, centred on 64), channel and poly pressure. mvane yard
  sends an MPE configuration message when a computer connects.
- **Breath** (SEL: POLY+BREATH or MONO+BREATH): CC2 or CC11 from a wind controller sets the level and the brightness;
  notes are silent until you blow. Tried with a Diosynth.
- **MONO** is legato: a note played over a held one slides to it without restarting the envelope (the glide time, or
  20 ms at GLIDE 0); letting it go returns to the note still held.

### Sounds

PRESETS steps through mvane yard's saw (a band-limited saw through a key-tracked lowpass, dark to bright), then
Clipper's factory sounds, engines in an order for chords: HYBRID, JUNO, STRINGS, ANALOG, FM6, FMX, DIGITAL, WHEEL,
PHASE, VOICE, MACRO, PHYS, TRIO, VPS, TERRAIN, WSEQ, VECTOR, GRAIN, SAMPLE, LOFI, ACID, NOISE. WAVE is left out for
now (RAM). Clipper's sounds are dry, as Clipper ships them: no delay, no reverb. They respond to MPE, pressure (as
aftertouch), the mod wheel, expression, sustain, breath, and the standard CCs (71 resonance, 72 release, 73 attack,
74 cutoff, 75 decay, 5 glide, 16 to 19 the macros).

## The recorder

- It is always on, from power-on: everything you play goes into the history (notes, pressure, knob moves, MIDI in),
  as events, so its length is counted in events (32,768), not in time.
- It sleeps: a pause longer than 4 s is kept as 4 s.
- **REC** keeps a take from the history: a tap, the last phrase; held, everything. Nothing is copied: the take stays
  where it is in the history, protected until you save it (the history shows FULL: SAVE if you play on until it
  reaches it).
- **PLAY** replays the take through the current sound: try a take through different sounds.
- **SAVE** writes the take into the current slot (the sound stops for a moment while the flash is written). Saved takes
  play from the flash.
- **SEQ / HOME** pick one of eight slots.

## Your takes on the computer

- **The page**, <https://enkerli.github.io/mvane-yard/>, in Chrome or Edge with the FM-1 plugged in by a data cable
  (stop PLAY first): Connect; the eight slots; a piano roll of the selected take; **Download as MIDI** (one channel:
  one clip in a DAW, each note's pressure as poly aftertouch), **MIDI with MPE channels** (a channel a note, as
  played), **Download .mvt**; **Send a MIDI or .mvt file to this slot** (a MIDI file's notes, controllers, pressure
  and bend, all tracks merged, tempo honoured, 30,000 events at most); settings download and restore.
- **The command line**: `python3 tools/yard.py take --slot 3` (slot 3 as `take3.mid`), `yard.py takes` (every slot),
  `yard.py put song.mid --slot 5`, `yard.py settings`, `yard.py ls`. Needs `pip install mido python-rtmidi`.

## The plugin (VIBE FM VST)

TheFirmwareHub's converter turns mvane yard into a VIBE FM firmware; it plays as on the FM-1. In the plugin the take
stays in memory (its platform has no flash for files): capture and replay work, a take is gone when the session
closes. Whether settings persist with the DAW project depends on VIBE's store; see the roadmap.

## USB

The FM-1 is a USB audio interface (duplex: it records itself into the computer, and plays the computer's audio) and a
USB MIDI device. GLO sets the recording's level.

## Known limits (0.6)

- Clipper's code has 64-bit multiplies beside 16-bit loads, which on this CPU can occasionally flip a register bit
  and crash (felucca-bsp's docs/CPU.md). It is Clipper's code as Clipper ships it. If mvane yard crashes or glitches on
  Clipper's sounds, say so: the fix is known (those multiplies out of line).
- Headphones at full volume may distort a little (the FM-1's output stage).
- One processor core for the sound (Clipper uses two).
- No quantization, no arpeggiator, no sequencer yet.
