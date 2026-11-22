# mvane yard

A custom firmware for the M-VAVE FM-1, on [fm1-base](https://github.com/charlesvestal/fm1-base). See
[docs/MANUAL.md](docs/MANUAL.md) for how to play it, [docs/ROADMAP.md](docs/ROADMAP.md) for where it is going,
[docs/CONTEXT.md](docs/CONTEXT.md) for how it fits with Alex's Music Suite (music-suite, Vane, ProgGenie), and
[CREDITS.md](CREDITS.md) for whose work it builds on: Coral Flower's chord mode, Clipper's synth, fm1-base.

**0.4:** a 12-voice synth (band-limited saw, key-tracked lowpass from dark to bright) that plays MPE, notes or
chords from any of eight pitch-class sets with a separate bass and smooth voice leading, MPE out over USB, duplex USB
audio (from fm1-base), and a recorder that is always on: play, then decide to keep it. The history sleeps when you
stop, so waiting costs nothing.

The controls, page by page and layer by layer: [docs/MANUAL.md](docs/MANUAL.md).

MIDI in (USB, TRS) plays the synth with MPE: per-note pitch bend (bend range by RPN), slide (CC74 on a member
channel), channel and poly pressure. It goes into the history too.

## Your take on the computer

The take SAVE keeps is a file over USB (fm1-base's companion services). Two ways to get it:

- **The page**, https://enkerli.github.io/mvane-yard/ (`web/index.html`): Connect, see the take as a piano roll, **Download as MIDI** (or `.mvt`), or send a
  MIDI file to the FM-1 as the take PLAY plays; settings download and restore. Chrome or Edge. To run it from a checkout
  instead: `python3 -m http.server 8737 --directory web`, then http://localhost:8737 (Web MIDI needs HTTPS or localhost).
- **The command line**: `python3 tools/yard.py take` writes slot 1's take as `take1.mid` (`--slot N`, or `takes` for
  all eight); `tools/yard.py put song.mid --slot 3` sends a MIDI file
  (notes, controllers, pressure, bend; tracks merged; tempo honoured); `tools/yard.py settings` saves the settings; `tools/yard.py sounds` your own YARD sounds as JSON (`--put` sends them).
  Needs `pip install mido python-rtmidi`.

Stop PLAY first: the FM-1 refuses files while a take plays. Download as MIDI puts everything on one channel, so a DAW makes one clip of it
(each note's pressure as poly aftertouch: per-note pressure in Bitwig); `--mpe` or the page's MIDI with MPE channels
keeps a channel a note. In the VIBE FM plugin the take stays in memory (no flash there).

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

1. **GitHub Actions** (`.github/workflows/build.yml`): every push builds it; download `mvane-yard-0.10-dev+COMMIT` from the run (the version, with the commit, is the file's name and what the screen and the page show).
2. **A Linux x86-64 PC:** `base/tools/setup_toolchain.sh --only uv,python,jieli,ac79-sdk`, then `./build.sh`.
3. **This Mac, in a container:** install Docker or Podman; `base/tools/build.py` runs the toolchain in the pinned
   image by itself.

Install with M-Patcher, or `python3 base/tools/fm1_install.py app/build/mvane-yard.fwsc`. Back up the current firmware
first.

## Releases

Tag `vX.Y` (or `vX.Y-beta` for a pre-release) and push the tag: `.github/workflows/release.yml` (fm1-base's, through
the shared FM-1 builder `zvenson/fm1-build`) builds the package from source, runs `tests/run_tests.sh`, attests the
build's provenance (which the FM-1 Firmware Hub checks) and publishes the release with `mvane-yard-X.Y.fwsc`,
`firmware.json` and the `.rfm1`.

## Licence

GPL-3.0-only (see `LICENSE`), compatible with Clipper's and Coral Flower's (both GPL-3.0); credits in
[CREDITS.md](CREDITS.md). fm1-base and
felucca-bsp stay MPL-2.0. The firmware builds without Bluetooth, so none of JieLi's closed libraries are linked.
