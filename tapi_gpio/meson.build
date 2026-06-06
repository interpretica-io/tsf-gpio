# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): engine-side TAPI for driving
# GPIO/I2C/SPI on agents. Built as a shared library on the engine
# platform. It needs the gpio_* RPCs in rpcxdr, so add
# ../ta_gpio/gpio_rpc.x.m4 to the rpcxdr definitions of the engine
# platform. This is functional, not a security audit - it does not
# depend on tsf-cybersec.

if get_option('engine')
    build_lib_shared = true
endif
build_lib_static = not get_option('engine')
install_lib = install_dev

headers += files(
    'tapi_gpio.h',
    'tapi_gpio_rpc.h',
)

sources += files(
    'tapi_gpio.c',
    'tapi_gpio_rpc.c',
)

te_libs += [
    'tapi',
    'tapi_rpc',
    'rcfrpc',
    'confapi',
    'rcfapi',
    'rpcxdr',
    'logger_core',
    'tools',
]
