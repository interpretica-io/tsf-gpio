# tsf-gpio

Driving the low-level buses of a Test Agent — GPIO, I²C and SPI —
packaged as an external Test Environment (TE) repository (consumed with
the `TE_EXT_REPO` builder directive). It uses the kernel interfaces
directly over a C library and ioctls — **no `gpioget`/`i2cget`/`spidev`
tool is scraped, nothing is spawned**.

Three libraries:

- `ta_gpio` — agent side. GPIO over **libgpiod v2** (`gpiod.h`,
  `-lgpiod`), I²C over `/dev/i2c-N` with the `linux/i2c-dev.h` ioctls,
  and SPI over `/dev/spidevB.C` with the `linux/spi/spidev.h` ioctls:
  list the GPIO chips, read and drive a line, run an I²C write/read, and
  run a full-duplex SPI transfer. It touches only what a call names and
  never scans an unknown bus. The agent and its RPC server both link it.
- `rpcs_gpio` — the `gpio_*` RPCs for the RPC server of the agent, thin
  wrappers over `ta_gpio`. The bus access happens on the agent, where
  the hardware is.
- `tapi_gpio` — engine side. `tapi_gpio.h` lists chips into a
  `tapi_gpio_chip` vector and gives a test the line, I²C and SPI calls
  (with byte buffers); `tapi_gpio_rpc.h` is the one-per-RPC layer
  beneath. This is functional — there is **no** security-audit layer and
  no dependency on tsf-cybersec.

TE has no GPIO/I²C/SPI access of its own.

## What it drives

```c
te_vec chips = TE_VEC_INIT(tapi_gpio_chip);
const tapi_gpio_chip *chip;
int value;
uint8_t reg = 0x75, who = 0;

CHECK_RC(tapi_gpio_list(rpcs, &chips));
TE_VEC_FOREACH(&chips, chip)
    RING("%s: %s, %u lines", chip->path, chip->label, chip->num_lines);
tapi_gpio_list_free(&chips);

/* Read line 17 of a chip as an input. */
CHECK_RC(tapi_gpio_get(rpcs, "/dev/gpiochip0", 17, &value));

/* Read register 0x75 (WHO_AM_I) of an I2C device at 0x68 on bus 1. */
CHECK_RC(tapi_i2c_transfer(rpcs, 1, 0x68, &reg, 1, &who, 1));
```

GPIO lines are requested, read or driven, and released; I²C does a
write-then-read as one `I2C_RDWR` transaction (so a register read is a
repeated start); SPI clocks out and in the same number of bytes in one
`SPI_IOC_MESSAGE`. Across the RPC the chip list is a tab-separated
record the engine parses into `tapi_gpio_chip`, and the I²C/SPI byte
buffers cross as lower-case hex (the engine converts to and from bytes,
so a test works in `uint8_t`).

## The library is linked, not a program

`ta_gpio` does not run `gpioget`, `i2ctransfer` or any spidev tool. It
links libgpiod and opens the i2c-dev/spidev character devices in the
agent's RPC server process, calling `gpiod_chip_request_lines()`,
`ioctl(I2C_RDWR)` and `ioctl(SPI_IOC_MESSAGE)` directly.

Driving a GPIO line is **momentary**: `tapi_gpio_set()` requests the
line as an output at the value and releases it before returning, which
returns the line to its default. A persistent hold needs a stateful
API this does not yet have.

## Agent host requirements

- **libgpiod v2** with its development headers (Debian/trixie:
  `apt install libgpiod-dev`, giving `gpiod.h` and `libgpiod.so.3`).
  This targets the **v2** API (`gpiod_chip_request_lines`,
  `gpiod_line_settings_*`, `gpiod_line_config_*`); libgpiod **v1** has a
  different, incompatible API and will not compile against this code.
- The Linux **i2c-dev** and **spidev** drivers, exposing `/dev/i2c-N`
  and `/dev/spidevB.C`. I²C/SPI need no extra library — only the kernel
  uapi headers (`linux/i2c-dev.h`, `linux/i2c.h`, `linux/spi/spidev.h`).
- Access to the device nodes (root, or a `gpio`/`i2c`/`spi` group).

## Usage

Declare the repository in an external libraries catalog and pass it to
`dispatcher.sh --external=<catalog.yml>`:

```yaml
repositories:
  - name: tsf_gpio
    url: https://github.com/interpretica-io/tsf-gpio.git
    ref: <tag>
    libs:
      - ta_gpio
      - rpcs_gpio
      - tapi_gpio
```

In `builder.conf`, bind `tapi_gpio` to the engine, list `ta_gpio` and
`rpcs_gpio` among the RPC server's libraries, and add the RPC
definitions to both platforms:

```
TE_EXT_REPO_USE([tsf_gpio], [ta_gpio rpcs_gpio], [tapi_gpio])

TE_LIB_PARMS([rpcxdr], [${TE_HOST}], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_gpio/gpio_rpc.x.m4])
TE_LIB_PARMS([rpcxdr], [${TE_TA_TYPE}], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_gpio/gpio_rpc.x.m4])
```

The RPC program number is **27** (20–26 are taken by the other tsf
agent RPCs); change it in `gpio_rpc.x.m4` if it ever collides.

## What was verified, and what was not

**Nothing here was compiled or syntax-checked.** Unlike tsf-usb — whose
libusb usage could be `-fsyntax-only`'d against the installed library —
libgpiod and the Linux `i2c-dev`/`spidev` uapi headers are Linux-only
and are not present on the machine this was written on, so `ta_gpio.c`
could not be checked against them. The libgpiod calls were written
against the **documented libgpiod v2 API** and the ioctl use against the
**Linux uapi headers**; the engine side (`tapi_gpio*.c`), the RPCs and
the meson/`TE_EXT_REPO` wiring follow the tsf-usb template but were not
built either.

The first thing to check on a real build is the **libgpiod version**:
v2 is assumed, and its API differs from v1 entirely; even within v2 the
`gpiod_line_*` names settled over early releases. After that, expect the
ordinary first-build fixes, and confirm the i2c-dev/spidev struct and
ioctl names against the target kernel's headers.

## Scope

- **It touches only what you name.** A chip line, an I²C address, a
  spidev node — given explicitly. It never probes an unknown I²C bus for
  devices (a blind probe can wedge a device that misreads it).
- **GPIO drive is momentary** (see above): read-back of an output you
  set is not meaningful through separate calls, because the line is
  released between them.
- **No bit-banging.** I²C and SPI go through the kernel controller
  drivers; this does not bit-bang a protocol over raw GPIO.
