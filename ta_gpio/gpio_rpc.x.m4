/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief RPC for GPIO/I2C/SPI access
 *
 * The RPCs of rpcs_gpio, a thin layer over ta_gpio, which drives the
 * agent's GPIO/I2C/SPI in the RPC server process over libgpiod and the
 * i2c-dev/spidev ioctls. Add this file to the rpcxdr definitions of the
 * engine platform and of the agent platform:
 *
 *   TE_LIB_PARMS([rpcxdr], [<platform>], [],
 *                [--with-rpcdefs=tarpc_job.x.m4,../ta_gpio/gpio_rpc.x.m4])
 *
 * No handle survives between calls: each is a whole transaction. Binary
 * I2C/SPI payloads cross as lower-case hex strings; the chip list comes
 * back as newline-separated, tab-separated text - the engine side
 * parses it, the same shape tsf-usb uses.
 */

/* gpio_list(): one line per GPIO chip - path, name, label, num_lines. */
struct tarpc_gpio_list_in {
    struct tarpc_in_arg common;
};

struct tarpc_gpio_list_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
    tarpc_int       count;
    string          result<>;
};

/* gpio_get(): read one line as input; value is 0 or 1. */
struct tarpc_gpio_get_in {
    struct tarpc_in_arg common;

    string          chip<>;
    tarpc_uint      line;
};

struct tarpc_gpio_get_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
    tarpc_int       value;
};

/* gpio_set(): drive one line as output at value (momentary). */
struct tarpc_gpio_set_in {
    struct tarpc_in_arg common;

    string          chip<>;
    tarpc_uint      line;
    tarpc_int       value;
};

struct tarpc_gpio_set_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
};

/*
 * i2c_transfer(): write wr_hex (if any) then read rd_len bytes (if any)
 * to a 7-bit addr on /dev/i2c-<bus>; rd_hex is the bytes read, as hex.
 */
struct tarpc_i2c_transfer_in {
    struct tarpc_in_arg common;

    tarpc_int       bus;
    tarpc_int       addr;
    string          wr_hex<>;
    tarpc_int       rd_len;
};

struct tarpc_i2c_transfer_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
    string          rd_hex<>;
};

/*
 * spi_transfer(): full-duplex transfer on a spidev node; rx_hex is the
 * bytes clocked in, as hex, the same length as tx_hex.
 */
struct tarpc_spi_transfer_in {
    struct tarpc_in_arg common;

    string          dev<>;
    tarpc_uint      speed_hz;
    tarpc_uint      mode;
    tarpc_uint      bits;
    string          tx_hex<>;
};

struct tarpc_spi_transfer_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
    string          rx_hex<>;
};

program gpio
{
    version ver0
    {
        RPC_DEF(gpio_list)
        RPC_DEF(gpio_get)
        RPC_DEF(gpio_set)
        RPC_DEF(i2c_transfer)
        RPC_DEF(spi_transfer)
    } = 1;
} = 27;
