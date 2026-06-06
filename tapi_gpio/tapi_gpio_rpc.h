/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief GPIO TAPI: RPC client wrappers
 *
 * Client wrappers of the gpio_* RPCs, see gpio_rpc.x.m4. Tests use
 * tapi_gpio.h; these are the calls behind it, one per RPC. The I2C/SPI
 * payloads cross as lower-case hex strings.
 */

#ifndef __TAPI_GPIO_RPC_H__
#define __TAPI_GPIO_RPC_H__

#include "te_errno.h"
#include "te_string.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * List the agent's GPIO chips (raw record text).
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[out] count    Number of chips, or @c NULL.
 * @param[out] result   The chip lines, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno rpc_gpio_list(rcf_rpc_server *rpcs, int *count,
                              te_string *result);

/**
 * Read one GPIO line as an input.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  chip     Chip path.
 * @param[in]  line     Line offset.
 * @param[out] value    @c 0 or @c 1.
 *
 * @return Status code.
 */
extern te_errno rpc_gpio_get(rcf_rpc_server *rpcs, const char *chip,
                             unsigned int line, int *value);

/**
 * Drive one GPIO line as an output (momentary).
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  chip     Chip path.
 * @param[in]  line     Line offset.
 * @param[in]  value    @c 0 or non-zero.
 *
 * @return Status code.
 */
extern te_errno rpc_gpio_set(rcf_rpc_server *rpcs, const char *chip,
                             unsigned int line, int value);

/**
 * One I2C transaction (write then read), payloads as hex.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  bus      Bus number.
 * @param[in]  addr     7-bit device address.
 * @param[in]  wr_hex   Bytes to write, as hex, or @c NULL.
 * @param[in]  rd_len   Bytes to read back.
 * @param[out] rd_hex   Bytes read, as hex, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno rpc_i2c_transfer(rcf_rpc_server *rpcs, int bus, int addr,
                                 const char *wr_hex, int rd_len,
                                 te_string *rd_hex);

/**
 * One full-duplex SPI transfer, payloads as hex.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  dev      spidev path.
 * @param[in]  speed_hz Max clock in Hz, or @c 0.
 * @param[in]  mode     SPI mode 0..3.
 * @param[in]  bits     Bits per word, or @c 0 for 8.
 * @param[in]  tx_hex   Bytes to clock out, as hex.
 * @param[out] rx_hex   Bytes clocked in, as hex, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno rpc_spi_transfer(rcf_rpc_server *rpcs, const char *dev,
                                 unsigned int speed_hz, unsigned int mode,
                                 unsigned int bits, const char *tx_hex,
                                 te_string *rx_hex);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_GPIO_RPC_H__ */
