/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Driving GPIO, I2C and SPI on an agent from a test
 *
 * @defgroup tapi_gpio GPIO/I2C/SPI (tapi_gpio)
 * @{
 *
 * Driving the low-level buses of a Test Agent from a test, over
 * libgpiod v2 and the i2c-dev/spidev ioctls in the agent's RPC server:
 * list the GPIO chips, read and drive a line, talk to an I2C device,
 * and run a full-duplex SPI transfer. The agent touches only what a
 * call names - a given chip line, a given I2C address, a given spidev
 * node - and never scans an unknown bus.
 *
 * @code
 * te_vec chips = TE_VEC_INIT(tapi_gpio_chip);
 * const tapi_gpio_chip *chip;
 * int value;
 *
 * CHECK_RC(tapi_gpio_list(rpcs, &chips));
 * TE_VEC_FOREACH(&chips, chip)
 *     RING("%s: %s, %u lines", chip->path, chip->label, chip->num_lines);
 * tapi_gpio_list_free(&chips);
 *
 * CHECK_RC(tapi_gpio_get(rpcs, "/dev/gpiochip0", 17, &value));
 * @endcode
 */

#ifndef __TAPI_GPIO_H__
#define __TAPI_GPIO_H__

#include <stdint.h>
#include <stddef.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "te_vector.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One GPIO chip on the agent. */
typedef struct tapi_gpio_chip {
    /** Character-device path, e.g. @c "/dev/gpiochip0". */
    char *path;
    /** Kernel name, e.g. @c "gpiochip0". */
    char *name;
    /** Driver/consumer label, e.g. @c "pinctrl-bcm2835". */
    char *label;
    /** Number of lines the chip has. */
    unsigned int num_lines;
} tapi_gpio_chip;

/**
 * Snapshot the agent's GPIO chips.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[out] chips    Vector of #tapi_gpio_chip; release with
 *                      tapi_gpio_list_free().
 *
 * @return Status code.
 */
extern te_errno tapi_gpio_list(rcf_rpc_server *rpcs, te_vec *chips);

/**
 * Read one GPIO line as an input.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  chip     Chip path from a listed chip.
 * @param[in]  line     Line offset on the chip.
 * @param[out] value    @c 0 (inactive) or @c 1 (active).
 *
 * @return Status code.
 */
extern te_errno tapi_gpio_get(rcf_rpc_server *rpcs, const char *chip,
                              unsigned int line, int *value);

/**
 * Drive one GPIO line as an output (momentary; see ta_gpio.h).
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  chip     Chip path.
 * @param[in]  line     Line offset.
 * @param[in]  value    @c 0 (inactive) or non-zero (active).
 *
 * @return Status code.
 */
extern te_errno tapi_gpio_set(rcf_rpc_server *rpcs, const char *chip,
                              unsigned int line, int value);

/**
 * One I2C transaction: write @p wr then read into @p rd.
 *
 * A write (when @p wr_len > 0) followed by a read (when @p rd_len > 0)
 * as one I2C_RDWR, so a register read is @p wr the register and @p rd
 * the value.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  bus      Bus number N of @c /dev/i2c-N.
 * @param[in]  addr     7-bit device address.
 * @param[in]  wr       Bytes to write, or @c NULL.
 * @param[in]  wr_len   How many to write.
 * @param[out] rd       Buffer for the bytes read, or @c NULL.
 * @param[in]  rd_len   How many to read (size of @p rd).
 *
 * @return Status code.
 */
extern te_errno tapi_i2c_transfer(rcf_rpc_server *rpcs, int bus, int addr,
                                  const uint8_t *wr, size_t wr_len,
                                  uint8_t *rd, size_t rd_len);

/**
 * One full-duplex SPI transfer on a spidev node.
 *
 * Clocks out @p tx while clocking in @p rx, both @p len bytes.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  dev      spidev path, e.g. @c "/dev/spidev0.0".
 * @param[in]  speed_hz Max clock in Hz, or @c 0 for the default.
 * @param[in]  mode     SPI mode 0..3.
 * @param[in]  bits     Bits per word, or @c 0 for 8.
 * @param[in]  tx       Bytes to clock out.
 * @param[out] rx       Buffer for the bytes clocked in, or @c NULL.
 * @param[in]  len      Transfer length (size of @p tx and @p rx).
 *
 * @return Status code.
 */
extern te_errno tapi_spi_transfer(rcf_rpc_server *rpcs, const char *dev,
                                  unsigned int speed_hz, unsigned int mode,
                                  unsigned int bits, const uint8_t *tx,
                                  uint8_t *rx, size_t len);

/**
 * Find the first chip whose name or label matches.
 *
 * @param chips         A snapshot from tapi_gpio_list().
 * @param needle        A substring to look for in name and label.
 *
 * @return The chip, or @c NULL. Owned by @p chips.
 */
extern const tapi_gpio_chip *tapi_gpio_find(const te_vec *chips,
                                            const char *needle);

/**
 * Release a chip snapshot.
 *
 * @param chips         Vector from tapi_gpio_list().
 */
extern void tapi_gpio_list_free(te_vec *chips);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_GPIO_H__ */

/**@} <!-- END tapi_gpio --> */
