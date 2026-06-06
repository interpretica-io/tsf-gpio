/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Agent-side GPIO, I2C and SPI access
 *
 * Driving the low-level buses of an agent over their kernel interfaces:
 * GPIO over **libgpiod v2** (@c gpiod.h, @c -lgpiod), I2C over
 * @c /dev/i2c-N with the @c linux/i2c-dev.h ioctls, and SPI over
 * @c /dev/spidevB.C with the @c linux/spi/spidev.h ioctls. The library
 * and the character devices are used directly in-process; nothing is
 * spawned and no @c gpioget / @c i2cget / @c spidev tool is scraped.
 * The agent and its RPC server both link this; the RPCs (see
 * gpio_rpc.x.m4) are thin wrappers over these functions.
 *
 * It touches only what a call names - a given chip line, a given I2C
 * address, a given spidev node. It never scans an unknown bus for
 * devices (an I2C probe can wedge a device that misreads it), so the
 * caller says exactly what to talk to.
 *
 * Binary payloads cross as lower-case hex strings (two chars per byte,
 * no separator), to stay in the newline/tab record convention the
 * engine side parses. Lists come back one record per line with
 * tab-separated fields, the same shape tsf-usb uses.
 */

#ifndef __TA_GPIO_H__
#define __TA_GPIO_H__

#include "te_errno.h"
#include "te_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * List the GPIO chips on the agent.
 *
 * One chip per line, tab-separated:
 * @c "path\\tname\\tlabel\\tnum_lines", e.g.
 * @c "/dev/gpiochip0\tgpiochip0\tpinctrl-bcm2835\t54". Reads each
 * @c /dev/gpiochip* through libgpiod; a node it cannot open is skipped.
 *
 * @param[out] count    Number of chips.
 * @param[out] result   The chip lines.
 *
 * @return Status code.
 */
extern te_errno ta_gpio_list(int *count, te_string *result);

/**
 * Read one GPIO line as an input.
 *
 * Requests @p line on @p chip as an input, reads it, and releases it.
 *
 * @param[in]  chip     Chip path, e.g. @c "/dev/gpiochip0".
 * @param[in]  line     Line offset on the chip.
 * @param[out] value    @c 0 (inactive) or @c 1 (active).
 *
 * @return Status code.
 */
extern te_errno ta_gpio_get(const char *chip, unsigned int line, int *value);

/**
 * Drive one GPIO line as an output.
 *
 * Requests @p line on @p chip as an output at @p value. The line holds
 * that value for the duration of the request; this call releases the
 * request before returning, so the drive is momentary - a persistent
 * hold needs a stateful API this does not yet have.
 *
 * @param[in]  chip     Chip path.
 * @param[in]  line     Line offset.
 * @param[in]  value    @c 0 (inactive) or non-zero (active).
 *
 * @return Status code.
 */
extern te_errno ta_gpio_set(const char *chip, unsigned int line, int value);

/**
 * One I2C transaction on @c /dev/i2c-<bus> to a 7-bit address.
 *
 * Does a write of @p wr_hex (when non-empty) and then a read of
 * @p rd_len bytes (when > 0) as one I2C_RDWR transaction, so a
 * register read is a write of the register followed by a read with a
 * repeated start. Either half may be absent: a pure write (rd_len 0),
 * a pure read (empty @p wr_hex).
 *
 * @param[in]  bus      Bus number N of @c /dev/i2c-N.
 * @param[in]  addr     7-bit device address.
 * @param[in]  wr_hex   Bytes to write, as hex, or @c "" / @c NULL.
 * @param[in]  rd_len   Bytes to read back, or @c 0.
 * @param[out] rd_hex   The bytes read, as hex.
 *
 * @return Status code.
 */
extern te_errno ta_i2c_transfer(int bus, int addr, const char *wr_hex,
                                int rd_len, te_string *rd_hex);

/**
 * One full-duplex SPI transfer on a spidev node.
 *
 * Sets the mode, word size and clock, then clocks out @p tx_hex while
 * clocking in the same number of bytes - a single SPI_IOC_MESSAGE. The
 * read buffer is as long as @p tx_hex.
 *
 * @param[in]  dev      spidev path, e.g. @c "/dev/spidev0.0".
 * @param[in]  speed_hz Max clock in Hz, or @c 0 for the driver default.
 * @param[in]  mode     SPI mode 0..3.
 * @param[in]  bits     Bits per word, or @c 0 for 8.
 * @param[in]  tx_hex   Bytes to clock out, as hex.
 * @param[out] rx_hex   The bytes clocked in, as hex (same length).
 *
 * @return Status code.
 */
extern te_errno ta_spi_transfer(const char *dev, unsigned int speed_hz,
                                unsigned int mode, unsigned int bits,
                                const char *tx_hex, te_string *rx_hex);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_GPIO_H__ */
