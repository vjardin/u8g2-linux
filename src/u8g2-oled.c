// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2026 Free Mobile — Vincent Jardin
 */

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "u8g2-linux.h"

#define TEXT_LINES_MAX	5
#define LINE_HEIGHT	12

typedef void (*setup_fn)(u8g2_t *u8g2, const u8g2_cb_t *rotation,
			 u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb);

static const struct {
	const char *name;
	setup_fn setup;
} controllers[] = {
	{ "sh1106", u8g2_Setup_sh1106_i2c_128x64_noname_f },
	{ "ssd1306", u8g2_Setup_ssd1306_i2c_128x64_noname_f },
};

static void usage(FILE *f)
{
	fprintf(f,
		"Usage: u8g2-oled [-d i2c] [-a addr] [-r line] [-c controller] [-n] [text...]\n"
		"Draw a frame and up to %d text lines on a 128x64 I2C OLED.\n"
		"  -d i2c         /dev/i2c-N, or the end of the adapter's device-tree path\n"
		"                 (e.g. i2c-mux@70/i2c@3)\n"
		"  -a addr        7-bit I2C address (default 0x3c)\n"
		"  -r line        GPIO line name of the display's reset (active low),\n"
		"                 pulsed at init (default: none)\n"
		"  -c controller  sh1106 (default) or ssd1306\n"
		"  -n             no I2C: print the frame as text on stdout\n",
		TEXT_LINES_MAX);
}

static void draw(u8g2_t *u8g2, int argc, char **argv, const char *controller)
{
	int i, y = LINE_HEIGHT;

	u8g2_ClearBuffer(u8g2);
	u8g2_DrawFrame(u8g2, 0, 0, u8g2_GetDisplayWidth(u8g2),
		       u8g2_GetDisplayHeight(u8g2));
	u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
	if (!argc) {
		u8g2_DrawStr(u8g2, 4, y, "u8g2-oled");
		u8g2_DrawStr(u8g2, 4, y + LINE_HEIGHT, controller);
		return;
	}
	for (i = 0; i < argc && i < TEXT_LINES_MAX; i++, y += LINE_HEIGHT)
		u8g2_DrawStr(u8g2, 4, y, argv[i]);
}

static void print_buffer(u8g2_t *u8g2)
{
	const uint8_t *buf = u8g2_GetBufferPtr(u8g2);
	int w = u8g2_GetBufferTileWidth(u8g2) * 8;
	int h = u8g2_GetBufferTileHeight(u8g2) * 8;
	int x, y;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			putchar(buf[(y / 8) * w + x] >> (y % 8) & 1 ? '#' : '.');
		putchar('\n');
	}
}

int main(int argc, char **argv)
{
	const char *i2c = NULL, *reset = NULL, *controller = "sh1106";
	unsigned long addr = 0x3c;
	setup_fn setup = NULL;
	struct u8g2_linux l;
	int dry = 0, opt;
	char msg[256];
	u8g2_t u8g2;
	size_t i;
	char *end;

	while ((opt = getopt(argc, argv, "d:a:r:c:nh")) != -1) {
		switch (opt) {
		case 'd':
			i2c = optarg;
			break;
		case 'a':
			addr = strtoul(optarg, &end, 0);
			if (*end || addr > 0x7f) {
				fprintf(stderr, "u8g2-oled: bad address %s\n", optarg);
				return 2;
			}
			break;
		case 'r':
			reset = optarg;
			break;
		case 'c':
			controller = optarg;
			break;
		case 'n':
			dry = 1;
			break;
		case 'h':
			usage(stdout);
			return 0;
		default:
			usage(stderr);
			return 2;
		}
	}
	for (i = 0; i < sizeof(controllers) / sizeof(controllers[0]); i++)
		if (!strcmp(controller, controllers[i].name))
			setup = controllers[i].setup;
	if (!setup) {
		fprintf(stderr, "u8g2-oled: unknown controller %s (sh1106, ssd1306)\n",
			controller);
		return 2;
	}

	if (dry) {
		setup(&u8g2, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb);
		draw(&u8g2, argc - optind, argv + optind, controller);
		print_buffer(&u8g2);
		return 0;
	}

	if (!i2c) {
		fprintf(stderr, "u8g2-oled: -d is needed (or -n)\n");
		return 2;
	}
	if (u8g2_linux_open(&l, i2c, addr, reset, msg, sizeof(msg))) {
		fprintf(stderr, "u8g2-oled: %s\n", msg);
		return 1;
	}
	setup(&u8g2, U8G2_R0, u8g2_linux_byte_i2c, u8g2_linux_gpio_and_delay);
	u8x8_SetUserPtr(u8g2_GetU8x8(&u8g2), &l);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
	draw(&u8g2, argc - optind, argv + optind, controller);
	u8g2_SendBuffer(&u8g2);
	u8g2_linux_close(&l);
	if (l.err) {
		fprintf(stderr, "u8g2-oled: I2C 0x%02lx on %s: %s\n", addr, i2c,
			strerror(l.err));
		return 1;
	}
	return 0;
}
