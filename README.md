# mvane

A custom firmware for the M-VAVE FM-1, on [fm1-base](https://github.com/charlesvestal/fm1-base). See
[docs/ROADMAP.md](docs/ROADMAP.md) for where it is going.

**0.1 (proof of concept):** an 8-voice synth, notes or diatonic chords with smooth voice leading, MPE out over USB,
duplex USB audio (from fm1-base), and a recorder that is always on: play, then decide to keep it.

| Control | Does |
| --- | --- |
| Keys | NOTES: notes from F3, each on its own MPE channel with its pressure. CHORDS: white keys are the key's degrees (I ii iii IV V vi vii, twice), black keys the dominant of the white key to their left. Common tones hold and the moving voices glide |
| ALGO | NOTES (left) / CHORDS (right) |
| PRESET | the key, C to B |
| SELECT | major (left) / minor (right) |
| K1, K2, K3 | tone, release, glide: sent as CC74, CC72, CC5 and recorded, so a take replays your knob gestures |
| OCT- / OCT+ | octave |
| REC | tap: keep the last phrase (everything since the last 3 s of silence); hold: keep the whole history |
| PLAY | play / stop the take |
| SAVE | the settings and the take, kept across power-off |

A take is also a file over USB (`take.mvt`, fm1-base's companion services). `tools/mvt2mid.py take.mvt` makes a
MIDI file of it, MPE channels kept.

## Layout

| Path | What |
| --- | --- |
| `core/` | Portable C99, no platform calls: `harmony` (voicing, voice leading, degrees), `rec` (history, capture, player, take files). Shared by the firmware, the tools and a future plugin |
| `app/` | The firmware app: `app.c`, `app.h`, `fm1.toml`, the host-simulator tests |
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
