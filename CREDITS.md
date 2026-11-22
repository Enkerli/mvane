# Credits

mvane yard stands on other people's firmware for the M-VAVE FM-1, and on their ideas. Thank you.

## Coral Flower

[Coral Flower](https://github.com/FicusFicusov/CoralFlower) by FicusFicusov (GPL-3.0), a fork of ChoralRoot FM-1 by
Quixotic7, itself after Felucca, is the instrument mvane yard's chord mode grew from. Alex plays it by pressing keys
for successions of chords that go well together, then replays a sequence through other sounds, and mvane yard was
asked to keep that joy. What comes from it:

- The **chord qualities on the black keys**, latched, in its order: M6 M7 M9 S2 S4 m6 m7 m9 M m (the "top lock").
- **HOLD** on the top row's last key, F#5.
- **QUANT** as its spec defines it: OFF (chromatic roots, the tonic on the fifth key), BASIC (roots on the scale's
  degrees), FULL (the chord's tones snapped to the scale); and in NOTES, keys snapped to the nearest scale tone, down
  on a tie.
- The **PLAY page**: QUANT, KEY, SCALE, GLIDE on the four knobs.
- **Glide across whole chords**, the moving voices sliding while the common tones hold.
- The separate **bass** under the chords, and the sense that a chord's sound matters as much as its notes (its VA
  pads with chorus and reverb).
- Its looper's lesson: what was played should be kept, and replayed through other sounds.
- Its **Lock**: qualities pressed together latch together and **combine** by its merge (the third held most wins,
  added degrees join, a sus with a third adds its 2nd or 4th: `cf_merge_forms` in its `cf_chord.c`, reimplemented in
  `core/chords.c chord_build_qualities`); the only latched quality tapped again unlatches, and the keys play **single
  notes**, so a phrase can open from a note into chords.

No Coral Flower code is in mvane yard; its source was read for its design. Coral Flower's own inspiration is
Telepathic Instruments' Orchid; ChoralRoot's engine (`cr_engine`) is Quixotic7's.

## Clipper

[Clipper](https://github.com/isod89/clipper-fm1) by 3dSam and the Clipper contributors (GPL-3.0), after
[SLOOP](https://github.com/isod89/sloop-fm1) and [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita
(Hügelton Instruments): its synth core is vendored in `app/clipper/` (22 engines, their factory sounds, voices,
modulation, MPE, effects, master chain). Within it: the FM6 engine's msfa core (Google Inc. and Pascal Gauthier,
from Dexed, Apache-2.0), the PHYS engine's models (DaisySP by Electrosmith and Emilie Gillet, Rings' sympathetic
strings by Emilie Gillet, MIT), the PHASE engine's waveforms (CrispyZebra by Leo Kuroshita). See
`app/clipper/README.md` and `app/clipper/LICENSE`.

## The platform

- [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal and contributors (MPL-2.0): the platform
  layer, host simulator, browser emulator, companion services, build and release tools.
- [felucca-bsp](https://github.com/hugelton/felucca-bsp) by Leo Kuroshita (MPL-2.0): the board support.
- [Melodee](https://github.com/keremimo/melodee) by Kerem Kilic: the second core's start and the USB host, through
  felucca-bsp; Melodee's notes on USB host with audio-and-MIDI devices.
- The FM-1 Firmware Hub (TheFirmwareHub) and its VIBE FM plugin, which runs mvane yard in a DAW.
- `zvenson/fm1-build`, the shared release builder.

## Alex's own

The harmony and voice-leading ideas come from Alex's [music-suite](https://github.com/Enkerli/music-suite) and
ProgGenie (see [docs/CONTEXT.md](docs/CONTEXT.md)); Alex's pitch-class sets PCO and Noodling come from their Exquis
layouts; the PAIRS voicing (3rd and 7th under root and 5th) is how Alex voices sevenths.

## Fonts and words

The screen's fonts and the screen reader's words come with fm1-base (Barlow, SIL OFL 1.1; fm1-reader).
