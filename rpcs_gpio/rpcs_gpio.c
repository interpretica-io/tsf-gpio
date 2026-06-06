/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief GPIO RPC server library
 *
 * The gpio_* RPCs (see gpio_rpc.x.m4) on top of ta_gpio.
 * TARPC_FUNC_STATIC() binds an RPC to the function of the same name,
 * so each RPC has a plain C function first and the wrapper after it.
 */

#define TE_LGR_USER     "RPC GPIO"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "rpc_server.h"

#include "ta_gpio.h"

/* Hand a te_string result over to an RPC string field (never NULL). */
static char *
take(te_string *str)
{
    return str->ptr != NULL ? str->ptr : TE_STRDUP("");
}

static te_errno
gpio_list(int *count, char **result)
{
    te_string r = TE_STRING_INIT;
    te_errno rc = ta_gpio_list(count, &r);

    *result = take(&r);
    return rc;
}

TARPC_FUNC_STATIC(gpio_list, {},
{
    int count = 0;

    MAKE_CALL(out->retval = func(&count, &out->result));
    out->count = count;
    out->common.errno_changed = false;
})

static te_errno
gpio_get(const char *chip, unsigned int line, int *value)
{
    return ta_gpio_get(chip, line, value);
}

TARPC_FUNC_STATIC(gpio_get, {},
{
    int value = 0;

    MAKE_CALL(out->retval = func(in->chip, in->line, &value));
    out->value = value;
    out->common.errno_changed = false;
})

static te_errno
gpio_set(const char *chip, unsigned int line, int value)
{
    return ta_gpio_set(chip, line, value);
}

TARPC_FUNC_STATIC(gpio_set, {},
{
    MAKE_CALL(out->retval = func(in->chip, in->line, in->value));
    out->common.errno_changed = false;
})

static te_errno
i2c_transfer(int bus, int addr, const char *wr_hex, int rd_len,
             char **rd_hex)
{
    te_string r = TE_STRING_INIT;
    te_errno rc = ta_i2c_transfer(bus, addr, wr_hex, rd_len, &r);

    *rd_hex = take(&r);
    return rc;
}

TARPC_FUNC_STATIC(i2c_transfer, {},
{
    MAKE_CALL(out->retval = func(in->bus, in->addr, in->wr_hex, in->rd_len,
                                 &out->rd_hex));
    out->common.errno_changed = false;
})

static te_errno
spi_transfer(const char *dev, unsigned int speed_hz, unsigned int mode,
             unsigned int bits, const char *tx_hex, char **rx_hex)
{
    te_string r = TE_STRING_INIT;
    te_errno rc = ta_spi_transfer(dev, speed_hz, mode, bits, tx_hex, &r);

    *rx_hex = take(&r);
    return rc;
}

TARPC_FUNC_STATIC(spi_transfer, {},
{
    MAKE_CALL(out->retval = func(in->dev, in->speed_hz, in->mode, in->bits,
                                 in->tx_hex, &out->rx_hex));
    out->common.errno_changed = false;
})
