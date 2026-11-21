# Clipper's synthesizer in mvane yard

`src/` is the synth core of [Clipper](https://github.com/isod89/clipper-fm1) (commit 7bc15a9), by 3dSam and the
Clipper contributors, after SLOOP and [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton
Instruments): its 23 engines, their 407 factory sounds, the voices, modulation, effects and master chain. GPL-3.0-only
(`LICENSE`); the FM6 engine's msfa core is Apache-2.0 (`LICENSES/Apache-2.0-msfa.txt`), the PHYS engine's models MIT
(the notices are in `src/phys_dsp.c`, `src/phys_symp.c`).

Changes to Clipper's files: one, mechanical: `__attribute__((section(".pool")))`, `(".lowram")` and `(".noinit")`
became `CLP_POOL`, `CLP_LOWRAM`, `CLP_NOINIT` (`clp.c` defines them for the FM-1 and the host). `src/seq_ctl.c` is
copied verbatim from Clipper's `seq.c` (the part's controllers and the standard CC map). `hal/fm1_xip.h` is mvane
yard's stand-in (nothing reads the flash). `gen/` holds the tables Clipper's `tools/gen_*.py` make.

`clp.c` is the glue: the power-on, a sound loaded, MIDI in (notes, bend, pressure, MPE, the controllers), the block
render, and stand-ins for what Clipper's UI, sequencer, USB and HAL gave the synth. It is its own unit (fm1.toml
`o2`), so Clipper's names stay inside it; `clp.h` is all mvane yard sees.

Not here: Clipper's UI, sequencer, arpeggiator, song mode, projects, user presets, the editor protocol, its USB and
second-core code. One core only for now (Clipper splits the voices over two).
