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
| Top left | NOTES or CHORDS (USB HOST, lit, while the port hosts a device: no computer meanwhile); a message for a moment after you change something |
| Top right | The sound |
| Middle (CHORDS) | The chord sounding (or the last one, dimmed); below: key, scale, chord type, voicing, voices + BASS, the pitch-class set's number, HOLD |
| Middle (NOTES) | The notes held |
| Knob row | K1 to K4: tone, release, glide, attack |
| HISTORY | How much playing time the history holds; % FULL; ASLEEP when you have stopped playing; FULL: SAVE when it has reached a recording not yet saved |
| RECORDING n/8 | The recording in the current slot, its length, PLAYING or NOT SAVED |
| Bottom | CPU load (then x2 while the second core renders half the voices, x1 lit when it does not run) and XR (with how many came within a second of a change of sound, n SW, or of a flash write, n FL: SAVE, a file from the computer, where the FM-1's audio stops for each sector erased), PK and SW (the worst single block since the latest change of sound, and in the second after it, in % of a block's time: 100 misses it) (missed audio deadlines: should stay 0), then Q if the sound's event queue ever overflowed (larger, on the left; the play mode, the sound's cost and HOST on the right, each where it fits) (the count of lost events; each loss is repaired: the sound is silenced and what should sound plays again), the sound's cost (EZ, HRD), HOST while the USB port hosts a device, the play mode (MONO SOUND: a mono sound under POLY) |

## Controls

The four knobs' names are on the screen above them, and the one you turn lights up. What they do depends on the
**page** (HOME) or the **layer** (a button held, or tapped to leave it open until the next tap).

### Knob pages (HOME switches)

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| **PLAY** (as Coral Flower's) | QUANT (off, basic, full: MIDI in snapped to the scale too) | KEY | SCALE (the named scales; SEL: any) | GLIDE |
| **SOUND**, Clipper's sounds | BRITE | TIMBR | MOTN | SPACE (Clipper's four macros) |
| **SOUND**, YARD's sounds | TONE | ATTACK | DECAY (PLUCK's fall) | RELEASE |

Every knob move on these pages is kept in the recording (as CC 5, 16 to 19, 72 to 74), so a recording replays it.

### Layers: hold a button and turn a knob (or tap it, turn, tap again)

| Layer | K1 | K2 | K3 | K4 | SELECT |
| --- | --- | --- | --- | --- | --- |
| **EDIT** on the PLAY page: CHORD | VOICING (close, open, wide, pairs) | VOICES (3 to 6) | BASS (on, off) | BANK (quality, scale) | the chord type; the screen lists the black keys' types |
| **FX** (Clipper's sounds) | DRIVE | CHORUS | DELAY | REVERB (sends; recorded as CC 12, 93, 94, 91) | the reverb's model |
| **EDIT** on the SOUND page, a YARD sound (FX shows the same) | DETUNE (a second oscillator) / WAVE (SAW, PULSE, TRI) | SUB (a square two octaves down) / PW (the pulse's width) | RES (resonance) / NOISE (breath noise) | PLUCK (from a pad to a pluck) / FILTER (LOW, BAND, HIGH) | the other page |
| **GLO** on the PLAY page: SETTINGS (its screen: HOME for MORE, and where the USB port stands) | PLAY (poly, mono, + breath) | USB AUDIO (full, master) | MPE OUT (on, off) | BEND (12, 24, 48, 96) | USB: right, host to a USB MIDI device (the Diosynth); left, back to the computer |
| **EDIT** on the SOUND page, a Clipper sound | its engine's values 1-4 / 5-8 / ATTACK | ... / DECAY | ... / SUSTAIN | ... / RELEASE | the next page (ENGINE 1-4, ENGINE 5-8, ENVELOPE) |
| **GLO** on the SOUND page: MORE | PHONES MAX (MASTER's top for the headphones and speaker, 100% down to 10%: the FM-1's analog side can clip at a level the digital signal has clean; with USB AUDIO FULL the USB recording keeps its level) | SPREAD (MPE glide: OFF, or how much longer the highest voice glides than the lowest, 100% to 400%) | | | |
| **SEQ**: RECORDINGS | SLOT | LOOP (off, on) | | | the recording slot (1 to 8); the screen shows the eight slots |
| **SEL**: SCALE (either page) | the scale's number, thousands (0 to 4) | hundreds | tens | units (the number at most 4095) | the list (below), yours after it; SAVE: keep the number in your list |

**The scale is a pitch-class set's number**, 0 to 4095, as in the Music Suite: each of the twelve semitones above
the key is a bit (the key 1, a semitone up 2, a whole tone up 4 ... the major seventh 2048), and the scale is their
sum. C major is 2741 (1 + 4 + 16 + 32 + 128 + 512 + 2048), the minor 1453, the whole-tone scale 1365, the chromatic
4095. The key is always in: an even number plays as the next odd one. The SCALE layer shows the number, its name when
it has one, and the twelve semitones from the key (the key lit). The PLAY page's SCALE steps through the named
scales; a number of your own shows as PCS and its number. It is saved with the settings.

**The list**, on SCALE and on SEL's SELECT, from bright to dark: the diatonic modes LYDIAN, MAJOR (Ionian, the
default), MIXOLYDIAN, DORIAN, MINOR (Aeolian), PHRYGIAN, LOCRIAN; the pentatonics MAJ PENTA, SUS PENTA, MIN PENTA;
MAJ BLUES, MIN BLUES; MEL MIN, HARM MIN; the symmetric WHOLE TONE, OCT H-W, OCT W-H, AUGMENTED; Alex's PCO and
NOODLING; CHROMATIC. Then **your own**: on SEL's layer, set a number and SAVE (up to 16; SAVE there keeps the scale,
not the settings). The page names them, renumbers them and removes them (Your sounds and scales); one without a name
shows as its number.

### Encoders and buttons

| Control | Does |
| --- | --- |
| **ALGORITHM** | NOTES (left) / CHORDS (right) |
| **PRESETS** | The sound: mvane yard's saw, then Clipper's sounds by family (a fast turn: the next family) |
| **SELECT** | On the pages: the next or previous family's first sound. In a layer: as above |
| **HOME** | PLAY page / SOUND page (with GLO or EDIT open: their other half) |
| **OCT- / OCT+** | Octave; both together: panic (everything off) |
| **REC** | Tap: keep the last phrase (everything since the last 3 s of silence). Hold: keep the whole history. While a recording loops: tap, overdub (from the next pass; again, stop at its end); hold, undo the last layer |
| **PLAY** | Play / stop the recording |
| **SAVE** | Save the settings, and the recording into its slot |

**QUANT** as Coral Flower's: OFF, the chords' roots a semitone a white key; BASIC, the roots the scale's degrees; FULL, the chords' tones too, and the notes coming in over MIDI (USB, TRS, a hosted device), each to the nearest tone of the scale (down on a tie: C# to C in C major). A note's release finds the note it became, even after the scale or QUANT has changed.

Moving to another slot with a recording not saved: the first turn warns, a second within 3 s moves on and drops it.

## Playing

### NOTES

Keys play notes from F3, each on its own MPE channel with its pressure (so a DAW or an MPE synth hears each note's
pressure apart).

### CHORDS

- **White keys** are the scale's degrees, climbing the keyboard; higher keys are voiced higher.
- **Black keys** pick the chord type and stay latched (lit). A chord that is sounding takes the new type at once.
  - **Combine** (as Coral Flower): press black keys together, and they latch together. In the qualities bank they
    merge: the third held most wins (on a tie, the last pressed), a quality with the other third is left out, the
    added degrees join, and a sus with a third adds its 2nd or 4th. M6 + M9 is 6/9, M7 + S4 is maj7 add4, S2 + S4 is
    sus2 add4. The screen shows them as M6+M9. (In the scale bank the last one pressed counts.) A combination lasts
    until you change it; after a power cycle the last quality pressed comes back alone.
  - **Single notes**: tap the only latched key again, and it unlatches. The white keys then play the degree's root
    alone (NOTE on screen). Latch a quality while a note sounds, and it opens into the chord; the chord's voices glide
    from the note.
  - Qualities bank (the default, as Coral Flower's top row): **M6 M7 M9 S2 S4 m6 m7 m9 M m**, on the white key's root
    whatever the scale says.
  - Scale bank (ENV): **STACK** (thirds of the scale: 7th, 9th, then 13th and 11th), **7**, **TRIAD**, **ADD9**,
    **SUS**, **6/9**, **QUARTAL**, **11** (no third), **FIFTHS**, **V/** (the dominant of the degree).
- **F#5**, the last black key, is **HOLD**: a chord keeps sounding after you let its key go; the next white key changes
  it; HOLD again stops it. A held chord follows a change of sound (it sounds again on the new one).
- Every chord has a separate bass (EDIT turns it off) and 3 to 6 upper voices (LFO).
- Voice leading: each chord is voiced from the one before, so the voices move as little as they can; notes the two
  chords share are held, and the moving voices glide (GLIDE sets how long).
- **MPE glide** (SPREAD up, GLO on the SOUND page; MPE OUT on): each moving voice glides in its own time, the lowest
  in GLIDE's, the highest up to four times longer, so a change of chord blooms rather than moving as a block. A new
  note starts bent to the pitch of the note it takes over (the closest pairs first) and the bend eases back to 0:
  plain MPE, so YARD, Clipper and an MPE synth on MPE OUT glide alike, and a recording keeps the gesture. The engines' own
  glide is off meanwhile; a chord let go is glided from for 2 s. SPREAD OFF: the engines' glide as before.
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

PRESETS steps through **YARD**, mvane yard's own synth, then Clipper's factory sounds (337) in families, by how each
behaves (the full list: [SOUNDS.md](SOUNDS.md)).

**YARD** (13 sounds: SAW, PAD, ENSEMBLE, KEYS, PLUCK, BRASS, HOLLOW, CLAV; LEAD, BASS, WIND, FLUTE, REED mono): an
oscillator (WAVE: a band-limited SAW, a PULSE of width PW, 50% down to 5%, or a TRIANGLE), a second one detuned
(DETUNE), a square two octaves down (SUB), noise (NOISE: with breath, the breath's) through a filter that follows the
note (TONE: dark to bright; RES its resonance; FILTER: LOW 18 dB/oct, BAND or HIGH 12 dB/oct); PLUCK lowers the
sustain and opens the filter from five octaves up at each note, falling at DECAY: one knob from a pad to a pluck.
Pressure brightens and narrows the pulse (each note's own, with MPE), slide (CC74) moves the cutoff, breath
(POLY+BR, MONO+BR) is the level. The lightest sounds here (SAW about 12% of the FM-1 with a chord; BRASS and HOLLOW,
the heaviest, about half as much again), its voices over both cores. A YARD sound brings its parameters; GLIDE
stays the PLAY page's; the mono ones play one voice whatever PLAY says. To edit one: HOME to the SOUND page, then
EDIT (held or tapped): its two pages, SELECT between them, SAVE keeps the sound (FX shows the same pages). On the PLAY
page, EDIT is the chords' layer as before.
Every knob is recorded in the history as a CC on channel 1 (TONE 74, ATTACK 73, DECAY 75, RELEASE 72, RES 71, DETUNE
20, SUB 21, PLUCK 22, WAVE 23, PW 24, NOISE 25, FILTER 26).

**Your own sounds**, YARD's and Clipper's: FX held (or opened), SAVE keeps the sound playing (over itself if it is
already one of yours), up to 32 of each, kept across a restart.
- A **YARD** sound is its thirteen values (oscillator, filter, envelope, noise): saved as USER n,
  listed after YARD's nine in their family (mono: LEADS EZ; PLUCK 64 or more: HITS EZ; else CHORDS EZ).
- A **Clipper** sound is a factory sound and what was changed on it: up to 14 parameters (on the panel: its four
  macros on the SOUND page; on EDIT (SOUND page) its engine's eight values over two pages and its envelope, SELECT
  between; its four sends on FX) and the reverb's model (FX, SELECT); saved as its sound's name and a + (GLASS PAD+),
  listed first in that sound's family. EDIT and SAVE (or FX and SAVE) keeps it.
On the computer they are one file, `yard.ysb`: the page's **Your sounds** card lists them to edit (a Clipper sound:
its sound, reverb and changes as `name=value`, any of Clipper's 126 parameters by name or `#number`), add or remove,
send back, or save as JSON to share; `tools/yard.py sounds` fetches them as JSON and `tools/yard.py sounds --put
mine.json` sends them (replacing the FM-1's). In the JSON a YARD sound is `{"slot", "name", "mono", "tone",
"release", "glide", "attack", "decay", "res", "detune", "sub", "pluck", "wave", "pw", "noise", "filter"}` (wave and
filter: 0 SAW / LOW, 64 PULSE / BAND, 127 TRI / HIGH), a Clipper sound `{"slot", "name", "sound"
(or "engine" and "preset"), "reverb", "changes": [{"id" or "name", "value"}]}` (BRITE, TIMBR, MOTN and SPACE are a
HYBRID sound's M1 to M4, as on the panel).

| Family | What | For |
| --- | --- | --- |
| **CHORDS EZ** (20) | Sustained, polyphonic, light: YARD's SAW, PAD, ENSEMBLE, BRASS; SAMPLE's pads, the WHEEL organs, PHASE's keys | Chords, with CPU to spare |
| **LEADS EZ** (72) | Mono, sustained, light: YARD's LEAD, BASS, WIND; most mono leads and basses | A wind controller |
| **HITS EZ** (18) | Short, polyphonic, light: YARD's KEYS, PLUCK; light plucks, bells, stabs | Stabs, hits, bursts, arpeggios |
| **CHORDS HRD** (109), **LEADS HRD** (10), **HITS HRD** (88) | The same, heavier on the CPU | |
| **OTHER** (29) | Mono and short, percussion, noises | |

**EZ** and **HRD** say the load on the FM-1 (a chord of six on its two cores: EZ about 40% or less), not the size
of the sound. In each family YARD's own sounds come first, then yours (FX and SAVE), then Clipper's. The screen
names the family as the sound changes (CHORDS EZ, SAMPLE CHOIR). A fast turn of PRESETS, or SELECT on the play
pages, goes to the next family's first sound (back: this family's first, then the one before). Within a family,
Clipper's sounds of a kind are together, in Clipper's own filing: pads, organs, keys, stabs, plucks, leads, basses.
Each of Clipper's sounds is trimmed so that a chord of six (the bass and five voices) stays under its master
limiter: Clipper's levels suit a few notes, and a limiter working hard on a held chord is heard as grit (`make
clp-trim`), played hard: velocity 127, full pressure, chords struck in quick succession. Chord sounds are quieter
than in Clipper (MASTER makes it up); none is limited more than about 2 dB.
WAVE is left out for now (RAM); ACID, NOISE and LOFI are out of the list. A sound saved in the settings stays the
same across the reordering (it is kept by name, not by place). Clipper's sounds are dry, as Clipper ships them: no delay, no reverb. They respond to MPE, pressure (as
aftertouch), the mod wheel, expression, sustain, breath, and the standard CCs (71 resonance, 72 release, 73 attack,
74 cutoff, 75 decay, 5 glide, 16 to 19 the macros). A sound that full pressure or slide left unchanged as Clipper
made it (measured, `make clp-expr`) gets them routed when it loads: pressure to its tone and a little to its level,
slide to its tone, nothing changed at rest; some organs, phase and wave-sequence sounds still answer little. The
PLAY page's GLIDE is yours, not the sound's: it applies to every sound as it loads.

**Mono sounds** (89 of them, as Clipper's presets say) show MONO when you pick them, and MONO SOUND on the bottom line
while POLY is set: they play one note of a chord.

**Cost.** Each sound is tagged EZ or HRD (on the bottom line), measured on a computer as the FM-1 plays it: a chord
of six, its voices over the two cores (`make clp-cost`, `tools/clp_cost.c`; fit to Alex's readings on the FM-1).
Engines, lightest first, by their sounds' mean: SAMPLE, PHASE, DIGITAL, ANALOG, WSEQ, WHEEL, TRIO, VOICE, VPS,
TERRAIN, VECTOR, FM6, FMX, HYBRID, GRAIN, MACRO, STRINGS, PHYS, JUNO. YARD's sounds are lighter than any. About 15%
of a light sound is Clipper's effect buses, which stop only once every send has been silent for 20 s.

## The recorder

- It is always on, from power-on: everything you play goes into the history (notes, pressure, knob moves, MIDI in),
  as events, so its length is counted in events (32,768), not in time.
- It sleeps: a pause longer than 4 s is kept as 4 s.
- **REC** keeps a recording from the history: a tap, the last phrase; held, everything. Nothing is copied: the recording stays
  where it is in the history, protected until you save it (the history shows FULL: SAVE if you play on until it
  reaches it).
- **PLAY** replays the recording through the current sound: try a recording through different sounds.
- **In any scale:** while a recording plays, change the KEY or the SCALE (the PLAY page, or SEL's number) and the recording
  follows: each note to the same degree of the new scale, in the same octave (from C major to C minor, E becomes Eb;
  to D minor, F). A scale with fewer or more tones takes the degrees in proportion; a note outside the scale keeps its
  distance from the degree below it. The scale in force when you pressed PLAY is the recording's own.
- **LOOP** (SEQ, K2): the recording plays again from its start when it ends, on time, still in the scale you moved it to
  (the screen: LOOPING). The loop is the recording's length: from its first note to where you tapped REC.
- **Overdub**: while a recording loops, tap **REC**. From the top of the next pass (the screen: OVERDUB AT TOP, then
  OVERDUB 1, 2 ...) what you play goes in with the recording, and at the end of each pass that pass becomes the recording: a
  layer. Play over it again on the next pass, and the next. Tap REC again to stop at the end of the pass (OVERDUB
  n END); PLAY stops at once (the pass under way is not kept). **Hold REC** while it loops: the last layer undone
  (one step). What a layer keeps is what you heard: the recording as it played (in the scale it was moved to) and what
  you played over it, knob moves included. Stop the recording, then SAVE, to keep the layers in the slot. Each layer is a
  copy in the history, so a long, busy loop fills it sooner (FULL: SAVE).
- **SAVE** writes the recording into the current slot (the sound stops for a moment while the flash is written). Saved recordings
  play from the flash.
- **SEQ** (held or tapped) and SELECT pick one of eight slots. A recording sent from the computer (the page, `yard.py put`)
  makes its slot the current one, ready for PLAY (unless the current slot holds a recording not yet saved: then it waits in
  its slot, and the screen says so).

## Your recordings on the computer

- **The page**, <https://enkerli.github.io/mvane-yard/>, in Chrome or Edge with the FM-1 plugged in by a data cable
  (stop PLAY first): Connect; the eight slots; a piano roll of the selected recording; **Download as MIDI** (one channel:
  one clip in a DAW, each note's pressure as poly aftertouch), **MIDI with MPE channels** (a channel a note, as
  played), **Download .mvt**; **Send a MIDI or .mvt file to this slot** (a MIDI file's notes, controllers, pressure
  and bend, all tracks merged, tempo honoured, 30,000 events at most); **Download all (ZIP)**: every slot as both
  MIDI files and as .mvt, with a README; your sounds and scales; settings download and restore.
- **.mvt** is mvane yard's own recording format: the recording whole, exactly as the FM-1 keeps and plays it (each
  event one 32-bit word: the milliseconds since the one before, then a MIDI message; a header "MVT1", version, count,
  length: `core/rec.h`). Keep it to keep a recording whole; the MIDI files are made from it.
- **The command line**: `python3 tools/yard.py take --slot 3` (slot 3 as `take3.mid`), `yard.py takes` (every slot),
  `yard.py put song.mid --slot 5`, `yard.py settings`, `yard.py ls`. Needs `pip install mido python-rtmidi`.

## The plugin (VIBE FM VST)

TheFirmwareHub's converter turns mvane yard into a VIBE FM firmware; it plays as on the FM-1. In the plugin the recording
stays in memory (its platform has no flash for files): capture and replay work, a recording is gone when the session
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
