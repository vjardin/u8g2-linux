// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2026 Free Mobile — Vincent Jardin
 */

#define _DEFAULT_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <gpiod.h>

#include "u8g2-linux.h"

static int fail(char *msg, size_t msg_len, int err, const char *fmt, ...)
{
	va_list ap;

	if (msg && msg_len) {
		va_start(ap, fmt);
		vsnprintf(msg, msg_len, fmt, ap);
		va_end(ap);
	}
	errno = err;
	return -1;
}

static uint8_t set_err(struct u8g2_linux *l, int err)
{
	if (!l->err)
		l->err = err;
	return 0;
}

static int i2c_path(const char *spec, char *path, size_t len)
{
	char link[PATH_MAX], node[PATH_MAX];
	size_t sl = strlen(spec), nl;
	struct dirent *de;
	int found = 0;
	DIR *d;

	if (!strncmp(spec, "/dev/", 5)) {
		snprintf(path, len, "%s", spec);
		return 1;
	}
	d = opendir("/sys/bus/i2c/devices");
	if (!d)
		return -1;
	while ((de = readdir(d))) {
		if (strncmp(de->d_name, "i2c-", 4))
			continue;
		snprintf(link, sizeof(link), "/sys/bus/i2c/devices/%s/of_node",
			 de->d_name);
		if (!realpath(link, node))
			continue;
		nl = strlen(node);
		if (nl < sl || strcmp(node + nl - sl, spec))
			continue;
		if (nl > sl && spec[0] != '/' && node[nl - sl - 1] != '/')
			continue;
		snprintf(path, len, "/dev/%s", de->d_name);
		found++;
	}
	closedir(d);
	return found;
}

static struct gpiod_line_request *output_request(struct gpiod_chip *chip,
						 unsigned int offset)
{
	struct gpiod_line_settings *s = gpiod_line_settings_new();
	struct gpiod_line_config *lc = gpiod_line_config_new();
	struct gpiod_request_config *rc = gpiod_request_config_new();
	struct gpiod_line_request *req = NULL;
	int err = ENOMEM;

	if (!s || !lc || !rc)
		goto out;
	gpiod_line_settings_set_direction(s, GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_output_value(s, GPIOD_LINE_VALUE_ACTIVE);
	if (gpiod_line_config_add_line_settings(lc, &offset, 1, s)) {
		err = errno;
		goto out;
	}
	gpiod_request_config_set_consumer(rc, "u8g2-linux");
	req = gpiod_chip_request_lines(chip, rc, lc);
	err = errno;
out:
	gpiod_request_config_free(rc);
	gpiod_line_config_free(lc);
	gpiod_line_settings_free(s);
	errno = err;
	return req;
}

static struct gpiod_line_request *reset_request(const char *name,
						unsigned int *offset)
{
	struct gpiod_line_request *req = NULL;
	char path[PATH_MAX];
	struct gpiod_chip *chip;
	struct dirent *de;
	int err = ENOENT;
	int off;
	DIR *d;

	d = opendir("/dev");
	if (!d)
		return NULL;
	while (!req && (de = readdir(d))) {
		if (strncmp(de->d_name, "gpiochip", 8))
			continue;
		snprintf(path, sizeof(path), "/dev/%s", de->d_name);
		chip = gpiod_chip_open(path);
		if (!chip)
			continue;
		off = gpiod_chip_get_line_offset_from_name(chip, name);
		if (off >= 0) {
			*offset = off;
			req = output_request(chip, off);
			err = errno;
		}
		gpiod_chip_close(chip);
		if (off >= 0)
			break;
	}
	closedir(d);
	errno = err;
	return req;
}

int u8g2_linux_open(struct u8g2_linux *l, const char *i2c, uint8_t addr,
		    const char *reset_line, char *msg, size_t msg_len)
{
	char path[PATH_MAX];
	int n, err;

	memset(l, 0, sizeof(*l));
	l->fd = -1;

	n = i2c_path(i2c, path, sizeof(path));
	if (n < 0)
		return fail(msg, msg_len, errno, "%s: %s", i2c, strerror(errno));
	if (n == 0)
		return fail(msg, msg_len, ENODEV,
			    "%s: no I2C adapter with that device-tree path", i2c);
	if (n > 1)
		return fail(msg, msg_len, EINVAL,
			    "%s: %d I2C adapters match, give more of the path",
			    i2c, n);

	l->fd = open(path, O_RDWR | O_CLOEXEC);
	if (l->fd < 0)
		return fail(msg, msg_len, errno, "%s: %s", path, strerror(errno));
	if (ioctl(l->fd, I2C_SLAVE, addr) < 0) {
		err = errno;
		u8g2_linux_close(l);
		return fail(msg, msg_len, err, "%s: address 0x%02x: %s", path,
			    addr, strerror(err));
	}
	if (!reset_line)
		return 0;

	l->reset = reset_request(reset_line, &l->reset_offset);
	if (!l->reset) {
		err = errno;
		u8g2_linux_close(l);
		return fail(msg, msg_len, err, "GPIO line %s: %s", reset_line,
			    strerror(err));
	}
	return 0;
}

void u8g2_linux_close(struct u8g2_linux *l)
{
	if (l->reset)
		gpiod_line_request_release(l->reset);
	l->reset = NULL;
	if (l->fd >= 0)
		close(l->fd);
	l->fd = -1;
}

uint8_t u8g2_linux_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
			    void *arg_ptr)
{
	struct u8g2_linux *l = u8x8_GetUserPtr(u8x8);
	ssize_t n;

	if (!l)
		return 0;

	switch (msg) {
	case U8X8_MSG_BYTE_INIT:
	case U8X8_MSG_BYTE_SET_DC:
		return 1;
	case U8X8_MSG_BYTE_START_TRANSFER:
		l->len = 0;
		return 1;
	case U8X8_MSG_BYTE_SEND:
		if (l->len + arg_int > sizeof(l->buf))
			return set_err(l, EMSGSIZE);
		memcpy(l->buf + l->len, arg_ptr, arg_int);
		l->len += arg_int;
		return 1;
	case U8X8_MSG_BYTE_END_TRANSFER:
		n = write(l->fd, l->buf, l->len);
		if (n < 0)
			return set_err(l, errno);
		if ((size_t)n != l->len)
			return set_err(l, EIO);
		return 1;
	}
	return 0;
}

static void sleep_ns(long ns)
{
	struct timespec ts = {
		.tv_sec = ns / 1000000000L,
		.tv_nsec = ns % 1000000000L,
	};

	while (nanosleep(&ts, &ts) && errno == EINTR)
		;
}

uint8_t u8g2_linux_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
				  void *arg_ptr)
{
	struct u8g2_linux *l = u8x8_GetUserPtr(u8x8);
	enum gpiod_line_value v;

	(void)arg_ptr;

	switch (msg) {
	case U8X8_MSG_DELAY_NANO:
		sleep_ns(arg_int);
		return 1;
	case U8X8_MSG_DELAY_100NANO:
		sleep_ns(100L * arg_int);
		return 1;
	case U8X8_MSG_DELAY_10MICRO:
		sleep_ns(10000L * arg_int);
		return 1;
	case U8X8_MSG_DELAY_MILLI:
		sleep_ns(1000000L * arg_int);
		return 1;
	case U8X8_MSG_GPIO_RESET:
		if (!l || !l->reset)
			return 1;
		v = arg_int ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
		if (gpiod_line_request_set_value(l->reset, l->reset_offset, v))
			return set_err(l, errno);
		return 1;
	}
	return 1;
}
