# User testing: a newcomer with the manual and the panel

A way to find what the manual and the panel leave unclear before a person trips on it (Alex, 2026-11-23: "it's
unclear to me how to move between global settings, how to switch the USB audio and go through poly+br"). A tester
(a person, or a synthetic user: an agent told to know only what is written here and in the manual) gets the tasks
below, the manual (`docs/MANUAL.md`, as the Hub links it) and the FM-1 in the host simulator, and does each task by
pressing, turning and looking at the screen. It never reads the firmware's source: what it cannot find from the
manual and the screen is the finding.

## The panel in the simulator

Build it once: `./build.sh --host` (the binary: `app/build/host/mvane-yard_host`). A script is a text file of
lines, run with `app/build/host/mvane-yard_host SCRIPT.txt OUTDIR` (OUTDIR must exist):

| Line | Does |
| --- | --- |
| `wait MS` | let time pass (start every script with `wait 600`: the boot) |
| `tap NAME` | press and let go of a button (about 80 ms) |
| `press NAME` / `release NAME` | hold a button down / let it go (a layer: press, turn, release) |
| `turn KNOB N` | turn a knob N steps (negative: the other way), 80 ms a step |
| `tapkey w3 90` | tap a white key (w0 F3 ... w15 F5) at a velocity; `tapkey b2 90` a black key (b0 F#3 ... b10 F#5) |
| `key w3 down 90,40` / `key w3 up` | hold a key (velocity, pressure 0..100) / let it go |
| `shot NAME.png` | the screen (240 x 240) as a picture in OUTDIR: look at it |
| `leds` | which buttons and keys are lit, printed |

Buttons: FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+. Knobs (the simulator's names): K1 to K4 (under
the screen), SELECT, ALGO (the panel's ALGORITHM), PRESET (the panel's PRESETS: `turn PRESET 1`). The settings start fresh on each run; keep a task's steps in one
script, and take a shot after each step that should change the screen.

## The tasks

1. Find which page the knobs are on, and switch to the other page and back.
2. Set the play mode to POLY+BREATH (for a wind controller), then back to POLY.
3. Set USB AUDIO so the computer records the FM-1 at full level, not following MASTER.
4. Lower PHONES MAX to 50%.
5. Play a C minor chord progression in CHORDS: the key C, the scale minor; play three chords.
6. Make the chords minor sevenths (a black key), then try two qualities at once.
7. Keep the last phrase as a recording, play it back, then loop it.
8. Overdub one layer on the loop, then undo it.
9. Save the recording into slot 3.
10. Turn the arpeggiator on with LATCH, and change its rate to 1/16.
11. Pick the YARD sound CLAV and lower its LEVEL by 3 dB, then keep the sound.
12. Change the scale to the pitch-class set 1723 (PCO).

## What the tester reports

For each task: done or not; the steps it took (the lines); where the manual was wrong, missing or unclear, and what
the screen showed instead; how many tries. Then the three things that would most help a newcomer. Kept in
`tests/usertest/REPORT-<date>.md` (an agent returns it as text; the session saves it). The first run:
[REPORT-2026-11-23.md](REPORT-2026-11-23.md).
