/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief GPIO TAPI: RPC client wrappers
 *
 * The rcf_rpc_call() boilerplate behind tapi_gpio. The RPCs return
 * te_errno; an RPC transport failure is mapped to TE_ECORRUPTED.
 */

#define TE_LGR_USER     "TAPI GPIO RPC"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_rpc_internal.h"
#include "tarpc.h"

#include "tapi_gpio_rpc.h"

#define CHECK_RPC_ERRNO_UNCHANGED(_func, _var) \
    CHECK_RETVAL_VAR_ERR_COND(_func, _var, false,                    \
                              TE_RC(TE_TAPI, TE_ECORRUPTED), false)

/* Append an RPC string result, when there is one. */
static void
take_string(te_string *dst, const char *src)
{
    if (dst != NULL && src != NULL)
        te_string_append(dst, "%s", src);
}

/* See description in tapi_gpio_rpc.h */
te_errno
rpc_gpio_list(rcf_rpc_server *rpcs, int *count, te_string *result)
{
    tarpc_gpio_list_in in;
    tarpc_gpio_list_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "gpio_list", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(gpio_list, out.retval);
    TAPI_RPC_LOG(rpcs, gpio_list, "", "%r count=%d", out.retval, out.count);

    if (out.retval == 0)
    {
        if (count != NULL)
            *count = out.count;
        take_string(result, out.result);
    }
    RETVAL_TE_ERRNO(gpio_list, out.retval);
}

/* See description in tapi_gpio_rpc.h */
te_errno
rpc_gpio_get(rcf_rpc_server *rpcs, const char *chip, unsigned int line,
             int *value)
{
    tarpc_gpio_get_in in;
    tarpc_gpio_get_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.chip = (char *)chip;
    in.line = line;

    rcf_rpc_call(rpcs, "gpio_get", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(gpio_get, out.retval);
    TAPI_RPC_LOG(rpcs, gpio_get, "%s line=%u", "%r value=%d",
                 chip != NULL ? chip : "", line, out.retval, out.value);

    if (out.retval == 0 && value != NULL)
        *value = out.value;
    RETVAL_TE_ERRNO(gpio_get, out.retval);
}

/* See description in tapi_gpio_rpc.h */
te_errno
rpc_gpio_set(rcf_rpc_server *rpcs, const char *chip, unsigned int line,
             int value)
{
    tarpc_gpio_set_in in;
    tarpc_gpio_set_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.chip = (char *)chip;
    in.line = line;
    in.value = value;

    rcf_rpc_call(rpcs, "gpio_set", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(gpio_set, out.retval);
    TAPI_RPC_LOG(rpcs, gpio_set, "%s line=%u value=%d", "%r",
                 chip != NULL ? chip : "", line, value, out.retval);

    RETVAL_TE_ERRNO(gpio_set, out.retval);
}

/* See description in tapi_gpio_rpc.h */
te_errno
rpc_i2c_transfer(rcf_rpc_server *rpcs, int bus, int addr, const char *wr_hex,
                 int rd_len, te_string *rd_hex)
{
    tarpc_i2c_transfer_in in;
    tarpc_i2c_transfer_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.bus = bus;
    in.addr = addr;
    in.wr_hex = (char *)(wr_hex != NULL ? wr_hex : "");
    in.rd_len = rd_len;

    rcf_rpc_call(rpcs, "i2c_transfer", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(i2c_transfer, out.retval);
    TAPI_RPC_LOG(rpcs, i2c_transfer, "bus=%d addr=0x%02x rd_len=%d", "%r",
                 bus, addr, rd_len, out.retval);

    if (out.retval == 0)
        take_string(rd_hex, out.rd_hex);
    RETVAL_TE_ERRNO(i2c_transfer, out.retval);
}

/* See description in tapi_gpio_rpc.h */
te_errno
rpc_spi_transfer(rcf_rpc_server *rpcs, const char *dev, unsigned int speed_hz,
                 unsigned int mode, unsigned int bits, const char *tx_hex,
                 te_string *rx_hex)
{
    tarpc_spi_transfer_in in;
    tarpc_spi_transfer_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.dev = (char *)dev;
    in.speed_hz = speed_hz;
    in.mode = mode;
    in.bits = bits;
    in.tx_hex = (char *)(tx_hex != NULL ? tx_hex : "");

    rcf_rpc_call(rpcs, "spi_transfer", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(spi_transfer, out.retval);
    TAPI_RPC_LOG(rpcs, spi_transfer, "%s speed=%u mode=%u", "%r",
                 dev != NULL ? dev : "", speed_hz, mode, out.retval);

    if (out.retval == 0)
        take_string(rx_hex, out.rx_hex);
    RETVAL_TE_ERRNO(spi_transfer, out.retval);
}
