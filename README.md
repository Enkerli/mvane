# mvane

A custom firmware for the M-VAVE FM-1, on [fm1-base](https://github.com/charlesvestal/fm1-base). See
[docs/ROADMAP.md](docs/ROADMAP.md) for where it is going.

**0.2:** an 8-voice synth (band-limited saw, key-tracked lowpass from dark to bright) that plays MPE, notes or
chords from any of eight pitch-class sets with a separate bass and smooth voice leading, MPE out over USB, duplex USB
audio (from fm1-base), and a recorder that is always on: play, then decide to keep it. The history sleeps when you
stop, so waiting costs nothing.

| Control | Does |
| --- | --- |
| White keys | NOTES: notes from F3, each on its own MPE channel with its pressure. CHORDS: the scale's degrees, up the keyboard (higher ones voiced higher). Common tones hold and the moving voices glide |
| Black keys | CHORDS: the chord type, latched (9, 7, TRIAD, ADD9, SUS, 6/9, QUARTAL, 11, FIFTHS, V/ the dominant of the degree); a sounding chord takes it at once. The last one (F#5) is HOLD |
| ALGO | NOTES (left) / CHORDS (right) |
| PRESET | the key, C to B |
| SELECT | the scale: MAJOR, MINOR, DORIAN, HARM MIN, PENTA, PCO, NOODLING, OCTATONIC |
| FX | voicing: CLOSE, OPEN, WIDE |
| EDIT | bass on / off |
| GLO | USB audio at full level (MASTER for speaker and headphones only) or at MASTER |
| K1, K2, K3, K4 | tone, release, glide, attack: sent as CC74, CC72, CC5, CC73 on the master channel and recorded, so a take replays your knob gestures |
| OCT- / OCT+ | octave |
| REC | tap: keep the last phrase (everything since the last 3 s of silence); hold: keep the whole history |
| PLAY | play / stop the take |
| SAVE | the settings and the take, kept across power-off |

MIDI in (USB, TRS) plays the synth with MPE: per-note pitch bend (bend range by RPN), slide (CC74 on a member
channel), channel and poly pressure. It goes into the history too.

A take is also a file over USB (`take.mvt`, fm1-base's companion services). `tools/mvt2mid.py take.mvt` makes a
MIDI file of it, MPE channels kept.

## Layout

| Path | What |
| --- | --- |
| `core/` | Portable C99, no platform calls: `chords` (pitch-class sets, chord types, voicing styles, names), `harmony` (whole-loop voice leading, for ProgGenie), `rec` (history, capture, player, take files). Shared by the firmware, the tools and a future plugin |
| `app/` | The firmware app: `app.c` (panel, bus, recorder, chords), `synth.c` (the voice, at -O2), `fm1.toml`, the host-simulator tests |
| `base/` | fm1-base, a git submodule pinned at 565fafe (with felucca-bsp inside) |
| `tools/` | `mvt2mid.py` |
| `docs/` | The roadmap; the earlier starter notes (platform bring-up, recording design, Vane feasibility) |

## Build and test

    git clone --recursive <this repository>
    make check                   # core checks on the desktop (any C compiler)
    ./build.sh --host            # the host simulator, then:
    app/build/host/mvane_host app/tests/basic.txt app/build/host
    ./build.sh                   # app/build/mvane.fwsc: needs the JieLi toolchain (below)

The host build needs Python 3 with Pillow and FriBiDi (`brew install fribidi` on macOS).

The JieLi toolchain only runs on Linux x86-64. Three ways to get the `.fwsc`:

1. **GitHub Actions** (`.github/workflows/build.yml`): every push builds it; download `mvane-fwsc` from the run.
2. **A Linux x86-64 PC:** `base/tools/setup_toolchain.sh --only uv,python,jieli,ac79-sdk`, then `./build.sh`.
3. **This Mac, in a container:** install Docker or Podman; `base/tools/build.py` runs the toolchain in the pinned
   image by itself.

Install with M-Patcher, or `python3 base/tools/fm1_install.py app/build/mvane.fwsc`. Back up the current firmware
first.

## Licence

GPL-3.0-only (see `LICENSE`), so code from Clipper and Coral Flower (both GPL-3.0) can come in. fm1-base and
felucca-bsp stay MPL-2.0. The firmware builds without Bluetooth, so none of JieLi's closed libraries are linked.
