// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2026 Free Mobile — Vincent Jardin
 */

#ifndef U8G2_LINUX_H
#define U8G2_LINUX_H

#include <stddef.h>
#include <stdint.h>

#include <u8g2/u8g2.h>

struct gpiod_line_request;

struct u8g2_linux {
	int fd;				/* /dev/i2c-N, the display selected */
	struct gpiod_line_request *reset;
	unsigned int reset_offset;
	uint8_t buf[256];		/* one transfer */
	size_t len;
	int err;			/* errno of the first failure, 0 if none */
};

int u8g2_linux_open(struct u8g2_linux *l, const char *i2c, uint8_t addr,
		    const char *reset_line, char *msg, size_t msg_len);
void u8g2_linux_close(struct u8g2_linux *l);

uint8_t u8g2_linux_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_linux_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif
