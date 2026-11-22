# Where mvane yard fits: Alex's Music Suite

mvane yard is one instrument in a family of Alex's music tools that share ideas, conventions and, where they can,
code.

## The family

| Project | What | What mvane yard takes from it, or gives |
| --- | --- | --- |
| [music-suite](https://github.com/Enkerli/music-suite) | A TypeScript monorepo of web apps and plugins: theory, progression generation (`proggen`), accompaniment, MIDI tools, Vane's web panel | The **pitch-class-set numbers** (bit 0 the root: C major 2741, `CONVENTIONS.md`); the reference **voice leading** (`packages/theory/src/voiceLeading.ts`: Tymoczko's minimal taxicab voice leading, with none, loose and strict modes in `proggen`), which mvane yard means to match in C against music-suite's shared vectors |
| [Vane](https://github.com/Enkerli/Vane) | A physical-model and wavetable synth plugin (AU, VST3, CLAP), with WindCore, a reed-and-cone wind model, voice input and pitch tracking | The goal of a **Vane voice on the FM-1**: a mono WindCore lead first, then its wavetable engine; the breath and MPE habits Vane was built for (a Diosynth, an Exquis) |
| ProgGenie | An AUv3 MIDI processor that walks corpus transition statistics for chord progressions, and keeps an opinion about each chord change | **Progressions on the device** (roadmap M3): its tables and curation weights in C, phrases voiced for the whole loop by `core/harmony` |
| mvane yard | This firmware | The **retrospective recorder** (a history that never stops, takes captured after the fact) and chords from any pitch-class set on an FM-1 |

## The name

*Yard*, as Alex tells it: first hills, terrain, textures, a landscape with a weathervane; then a playground, a
schoolyard. mvane yard is meant for playful exploration. The *vane* sits less inside the image, without disturbing it.

## Shared conventions

- **Pitch-class sets as numbers**, bit 0 the root: the screen shows each scale's (PCO 1723, Noodling 1267), and an
  arbitrary set by number is on the roadmap.
- **MPE everywhere**: mvane yard sends and receives it as Vane and the suite's MIDI tools do.
- **Takes as MIDI**: what the FM-1 keeps goes to a DAW (Bitwig) as an ordinary MIDI file, one clip.
- **Judged by ear**: sound is checked by listening first; measurements serve the ear.

## Portable core

`core/` is plain C99 with no platform calls (chords, harmony, the recorder), so the same code can run in a plugin
beside Vane, in the browser, or on another device. The roadmap's file-compatible standalone and plugin build on it.
