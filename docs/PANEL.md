# Panel proposal (for 0.7)

0.6's panel grew one control at a time (see [MANUAL.md](MANUAL.md)). Alex's notes after 0.6: SEQ / HOME are not how
FM-1 firmwares move through clips (SELECT does, in the SEQ context); in play, K1 to K4 should make sense together,
one page as Coral Flower's (QUANTIZE, KEY, SCALE, GLIDE), another for the synth's macros. This proposal follows the
FM-1's habits (M-VAVE's firmware, Clipper, Coral Flower): PRESETS is the sound, SELECT moves through whatever the
context lists, a held button is a layer that changes what the knobs do, and the screen always names the four knobs.

## Knob pages

The four knobs' labels sit on the screen above them. **HOME** switches between the two play pages.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| **PLAY** (as Coral Flower's) | QUANT (off, basic, full) | KEY | SCALE | GLIDE |
| **SOUND** (the synth's macros) | BRITE | TIMBR | MOTN | SPACE |

On Clipper's sounds SOUND is Clipper's own four macros (CC 16 to 19). On mvane yard's saw: TONE, RELEASE, ATTACK,
and a fourth to choose (drive, or detune). Every knob move is recorded with the take, as now.

## Layers: hold a button, turn a knob

| Hold | K1 | K2 | K3 | K4 | SELECT |
| --- | --- | --- | --- | --- | --- |
| **EDIT** (chords) | VOICING (close, open, wide; more later) | VOICES (3 to 6) | BASS (on, off) | BANK (qualities, scale) | chord type |
| **FX** (Clipper's sends) | CHORUS | DELAY | REVERB | DRIVE | the reverb's model |
| **GLO** (settings) | PLAY (poly, mono, + breath) | USB AUDIO (full, master) | MPE OUT (on, off) | BEND RANGE | |
| **SEQ** (takes) | | | | | the take slot (1 to 8) |

A tap of a layer button shows its page until the next tap (for one hand). The black keys keep choosing the chord type
and F#5 stays HOLD.

## Buttons

| Button | Tap | Hold |
| --- | --- | --- |
| REC | keep the last phrase | keep the whole history |
| PLAY | play / stop the take | |
| SAVE | settings, and the take into its slot | |
| SEQ | the takes page (SELECT: the slot; the piano roll of the take) | the takes layer |
| HOME | PLAY page / SOUND page | |
| ARP | (later: the arpeggiator) | |
| SEL | shift (for the rare second meanings) | |
| OCT- / OCT+ | octave | both: panic (all notes off) |

## Encoders

| Encoder | Does |
| --- | --- |
| ALGORITHM | NOTES / CHORDS (later: PADS, the chord memory) |
| PRESETS | the sound |
| SELECT | what the context lists: on the play pages, the sound's kind (pads, keys, strings, bells, leads, organs, choirs, basses: a jump in the PRESETS list); with SEQ, the take slot; with EDIT, the chord type |

## Discoverable

- Each knob's label on screen, lit while it turns, with its value.
- Holding a layer button shows its page at once.
- The chord bank named on the chord line (QUALITIES / FROM SCALE), and the black keys' types shown while EDIT is held.
