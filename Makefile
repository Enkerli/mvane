# Desktop checks for the portable core (core/): no FM-1 toolchain needed.
CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -Wsign-compare
B = build/desktop
.PHONY: loud check host fwsc clean clp-cost synth-cmp clp-expr clp-split clp-race clp-bank yard-race yard-render clp-trim clp-names
check: $(B)/harmony_demo $(B)/rec_test $(B)/chords_test
	$(B)/harmony_demo
	$(B)/rec_test
	$(B)/chords_test

$(B)/harmony_demo: core/harmony.c core/harmony.h core/demo.c
	@mkdir -p $(B)
	$(CC) $(CFLAGS) core/harmony.c core/demo.c -o $@

$(B)/rec_test: core/rec.c core/rec.h core/rec_test.c
	@mkdir -p $(B)
	$(CC) $(CFLAGS) core/rec.c core/rec_test.c -o $@

$(B)/chords_test: core/chords.c core/chords.h core/port.h core/chords_test.c
	@mkdir -p $(B)
	$(CC) $(CFLAGS) core/chords.c core/chords_test.c -o $@

clp-cost:   # each Clipper sound's cost tier, EZ or HRD, as the FM-1's two cores play a chord (app/clipper/gen/clp_cost.h)
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_cost.c -o $(B)/clp_cost -lm
	$(B)/clp_cost app/clipper/gen/clp_cost.h > $(B)/clp_cost.txt
	tail -22 $(B)/clp_cost.txt

host:   # the firmware in the host simulator, and its scripted checks
	./build.sh --host

fwsc:   # the FM-1 package (JieLi toolchain: Linux x86-64, a container, or CI)
	./build.sh

VIBE_SDK ?= ../vibe-fm-sdk-ref/sdk
.PHONY: vibe vibe-cmp                # (vibe is also the folder: always run)
vibe:   # the native VIBE FM firmware (vibe/mvane-yard-native: the Hub's SDK, Zig): build/mvane-yard-native.zip in the SDK, then its check
	sh vibe/build.sh $(VIBE_SDK)
	cd $(VIBE_SDK) && node tools/vibe-check.mjs build/mvane-yard-native.wasm | tail -1

vibe-cmp: vibe   # the port's YARD against the FM-1's, sample for sample (44.1 kHz): should be bit for bit
	@mkdir -p $(B)/vibe
	cd $(VIBE_SDK) && node tools/wasmtest.mjs build/mvane-yard-native.wasm $(CURDIR)/vibe/cmp.json $(CURDIR)/$(B)/vibe 44100 >/dev/null
	$(CC) -O2 -ffp-contract=off -std=c99 -w -Ibase/include tools/vibe_cmp.c -o $(B)/vibe_cmp -lm
	python3 tools/vibe_events.py vibe/cmp.json | $(B)/vibe_cmp $(B)/vibe/native.raw 517
	python3 tools/vibe_cmp.py $(B)/vibe/audio.wav $(B)/vibe/native.raw

loud:   # every sound's loudness, Clipper's (trims on) and YARD's, for the same ordinary phrase (tools/loud_phrase.h)
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_loud.c -o $(B)/clp_loud -lm
	$(CC) -O2 -std=c99 -w -Ibase/include tools/yard_loud.c -o $(B)/yard_loud -lm
	$(B)/clp_loud > $(B)/clp_loud.txt
	$(B)/yard_loud > $(B)/yard_loud.txt
	cat $(B)/clp_loud.txt $(B)/yard_loud.txt | awk '{print $$NF, $$0}' | sort -n | cut -d' ' -f2- | sed -n '1,5p;$$p'   # the quietest five, the loudest

onset-cal:   # ONSET's levels (app/synth.c's table): each burst's mean peak, the gain to 0.85
	@mkdir -p $(B)
	$(CC) -O2 -std=c99 -w -Ibase/include tools/onset_cal.c -o $(B)/onset_cal -lm
	$(B)/onset_cal

scale-names:   # the known scales' names, for the screen and the page (core/scale_names.h, web/scale_names.json)
	python3 tools/scale_names.py

clean:
	rm -rf build app/build

synth-cmp:   # the saw's filter tables against the exact coefficients: the output's difference and the CPU
	@mkdir -p $(B)
	$(CC) -O2 -std=c99 -Wall -Wextra -Werror -Ibase/include tools/synth_cmp.c -o $(B)/synth_cmp_tab -lm
	$(CC) -O2 -std=c99 -Wall -Wextra -Werror -Ibase/include -DSYNTH_EXACT tools/synth_cmp.c -o $(B)/synth_cmp_exact -lm
	$(B)/synth_cmp_tab $(B)/saw_tab.raw
	$(B)/synth_cmp_exact $(B)/saw_exact.raw
	python3 tools/synth_cmp.py $(B)/saw_tab.raw $(B)/saw_exact.raw 35

clp-expr:   # how each Clipper sound answers pressure and slide (build/desktop/clp_expr.tsv)
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal app/clipper/clp.c tools/clp_expr.c -o $(B)/clp_expr -lm
	$(B)/clp_expr $(B)/clp_expr.tsv app/clipper/gen/clp_expr.h
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal app/clipper/clp.c tools/clp_expr.c -o $(B)/clp_expr -lm
	$(B)/clp_expr $(B)/clp_expr.tsv $(B)/clp_expr_again.h check

clp-split:   # the expression slots must leave each Clipper sound's voices split between the cores
	@mkdir -p $(B)
	$(CC) -O1 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_split.c -o $(B)/clp_split -lm
	$(B)/clp_split

ENGINES ?= SAMPLE,WHEEL
clp-race:   # Clipper's voices on two threads under ThreadSanitizer, bit for bit against one core
	@mkdir -p $(B)
	$(CC) -O1 -g -std=gnu99 -w -fsanitize=thread -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_race.c -o $(B)/clp_race -lm -lpthread
	TSAN_OPTIONS=halt_on_error=1 $(B)/clp_race $(ENGINES)

clp-bank:   # each Clipper sound's family by how it behaves (app/clipper/gen/clp_bank.h)
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal -Iapp -Ibase/include app/clipper/clp.c app/synth.c tools/clp_bank.c -o $(B)/clp_bank -lm
	$(B)/clp_bank app/clipper/gen/clp_bank.h > $(B)/clp_bank.txt
	tail -5 $(B)/clp_bank.txt
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal -Iapp -Ibase/include app/clipper/clp.c app/synth.c tools/clp_bank.c -o $(B)/clp_bank -lm
	$(B)/clp_bank docs/SOUNDS.md list

yard-race:   # YARD's voices on two threads under ThreadSanitizer, bit for bit against one core
	@mkdir -p $(B)
	$(CC) -O1 -g -std=gnu99 -Wall -Wextra -Werror -fsanitize=thread -Ibase/include tools/yard_render.c -o $(B)/yard_race -lm -lpthread
	TSAN_OPTIONS=halt_on_error=1 $(B)/yard_race race

yard-render:   # YARD's sounds to listen to (build/desktop/yard_sounds.wav), and their CPU here
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -Wall -Wextra -Werror -Ibase/include tools/yard_render.c -o $(B)/yard_render -lm -lpthread
	$(B)/yard_render $(B)/yard_sounds.wav

clp-trim:   # each Clipper sound's trim: a chord of six under the master limiter (app/clipper/gen/clp_trim.h)
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_trim.c -o $(B)/clp_trim -lm
	$(B)/clp_trim app/clipper/gen/clp_trim.h
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_trim.c -o $(B)/clp_trim -lm
	$(B)/clp_trim $(B)/clp_trim_again.h check

clp-names:   # Clipper's names for the computer's side of the user's Clipper sounds (web/clp_names.json)
	@mkdir -p $(B)
	$(CC) -O1 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal tools/clp_names.c -o $(B)/clp_names -lm
	$(B)/clp_names web/clp_names.json
