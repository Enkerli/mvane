# mvane yard

A custom firmware for the M-VAVE FM-1, on [fm1-base](https://github.com/charlesvestal/fm1-base). See
[docs/ROADMAP.md](docs/ROADMAP.md) for where it is going.

**0.4:** a 12-voice synth (band-limited saw, key-tracked lowpass from dark to bright) that plays MPE, notes or
chords from any of eight pitch-class sets with a separate bass and smooth voice leading, MPE out over USB, duplex USB
audio (from fm1-base), and a recorder that is always on: play, then decide to keep it. The history sleeps when you
stop, so waiting costs nothing.

| Control | Does |
| --- | --- |
| White keys | NOTES: notes from F3, each on its own MPE channel with its pressure. CHORDS: the scale's degrees, up the keyboard (higher ones voiced higher). Common tones hold and the moving voices glide |
| Black keys | CHORDS: the chord type, latched: qualities (M6 M7 M9 S2 S4 m6 m7 m9 M m) or, with ENV, types from the scale (STACK, 7, TRIAD, ADD9, SUS, 6/9, QUARTAL, 11, FIFTHS, V/ the dominant of the degree); a sounding chord takes it at once. The last one (F#5) is HOLD |
| ALGO | NOTES (left) / CHORDS (right) |
| PRESET | the key, C to B |
| SELECT | the scale: MAJOR, MINOR, DORIAN, HARM MIN, PENTA, PCO (1723), NOODLING (1267), OCTATONIC; its pitch-class-set number on screen (music-suite's convention) |
| FX | voicing: CLOSE, OPEN, WIDE |
| LFO | chord size: 3 to 6 upper voices (plus the bass) |
| ENV | the black keys' bank: fixed qualities as Coral Flower's (M6 M7 M9 S2 S4 m6 m7 m9 M m, the default) or types from the scale |
| EDIT | bass on / off |
| GLO | USB audio at full level (MASTER for speaker and headphones only) or at MASTER |
| K1, K2, K3, K4 | tone, release, glide, attack: sent as CC74, CC72, CC5, CC73 on the master channel and recorded, so a take replays your knob gestures |
| OCT- / OCT+ | octave |
| REC | tap: keep the last phrase (everything since the last 3 s of silence); hold: keep the whole history |
| PLAY | play / stop the take |
| SAVE | the settings and the take, kept across power-off |

MIDI in (USB, TRS) plays the synth with MPE: per-note pitch bend (bend range by RPN), slide (CC74 on a member
channel), channel and poly pressure. It goes into the history too.

## Your take on the computer

The take SAVE keeps is a file over USB (fm1-base's companion services). Two ways to get it:

- **The page** (`web/index.html`): Connect, see the take as a piano roll, **Download as MIDI** (or `.mvt`), or send a
  MIDI file to the FM-1 as the take PLAY plays; settings download and restore. Chrome or Edge, served over `http://localhost`
  or HTTPS (Web MIDI needs a secure page): `python3 -m http.server 8737 --directory web`, then http://localhost:8737.
- **The command line**: `python3 tools/yard.py take` writes `take.mid`; `tools/yard.py put song.mid` sends a MIDI file
  (notes, controllers, pressure, bend; tracks merged; tempo honoured); `tools/yard.py settings` saves the settings.
  Needs `pip install mido python-rtmidi`.

Stop PLAY first: the FM-1 refuses files while a take plays. The MIDI file keeps the MPE channels, so an MPE instrument
(Vane, Bitwig's) plays it back as played. In the VIBE FM plugin the take stays in memory (no flash there).

## Layout

| Path | What |
| --- | --- |
| `core/` | Portable C99, no platform calls: `chords` (pitch-class sets, chord types, voicing styles, names), `harmony` (whole-loop voice leading, for ProgGenie), `rec` (history, capture, player, take files). Shared by the firmware, the tools and a future plugin |
| `app/` | The firmware app: `app.c` (panel, bus, recorder, chords), `synth.c` (the voice, at -O2), `fm1.toml`, the host-simulator tests |
| `base/` | fm1-base, a git submodule pinned at 565fafe (with felucca-bsp inside) |
| `tools/` | `yard.py` (take and settings over USB), `mvt2mid.py` (take to MIDI), their tests |
| `web/` | The page: take as MIDI, MIDI as the take, settings (`take.js` converts; `lib/companion.js` and `fm1ports.js` are fm1-base's, MPL-2.0) |
| `docs/` | The roadmap; the earlier starter notes (platform bring-up, recording design, Vane feasibility) |

## Build and test

    git clone --recursive <this repository>
    make check                   # core checks on the desktop (any C compiler)
    ./build.sh --host            # the host simulator, then:
    app/build/host/mvane-yard_host app/tests/basic.txt app/build/host
    ./build.sh                   # app/build/mvane-yard.fwsc: needs the JieLi toolchain (below)

The host build needs Python 3 with Pillow and FriBiDi (`brew install fribidi` on macOS).

The JieLi toolchain only runs on Linux x86-64. Three ways to get the `.fwsc`:

1. **GitHub Actions** (`.github/workflows/build.yml`): every push builds it; download `mvane-fwsc` from the run.
2. **A Linux x86-64 PC:** `base/tools/setup_toolchain.sh --only uv,python,jieli,ac79-sdk`, then `./build.sh`.
3. **This Mac, in a container:** install Docker or Podman; `base/tools/build.py` runs the toolchain in the pinned
   image by itself.

Install with M-Patcher, or `python3 base/tools/fm1_install.py app/build/mvane-yard.fwsc`. Back up the current firmware
first.

## Licence

GPL-3.0-only (see `LICENSE`), so code from Clipper and Coral Flower (both GPL-3.0) can come in. fm1-base and
felucca-bsp stay MPL-2.0. The firmware builds without Bluetooth, so none of JieLi's closed libraries are linked.
