# Clipper's synthesizer in mvane yard

`src/` is the synth core of [Clipper](https://github.com/isod89/clipper-fm1) (commit 7bc15a9), by 3dSam and the
Clipper contributors, after SLOOP and [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton
Instruments): its 23 engines, their 407 factory sounds, the voices, modulation, effects and master chain. GPL-3.0-only
(`LICENSE`); the FM6 engine's msfa core is Apache-2.0 (`LICENSES/Apache-2.0-msfa.txt`), the PHYS engine's models MIT
(the notices are in `src/phys_dsp.c`, `src/phys_symp.c`).

Changes to Clipper's files: one, mechanical: `__attribute__((section(".pool")))`, `(".lowram")` and `(".noinit")`
became `CLP_POOL`, `CLP_LOWRAM`, `CLP_NOINIT` (`clp.c` defines them for the FM-1 and the host). Two switches for
RAM: `engines.c` builds `eng_wave_off.c` (WAVE silent, no tables: 72 KB) unless `CLP_WAVE`, and `fx.c`'s delay line
is `CLP_DLY_LEN` long (16384: 0.74 s, half Clipper's). `src/seq_ctl.c` is
copied verbatim from Clipper's `seq.c` (the part's controllers and the standard CC map). `hal/fm1_xip.h` is mvane
yard's stand-in (nothing reads the flash). `gen/` holds the tables Clipper's `tools/gen_*.py` make.
`src/voice.c` `vs_ok` also lets SAMPLE and WHEEL split their voices between the cores (2026-11-22: each one's render
writes its own voice only; `make clp-race` renders every sound on two threads under ThreadSanitizer, bit for bit
against one core). `src/fx.c` has the effect gate's hook (`CLP_FX_GATE`) and the sounds' trim (`CLP_TRIM`).
`gen/felucca_samples.h` keeps only Clipper's own synthesized sample sets (the `CLIPPER_SYNTH` build's, from its
`tools/gen_clipper_sets.py`); Felucca's recorded sets, for builds that are not Clipper's synth, are left out: no
recording ships in mvane yard.

`clp.c` is the glue: the power-on, a sound loaded, MIDI in (notes, bend, pressure, MPE, the controllers), the block
render, and stand-ins for what Clipper's UI, sequencer, USB and HAL gave the synth. It is its own unit (fm1.toml
`o2`), so Clipper's names stay inside it; `clp.h` is all mvane yard sees.

Not here: Clipper's UI, sequencer, arpeggiator, song mode, projects, user presets, the editor protocol, its USB and
second-core code: fm1-base's `plat_cpu2_*` takes its place (`clp.c`), and Clipper's voice split runs on it.
