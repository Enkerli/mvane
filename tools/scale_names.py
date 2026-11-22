#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Names for pitch-class sets by number (music-suite's and Ian Ring's convention: bit k the tonic + k semitones, so
C major is 2741; https://ianring.com/musictheory/scales/ lists all 4096): a small list of the common ones, written
as core/scale_names.h (the FM-1 shows the name when SEL's number is one of them) and web/scale_names.json (the
page's picker). Each entry: a name (12 characters at most, the screen's), its tones in semitones from the tonic.
python3 tools/scale_names.py"""
import json
import os

SCALES = [
    # the diatonic modes and their kin
    ("LYDIAN", [0, 2, 4, 6, 7, 9, 11]), ("MAJOR", [0, 2, 4, 5, 7, 9, 11]), ("MIXOLYDIAN", [0, 2, 4, 5, 7, 9, 10]),
    ("DORIAN", [0, 2, 3, 5, 7, 9, 10]), ("MINOR", [0, 2, 3, 5, 7, 8, 10]), ("PHRYGIAN", [0, 1, 3, 5, 7, 8, 10]),
    ("LOCRIAN", [0, 1, 3, 5, 6, 8, 10]),
    ("MEL MIN", [0, 2, 3, 5, 7, 9, 11]), ("DORIAN B2", [0, 1, 3, 5, 7, 9, 10]), ("LYDIAN AUG", [0, 2, 4, 6, 8, 9, 11]),
    ("LYDIAN DOM", [0, 2, 4, 6, 7, 9, 10]), ("MIXOLYD B6", [0, 2, 4, 5, 7, 8, 10]), ("LOCRIAN #2", [0, 2, 3, 5, 6, 8, 10]),
    ("ALTERED", [0, 1, 3, 4, 6, 8, 10]),
    ("HARM MIN", [0, 2, 3, 5, 7, 8, 11]), ("LOCRIAN #6", [0, 1, 3, 5, 6, 9, 10]), ("IONIAN #5", [0, 2, 4, 5, 8, 9, 11]),
    ("UKR DORIAN", [0, 2, 3, 6, 7, 9, 10]), ("PHRYG DOM", [0, 1, 4, 5, 7, 8, 10]), ("LYDIAN #2", [0, 3, 4, 6, 7, 9, 11]),
    ("ULTRALOCRIAN", [0, 1, 3, 4, 6, 8, 9]),
    ("HARM MAJOR", [0, 2, 4, 5, 7, 8, 11]), ("DOUBLE HARM", [0, 1, 4, 5, 7, 8, 11]), ("HUNGAR MIN", [0, 2, 3, 6, 7, 8, 11]),
    ("HUNGAR MAJ", [0, 3, 4, 6, 7, 9, 10]), ("NEAPOL MAJ", [0, 1, 3, 5, 7, 9, 11]), ("NEAPOL MIN", [0, 1, 3, 5, 7, 8, 11]),
    ("ENIGMATIC", [0, 1, 4, 6, 8, 10, 11]), ("PERSIAN", [0, 1, 4, 5, 6, 8, 11]), ("MAJ LOCRIAN", [0, 2, 4, 5, 6, 8, 10]),
    ("LEAD WHOLE", [0, 2, 4, 6, 8, 10, 11]), ("GYPSY", [0, 2, 3, 6, 7, 8, 10]),
    # pentatonics and hexatonics
    ("MAJ PENTA", [0, 2, 4, 7, 9]), ("SUS PENTA", [0, 2, 5, 7, 10]), ("MIN PENTA", [0, 3, 5, 7, 10]),
    ("YO", [0, 2, 5, 7, 9]), ("DOM PENTA", [0, 2, 4, 7, 10]), ("HIRAJOSHI", [0, 2, 3, 7, 8]), ("IN", [0, 1, 5, 7, 8]),
    ("IWATO", [0, 1, 5, 6, 10]), ("KUMOI", [0, 2, 3, 7, 9]), ("PELOG", [0, 1, 3, 7, 8]),
    ("MAJ BLUES", [0, 2, 3, 4, 7, 9]), ("MIN BLUES", [0, 3, 5, 6, 7, 10]), ("MAJ HEXA", [0, 2, 4, 5, 7, 9]),
    ("MIN HEXA", [0, 2, 3, 5, 7, 10]), ("PROMETHEUS", [0, 2, 4, 6, 9, 10]), ("TRITONE", [0, 1, 4, 6, 7, 10]),
    # bebop
    ("BEBOP DOM", [0, 2, 4, 5, 7, 9, 10, 11]), ("BEBOP MAJ", [0, 2, 4, 5, 7, 8, 9, 11]),
    ("BEBOP DOR", [0, 2, 3, 4, 5, 7, 9, 10]), ("SPANISH 8", [0, 1, 3, 4, 5, 6, 8, 10]),
    # symmetric: Messiaen's modes of limited transposition
    ("WHOLE TONE", [0, 2, 4, 6, 8, 10]), ("OCT H-W", [0, 1, 3, 4, 6, 7, 9, 10]), ("OCT W-H", [0, 2, 3, 5, 6, 8, 9, 11]),
    ("AUGMENTED", [0, 3, 4, 7, 8, 11]), ("MESSIAEN 3", [0, 2, 3, 4, 6, 7, 8, 10, 11]),
    ("MESSIAEN 4", [0, 1, 2, 5, 6, 7, 8, 11]), ("MESSIAEN 5", [0, 1, 5, 6, 7, 11]), ("MESSIAEN 6", [0, 2, 4, 5, 6, 8, 10, 11]),
    ("MESSIAEN 7", [0, 1, 2, 3, 5, 6, 7, 8, 9, 11]),
    # Alex's, and the whole
    ("PCO", [0, 1, 3, 4, 5, 7, 9, 10]), ("NOODLING", [0, 1, 4, 5, 6, 7, 10]), ("CHROMATIC", list(range(12))),
]


def number(tones):
    return sum(1 << t for t in set(tones))


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    seen = {}
    for name, tones in SCALES:
        n = number(tones)
        assert len(name) <= 12 and 0 in tones, name
        assert n not in seen, f"{name} is {seen.get(n)} ({n})"
        seen[n] = name
    with open(os.path.join(root, "core", "scale_names.h"), "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* (made by tools/scale_names.py: edit the list there) "
                "names for pitch-class sets by number, C major 2741 */\n")
        f.write("static const struct {\n    uint16_t pcs;\n    char name[13];\n} SCALE_NAMES[] = {\n")
        for name, tones in SCALES:
            f.write(f'    {{{number(tones)}u, "{name}"}},\n')
        f.write("};\n#define SCALE_NNAMES (sizeof SCALE_NAMES / sizeof SCALE_NAMES[0])\n")
    with open(os.path.join(root, "web", "scale_names.json"), "w") as f:
        json.dump([{"name": n, "pcs": number(t), "tones": sorted(set(t))} for n, t in SCALES], f, indent=0)
        f.write("\n")
    print(f"scale_names: {len(SCALES)} names -> core/scale_names.h, web/scale_names.json")


if __name__ == "__main__":
    main()
