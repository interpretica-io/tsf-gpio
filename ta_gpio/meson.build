# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): agent-side GPIO/I2C/SPI access
# over libgpiod v2 and the i2c-dev/spidev ioctls. The agent and its RPC
# server both link it.

headers += files(
    'ta_gpio.h',
)

sources += files(
    'ta_gpio.c',
)

te_libs += [
    'logger_core',
    'tools',
]

# libgpiod ships libgpiod.pc; fall back to finding the library and its
# header directly where it does not.
cc = meson.get_compiler('c')
gpiod_dep = dependency('libgpiod', required: false)
if not gpiod_dep.found()
    gpiod_dep = cc.find_library('gpiod', has_headers: 'gpiod.h')
endif

deps += [
    gpiod_dep,
]
