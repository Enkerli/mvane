# Desktop checks for the portable core (core/): no FM-1 toolchain needed.
CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -Wsign-compare
B = build/desktop
.PHONY: check host fwsc clean
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

host:   # the firmware in the host simulator, and its scripted checks
	./build.sh --host

fwsc:   # the FM-1 package (JieLi toolchain: Linux x86-64, a container, or CI)
	./build.sh

clean:
	rm -rf build app/build
