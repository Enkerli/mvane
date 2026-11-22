# mvane yard manual (0.9)

mvane yard is a performance firmware for the M-VAVE FM-1: play notes or chords, through mvane yard's own synth or
Clipper's 379 factory sounds, with MPE and breath, while a recorder keeps everything you play. Keep what you liked
after the fact, replay it, save it in one of eight slots, and take it to your computer as a MIDI file.

The panel follows the FM-1's habits (PRESETS the sound, SELECT what the context lists, a held button a layer); its
design is in [PANEL.md](PANEL.md).

## Install

Install `mvane-yard-X.Y.fwsc` with M-Patcher (or fm1-base's installer). Back up your current firmware first. Settings
start from their defaults after an update that changes their layout (0.6 and 0.7 do).

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
| Bottom | CPU load and XR (missed audio deadlines: should stay 0), then Q if the sound's event queue ever overflowed (larger, on the left; the play mode, the sound's cost and HOST on the right, each where it fits) (the count of lost events; each loss is repaired: the sound is silenced and what should sound plays again), the sound's cost (LIGHT, MID, HEAVY), HOST while the USB port hosts a device, the play mode (MONO SOUND: a mono sound under POLY) |

## Controls

The four knobs' names are on the screen above them, and the one you turn lights up. What they do depends on the
**page** (HOME) or the **layer** (a button held, or tapped to leave it open until the next tap).

### Knob pages (HOME switches)

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| **PLAY** (as Coral Flower's) | QUANT (off, basic, full) | KEY | SCALE | GLIDE |
| **SOUND**, Clipper's sounds | BRITE | TIMBR | MOTN | SPACE (Clipper's four macros) |
| **SOUND**, mvane yard's saw | TONE | ATTACK | RELEASE | GLIDE |

Every knob move on these pages is recorded with the take (as CC 5, 16 to 19, 72 to 74), so a take replays it.

### Layers: hold a button and turn a knob (or tap it, turn, tap again)

| Layer | K1 | K2 | K3 | K4 | SELECT |
| --- | --- | --- | --- | --- | --- |
| **EDIT**: CHORD | VOICING (close, open, wide, pairs) | VOICES (3 to 6) | BASS (on, off) | BANK (quality, scale) | the chord type; the screen lists the black keys' types |
| **FX** (Clipper's sounds) | DRIVE | CHORUS | DELAY | REVERB (sends; recorded as CC 12, 93, 94, 91) | the reverb's model |
| **GLO**: SETTINGS | PLAY (poly, mono, + breath) | USB AUDIO (full, master) | MPE OUT (on, off) | BEND (12, 24, 48, 96) | USB: right, host to a USB MIDI device (the Diosynth); left, back to the computer |
| **SEQ**: TAKES | SLOT | | | | the take slot (1 to 8); the screen shows the eight slots |

### Encoders and buttons

| Control | Does |
| --- | --- |
| **ALGORITHM** | NOTES (left) / CHORDS (right) |
| **PRESETS** | The sound: mvane yard's saw, then Clipper's sounds (a fast turn moves eight at a time) |
| **SELECT** | On the pages: the next or previous engine's sounds. In a layer: as above |
| **HOME** | PLAY page / SOUND page |
| **OCT- / OCT+** | Octave; both together: panic (everything off) |
| **REC** | Tap: keep the last phrase (everything since the last 3 s of silence). Hold: keep the whole history |
| **PLAY** | Play / stop the take |
| **SAVE** | Save the settings, and the take into its slot |

Moving to another slot with a take not saved: the first turn warns, a second within 3 s moves on and drops it.

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
  it; HOLD again stops it. A held chord follows a change of sound (it sounds again on the new one).
- Every chord has a separate bass (EDIT turns it off) and 3 to 6 upper voices (LFO).
- Voice leading: each chord is voiced from the one before, so the voices move as little as they can; notes the two
  chords share are held, and the moving voices glide (K3 sets how long).
- Voicings: CLOSE (within about an octave), OPEN (drop-2-like), WIDE (broad, every gap a fourth or more), PAIRS (the
  3rd and 7th in the left hand, the root and 5th above them in the right: two pairs of fifths moving around; more
  voices go on top; with three voices, OPEN).

### QUANT (as Coral Flower's: to the scale, not in time)

- **OFF**: CHORDS: the white keys are roots a semitone apart, the tonic on the fifth key. NOTES: as printed.
- **BASIC** (the default): CHORDS: the white keys are the scale's degrees. NOTES: each key snaps to the nearest tone
  of the scale (down on a tie: C# to C in C major).
- **FULL**: as BASIC, and each tone of a fixed quality (the qualities bank) snaps to the scale too: the chords become
  diatonic.

### Scales (pitch-class sets)

MAJOR (2741), MINOR (1453), DORIAN (1709), HARM MIN (2477), PENTA (661), PCO (1723), NOODLING (1267), OCTATONIC
(2925). The number is music-suite's: bit 0 is the root, so C major is `101011010101` = 2741.

### MPE, wind controllers

- MIDI in (USB or the TRS jack) plays the sound with MPE: per-note pitch bend (the bend range from RPN 0, 48 by
  default on member channels), slide (CC74 on a member channel, centred on 64), channel and poly pressure. mvane yard
  sends an MPE configuration message when a computer connects.
- **Breath** (GLO, K1: POLY+BREATH or MONO+BREATH): CC2 or CC11 from a wind controller sets the level and the brightness;
  notes are silent until you blow. Tried with a Diosynth.
- **MONO** is legato: a note played over a held one slides to it without restarting the envelope (the glide time, or
  20 ms at GLIDE 0); letting it go returns to the note still held.

### Sounds

PRESETS steps through mvane yard's saw (a band-limited saw through a key-tracked lowpass, dark to bright), then
Clipper's factory sounds (337), engines in an order for chords: HYBRID, JUNO, STRINGS, ANALOG, FM6, FMX, DIGITAL, WHEEL,
PHASE, VOICE, MACRO, PHYS, TRIO, VPS, TERRAIN, WSEQ, VECTOR, GRAIN, SAMPLE. WAVE is left out for now (RAM); ACID,
NOISE and LOFI are out of the list. Clipper's sounds are dry, as Clipper ships them: no delay, no reverb. They respond to MPE, pressure (as
aftertouch), the mod wheel, expression, sustain, breath, and the standard CCs (71 resonance, 72 release, 73 attack,
74 cutoff, 75 decay, 5 glide, 16 to 19 the macros). A sound that full pressure or slide left unchanged as Clipper
made it (measured, `make clp-expr`) gets them routed when it loads: pressure to its tone and a little to its level,
slide to its tone, nothing changed at rest; some organs, phase and wave-sequence sounds still answer little. The
PLAY page's GLIDE is yours, not the sound's: it applies to every sound as it loads.

**Mono sounds** (89 of them, as Clipper's presets say) show MONO when you pick them, and MONO SOUND on the bottom line
while POLY is set: they play one note of a chord.

**Cost.** Each sound is tagged LIGHT, MID or HEAVY (shown when you pick it, and on the bottom line), from its render
time measured on a computer (`make clp-cost`, `tools/clp_cost.c`); the FM-1 is much slower, the ranking holds.
Engines, lightest first, by their sounds' mean: SAMPLE, PHASE, DIGITAL, WHEEL, WSEQ, TRIO, ANALOG, VOICE, GRAIN, VPS,
TERRAIN, VECTOR, FM6, FMX, HYBRID, PHYS, JUNO, MACRO, STRINGS; the heaviest sounds are some HYBRID and FMX ones.
About half of a typical sound's cost is Clipper's chain with nothing playing (its chorus, delay and reverb buses,
master stage and limiter). Its effect buses stop once every send has been silent for 20 s: turning the CHORUS send
to 0 (FX, K2) on a sound that has one saves a good share of the CPU.

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
- **SEQ** (held or tapped) and SELECT pick one of eight slots.

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
USB MIDI device. GLO, K2 sets the recording's level.

**USB host**: GLO held, SELECT right: the FM-1 hosts a USB MIDI device plugged into its USB-C port, such as the
Diosynth. The FM-1 gives the port no power, and most devices, battery-powered ones too, wait for the host's 5 V
before they connect: put a hub with its own power supply between them (a single-chip one: four ports, typically), or
an OTG adapter that brings 5 V to the device. The screen names the device.
Meanwhile there is no computer: no USB audio, no files. GLO held, SELECT left: back to the computer.

## Known limits (0.7)

- Clipper's code has 64-bit multiplies beside 16-bit loads, which on this CPU can occasionally flip a register bit
  and crash (felucca-bsp's docs/CPU.md). It is Clipper's code as Clipper ships it. If mvane yard crashes or glitches on
  Clipper's sounds, say so: the fix is known (those multiplies out of line).
- Headphones at full volume may distort a little (the FM-1's output stage).
- Clipper's sounds use both processor cores (half the voices each, as in Clipper); mvane yard's saw one. Watch CPU on
  the screen: some sounds cost far more than others.
- No arpeggiator, no sequencer yet.
