/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard: the app's half of the contract with fm1-base (base/device/unity.c, base/host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "MVANE YARD"
#define APP_VERSION "0.5-dev"
#define APP_SPLASH_RGB RGB(120, 200, 170)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */

/* the store: the settings and the panel calibration (the platform's), A/B sector pairs */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* files over USB (fm1.toml companion = true): the settings (a store object), then the take (the data area) */
#define APP_FILE_OBJS {{OBJ_SETTINGS, PLAT_FK_SETTINGS, PLAT_FF_READ | PLAT_FF_WRITE, "settings"}}
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
