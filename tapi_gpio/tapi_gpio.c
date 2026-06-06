/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Driving GPIO/I2C/SPI on an agent from a test
 *
 * The engine-side layer over the gpio_* RPCs: it parses the chip list
 * (newline/tab text) into a vector of #tapi_gpio_chip, and converts the
 * I2C/SPI byte buffers to and from the hex the RPC carries.
 */

#define TE_LGR_USER     "TAPI GPIO"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "te_vector.h"
#include "logger_api.h"

#include "tapi_gpio.h"
#include "tapi_gpio_rpc.h"

/** The number of tab-separated fields in a gpio_list() line. */
#define TAPI_GPIO_FIELDS 4

/** Append @p len bytes of @p buf to @p dest as lower-case hex. */
static void
gpio_bin2hex(const uint8_t *buf, size_t len, te_string *dest)
{
    size_t i;

    for (i = 0; i < len; i++)
        te_string_append(dest, "%02x", buf[i]);
}

/** One hex nibble to its value, or -1. */
static int
gpio_hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/** Parse a hex string into @p buf (up to @p buf_len); @p *out_len set. */
static te_errno
gpio_hex2bin(const char *hex, uint8_t *buf, size_t buf_len, size_t *out_len)
{
    size_t n = 0;

    *out_len = 0;
    for (; hex != NULL && hex[0] != '\0' && hex[1] != '\0'; hex += 2)
    {
        int hi = gpio_hex_nibble(hex[0]);
        int lo = gpio_hex_nibble(hex[1]);

        if (hi < 0 || lo < 0)
            return TE_RC(TE_TAPI, TE_EINVAL);
        if (n >= buf_len)
            return TE_RC(TE_TAPI, TE_E2BIG);
        buf[n++] = (uint8_t)((hi << 4) | lo);
    }

    *out_len = n;
    return 0;
}

/** Parse one gpio_list() line into a chip; false on a short line. */
static bool
gpio_parse_chip(const char *line, size_t len, tapi_gpio_chip *chip)
{
    const char *p = line;
    const char *end = line + len;
    char *f[TAPI_GPIO_FIELDS];
    size_t n = 0;
    bool ok = false;
    size_t i;

    for (i = 0; i < TAPI_GPIO_FIELDS; i++)
        f[i] = NULL;

    for (i = 0; i < TAPI_GPIO_FIELDS && p <= end; i++)
    {
        const char *tab = memchr(p, '\t', (size_t)(end - p));
        size_t flen = (tab != NULL && tab < end) ? (size_t)(tab - p) :
                      (size_t)(end - p);

        f[i] = TE_STRNDUP(p, flen);
        n++;
        if (tab == NULL || tab >= end)
            break;
        p = tab + 1;
    }

    if (n < TAPI_GPIO_FIELDS)
        goto out;

    memset(chip, 0, sizeof(*chip));
    chip->path = TE_STRDUP(f[0]);
    chip->name = TE_STRDUP(f[1]);
    chip->label = TE_STRDUP(f[2]);
    chip->num_lines = (unsigned int)strtoul(f[3], NULL, 10);
    ok = true;

out:
    for (i = 0; i < TAPI_GPIO_FIELDS; i++)
        free(f[i]);
    return ok;
}

/* See description in tapi_gpio.h */
te_errno
tapi_gpio_list(rcf_rpc_server *rpcs, te_vec *chips)
{
    te_string raw = TE_STRING_INIT;
    const char *line;
    te_errno rc;

    *chips = (te_vec)TE_VEC_INIT(tapi_gpio_chip);

    rc = rpc_gpio_list(rpcs, NULL, &raw);
    if (rc != 0)
    {
        te_string_free(&raw);
        return rc;
    }

    line = te_string_value(&raw);
    while (line != NULL && *line != '\0')
    {
        const char *nl = strchr(line, '\n');
        size_t len = nl != NULL ? (size_t)(nl - line) : strlen(line);
        tapi_gpio_chip chip;

        if (len != 0 && gpio_parse_chip(line, len, &chip))
            TE_VEC_APPEND(chips, chip);

        line = nl != NULL ? nl + 1 : NULL;
    }

    te_string_free(&raw);

    return 0;
}

/* See description in tapi_gpio.h */
te_errno
tapi_gpio_get(rcf_rpc_server *rpcs, const char *chip, unsigned int line,
              int *value)
{
    return rpc_gpio_get(rpcs, chip, line, value);
}

/* See description in tapi_gpio.h */
te_errno
tapi_gpio_set(rcf_rpc_server *rpcs, const char *chip, unsigned int line,
              int value)
{
    return rpc_gpio_set(rpcs, chip, line, value);
}

/* See description in tapi_gpio.h */
te_errno
tapi_i2c_transfer(rcf_rpc_server *rpcs, int bus, int addr, const uint8_t *wr,
                  size_t wr_len, uint8_t *rd, size_t rd_len)
{
    te_string wr_hex = TE_STRING_INIT;
    te_string rd_hex = TE_STRING_INIT;
    size_t got = 0;
    te_errno rc;

    gpio_bin2hex(wr, wr_len, &wr_hex);

    rc = rpc_i2c_transfer(rpcs, bus, addr, te_string_value(&wr_hex),
                          (int)rd_len, &rd_hex);
    if (rc == 0 && rd != NULL && rd_len != 0)
        rc = gpio_hex2bin(te_string_value(&rd_hex), rd, rd_len, &got);

    te_string_free(&wr_hex);
    te_string_free(&rd_hex);

    return rc;
}

/* See description in tapi_gpio.h */
te_errno
tapi_spi_transfer(rcf_rpc_server *rpcs, const char *dev, unsigned int speed_hz,
                  unsigned int mode, unsigned int bits, const uint8_t *tx,
                  uint8_t *rx, size_t len)
{
    te_string tx_hex = TE_STRING_INIT;
    te_string rx_hex = TE_STRING_INIT;
    size_t got = 0;
    te_errno rc;

    gpio_bin2hex(tx, len, &tx_hex);

    rc = rpc_spi_transfer(rpcs, dev, speed_hz, mode, bits,
                          te_string_value(&tx_hex), &rx_hex);
    if (rc == 0 && rx != NULL && len != 0)
        rc = gpio_hex2bin(te_string_value(&rx_hex), rx, len, &got);

    te_string_free(&tx_hex);
    te_string_free(&rx_hex);

    return rc;
}

/* See description in tapi_gpio.h */
const tapi_gpio_chip *
tapi_gpio_find(const te_vec *chips, const char *needle)
{
    const tapi_gpio_chip *chip;

    TE_VEC_FOREACH((te_vec *)chips, chip)
    {
        if ((chip->name != NULL && strstr(chip->name, needle) != NULL) ||
            (chip->label != NULL && strstr(chip->label, needle) != NULL))
            return chip;
    }

    return NULL;
}

/* See description in tapi_gpio.h */
void
tapi_gpio_list_free(te_vec *chips)
{
    tapi_gpio_chip *chip;

    TE_VEC_FOREACH(chips, chip)
    {
        free(chip->path);
        free(chip->name);
        free(chip->label);
    }
    te_vec_free(chips);
}
