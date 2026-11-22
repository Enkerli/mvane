# Desktop checks for the portable core (core/): no FM-1 toolchain needed.
CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -Wsign-compare
B = build/desktop
.PHONY: check host fwsc clean clp-cost synth-cmp
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

clp-cost:   # each Clipper sound's cost tier (app/clipper/gen/clp_cost.h), measured here
	@mkdir -p $(B)
	$(CC) -O2 -std=gnu99 -w -DPLAT_HOST -Iapp/clipper -Iapp/clipper/src -Iapp/clipper/gen -Iapp/clipper/hal app/clipper/clp.c tools/clp_cost.c -o $(B)/clp_cost -lm
	$(B)/clp_cost app/clipper/gen/clp_cost.h

host:   # the firmware in the host simulator, and its scripted checks
	./build.sh --host

fwsc:   # the FM-1 package (JieLi toolchain: Linux x86-64, a container, or CI)
	./build.sh

clean:
	rm -rf build app/build

synth-cmp:   # the saw's filter tables against the exact coefficients: the output's difference and the CPU
	@mkdir -p $(B)
	$(CC) -O2 -std=c99 -Wall -Wextra -Werror -Ibase/include tools/synth_cmp.c -o $(B)/synth_cmp_tab -lm
	$(CC) -O2 -std=c99 -Wall -Wextra -Werror -Ibase/include -DSYNTH_EXACT tools/synth_cmp.c -o $(B)/synth_cmp_exact -lm
	$(B)/synth_cmp_tab $(B)/saw_tab.raw
	$(B)/synth_cmp_exact $(B)/saw_exact.raw
	python3 tools/synth_cmp.py $(B)/saw_tab.raw $(B)/saw_exact.raw 35
