/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Agent-side GPIO/I2C/SPI access
 *
 * Written against the libgpiod v2 API (gpiod_chip_open,
 * gpiod_line_settings_*, gpiod_line_config_*, gpiod_chip_request_lines,
 * gpiod_line_request_*) and the Linux i2c-dev / spidev ioctls. v1 of
 * libgpiod has a different, incompatible API; this targets v2. Each
 * call opens its device, does its one transaction, and closes it, so
 * nothing has to survive between calls.
 */

#define TE_LGR_USER     "TA GPIO"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>

#include <gpiod.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"

#include "ta_gpio.h"

/** Largest I2C/SPI payload a single transfer will move. */
#define TA_GPIO_MAX_XFER 4096

/** One hex nibble to its value, or -1. */
static int
hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/** Parse a hex string into @p buf; @p *out_len gets the byte count. */
static te_errno
hex2bin(const char *hex, uint8_t *buf, size_t buf_len, size_t *out_len)
{
    size_t n = 0;

    *out_len = 0;
    if (hex == NULL)
        return 0;

    for (; hex[0] != '\0' && hex[1] != '\0'; hex += 2)
    {
        int hi = hex_nibble(hex[0]);
        int lo = hex_nibble(hex[1]);

        if (hi < 0 || lo < 0)
            return TE_RC(TE_TA_UNIX, TE_EINVAL);
        if (n >= buf_len)
            return TE_RC(TE_TA_UNIX, TE_E2BIG);
        buf[n++] = (uint8_t)((hi << 4) | lo);
    }
    if (hex[0] != '\0')     /* an odd trailing nibble */
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    *out_len = n;
    return 0;
}

/** Append @p len bytes of @p buf to @p dest as lower-case hex. */
static void
bin2hex(const uint8_t *buf, size_t len, te_string *dest)
{
    size_t i;

    for (i = 0; i < len; i++)
        te_string_append(dest, "%02x", buf[i]);
}

/* See description in ta_gpio.h */
te_errno
ta_gpio_list(int *count, te_string *result)
{
    DIR *dir;
    struct dirent *entry;

    *count = 0;

    dir = opendir("/dev");
    if (dir == NULL)
    {
        ERROR("Cannot open /dev: %s", strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        char path[320];
        struct gpiod_chip *chip;
        struct gpiod_chip_info *info;

        if (strncmp(entry->d_name, "gpiochip", 8) != 0)
            continue;

        TE_SPRINTF(path, "/dev/%s", entry->d_name);
        chip = gpiod_chip_open(path);
        if (chip == NULL)
            continue;

        info = gpiod_chip_get_info(chip);
        if (info != NULL)
        {
            te_string_append(result, "%s\t%s\t%s\t%zu\n", path,
                             gpiod_chip_info_get_name(info),
                             gpiod_chip_info_get_label(info),
                             gpiod_chip_info_get_num_lines(info));
            gpiod_chip_info_free(info);
            (*count)++;
        }
        gpiod_chip_close(chip);
    }

    closedir(dir);

    return 0;
}

/**
 * Request one line on @p chip with @p settings and hand back the
 * request and the chip (both to be released by the caller).
 */
static te_errno
gpio_request_one(const char *chip, unsigned int line,
                 struct gpiod_line_settings *settings,
                 struct gpiod_chip **chip_out,
                 struct gpiod_line_request **req_out)
{
    struct gpiod_chip *handle;
    struct gpiod_line_config *line_cfg;
    struct gpiod_line_request *request;
    unsigned int offset = line;

    *chip_out = NULL;
    *req_out = NULL;

    handle = gpiod_chip_open(chip);
    if (handle == NULL)
    {
        ERROR("Cannot open GPIO chip %s: %s", chip, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    line_cfg = gpiod_line_config_new();
    if (line_cfg == NULL ||
        gpiod_line_config_add_line_settings(line_cfg, &offset, 1,
                                            settings) != 0)
    {
        if (line_cfg != NULL)
            gpiod_line_config_free(line_cfg);
        gpiod_chip_close(handle);
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    request = gpiod_chip_request_lines(handle, NULL, line_cfg);
    gpiod_line_config_free(line_cfg);
    if (request == NULL)
    {
        ERROR("Cannot request line %u on %s: %s", line, chip,
              strerror(errno));
        gpiod_chip_close(handle);
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    *chip_out = handle;
    *req_out = request;

    return 0;
}

/* See description in ta_gpio.h */
te_errno
ta_gpio_get(const char *chip, unsigned int line, int *value)
{
    struct gpiod_line_settings *settings;
    struct gpiod_chip *handle;
    struct gpiod_line_request *request;
    enum gpiod_line_value v;
    te_errno rc;

    settings = gpiod_line_settings_new();
    if (settings == NULL)
        return TE_OS_RC(TE_TA_UNIX, errno);
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    rc = gpio_request_one(chip, line, settings, &handle, &request);
    gpiod_line_settings_free(settings);
    if (rc != 0)
        return rc;

    v = gpiod_line_request_get_value(request, line);
    if (v == GPIOD_LINE_VALUE_ERROR)
        rc = TE_OS_RC(TE_TA_UNIX, errno);
    else
        *value = (v == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;

    gpiod_line_request_release(request);
    gpiod_chip_close(handle);

    return rc;
}

/* See description in ta_gpio.h */
te_errno
ta_gpio_set(const char *chip, unsigned int line, int value)
{
    struct gpiod_line_settings *settings;
    struct gpiod_chip *handle;
    struct gpiod_line_request *request;
    te_errno rc;

    settings = gpiod_line_settings_new();
    if (settings == NULL)
        return TE_OS_RC(TE_TA_UNIX, errno);
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings,
        value != 0 ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);

    rc = gpio_request_one(chip, line, settings, &handle, &request);
    gpiod_line_settings_free(settings);
    if (rc != 0)
        return rc;

    /* The value is driven by the request itself; release ends it. */
    gpiod_line_request_release(request);
    gpiod_chip_close(handle);

    return 0;
}

/* See description in ta_gpio.h */
te_errno
ta_i2c_transfer(int bus, int addr, const char *wr_hex, int rd_len,
                te_string *rd_hex)
{
    char path[64];
    uint8_t wr[TA_GPIO_MAX_XFER];
    uint8_t rd[TA_GPIO_MAX_XFER];
    size_t wr_len = 0;
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data xfer;
    unsigned int n_msgs = 0;
    int fd;
    te_errno rc;

    if (rd_len < 0 || rd_len > TA_GPIO_MAX_XFER)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    rc = hex2bin(wr_hex, wr, sizeof(wr), &wr_len);
    if (rc != 0)
        return rc;
    if (wr_len == 0 && rd_len == 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    TE_SPRINTF(path, "/dev/i2c-%d", bus);
    fd = open(path, O_RDWR);
    if (fd < 0)
    {
        ERROR("Cannot open %s: %s", path, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    if (wr_len != 0)
    {
        msgs[n_msgs].addr = (uint16_t)addr;
        msgs[n_msgs].flags = 0;
        msgs[n_msgs].len = (uint16_t)wr_len;
        msgs[n_msgs].buf = wr;
        n_msgs++;
    }
    if (rd_len != 0)
    {
        msgs[n_msgs].addr = (uint16_t)addr;
        msgs[n_msgs].flags = I2C_M_RD;
        msgs[n_msgs].len = (uint16_t)rd_len;
        msgs[n_msgs].buf = rd;
        n_msgs++;
    }
    xfer.msgs = msgs;
    xfer.nmsgs = n_msgs;

    if (ioctl(fd, I2C_RDWR, &xfer) < 0)
    {
        ERROR("I2C_RDWR on %s addr 0x%02x failed: %s", path, addr,
              strerror(errno));
        rc = TE_OS_RC(TE_TA_UNIX, errno);
    }
    else if (rd_len != 0)
    {
        bin2hex(rd, (size_t)rd_len, rd_hex);
    }

    close(fd);

    return rc;
}

/* See description in ta_gpio.h */
te_errno
ta_spi_transfer(const char *dev, unsigned int speed_hz, unsigned int mode,
                unsigned int bits, const char *tx_hex, te_string *rx_hex)
{
    uint8_t tx[TA_GPIO_MAX_XFER];
    uint8_t rx[TA_GPIO_MAX_XFER];
    size_t len = 0;
    uint8_t spi_mode = (uint8_t)mode;
    uint8_t spi_bits = (uint8_t)(bits != 0 ? bits : 8);
    struct spi_ioc_transfer xfer;
    int fd;
    te_errno rc;

    rc = hex2bin(tx_hex, tx, sizeof(tx), &len);
    if (rc != 0)
        return rc;
    if (len == 0)
        return TE_RC(TE_TA_UNIX, TE_EINVAL);

    fd = open(dev, O_RDWR);
    if (fd < 0)
    {
        ERROR("Cannot open %s: %s", dev, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &spi_mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits) < 0 ||
        (speed_hz != 0 &&
         ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0))
    {
        ERROR("Cannot configure %s: %s", dev, strerror(errno));
        close(fd);
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf = (unsigned long)(uintptr_t)tx;
    xfer.rx_buf = (unsigned long)(uintptr_t)rx;
    xfer.len = (uint32_t)len;
    xfer.speed_hz = speed_hz;
    xfer.bits_per_word = spi_bits;

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer) < 0)
    {
        ERROR("SPI_IOC_MESSAGE on %s failed: %s", dev, strerror(errno));
        rc = TE_OS_RC(TE_TA_UNIX, errno);
    }
    else
    {
        bin2hex(rx, len, rx_hex);
    }

    close(fd);

    return rc;
}
