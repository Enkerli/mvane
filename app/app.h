/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard: the app's half of the contract with fm1-base (base/device/unity.c, base/host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "MVANE YARD"
#if __has_include("build_id.h")         /* (CI writes it: 0.11.N+COMMIT, N the commits since the last release,
                                          * so builds count up; a release: 0.11 .. 0.99, two digits a part: fm1-base's
                                          * identity, tools/build.py) */
#include "build_id.h"
#endif
#ifndef APP_VERSION                      /* (a release build gives its own: tools/build.py --release) */
#define APP_VERSION "0.11-dev"
#endif
#define APP_SPLASH_RGB RGB(120, 200, 170)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
/* MASTER's curve (fm1-base: its position 0..4096 to the render's gain, the square by default), times PHONES MAX
 * (0.11, Alex: the headphones clip on the FM-1's analog side at a level the digital signal has clean); with USB
 * AUDIO FULL it is the speaker's and the headphones' only. Called from the audio interrupt: reads one value */
uint32_t app_gain(uint32_t pos);
#define APP_GAIN(m) app_gain(m)
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_USB_HOST_NAME                /* the USB host's device by name (plat_usb_host_name): the Diosynth */

/* the store: the settings and the panel calibration (the platform's), A/B sector pairs */
/* (0.10) OBJ_YARD, the user's YARD sounds, in the data area's last two sectors (after the takes and the panel's) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_YARD, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}

/* files over USB (fm1.toml companion = true): the settings (a store object), then the take (the data area) */
#define APP_FILE_OBJS {{OBJ_SETTINGS, PLAT_FK_SETTINGS, PLAT_FF_READ | PLAT_FF_WRITE, "settings"}, \
                       {OBJ_YARD, PLAT_FK_BANK, PLAT_FF_READ | PLAT_FF_WRITE, "yard.ysb"}}
#define APP_FILES
int app_file(uint32_t i, plat_file_info_t *f);
int app_file_read(uint32_t i, uint32_t off, uint8_t *dst, uint32_t n);
int app_file_begin(uint32_t i, uint32_t size);
int app_file_write(uint32_t i, uint32_t off, const uint8_t *src, uint32_t n);
int app_file_commit(uint32_t i, uint32_t size, uint32_t crc);
int app_file_delete(uint32_t i);
int app_file_busy(void);                 /* 1: no file written now (a take playing) */
void app_file_changed(uint32_t obj);     /* the computer wrote obj: load it again */

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's checks (base/host/plat_host.c "expect") */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
/* the app's own script commands: "flood N" sends the render N pressure events at once (the queue overflows) */
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
