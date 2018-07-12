/*
 * Nordic Semiconductor nRF51 SoC
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef NRF51_SOC_H
#define NRF51_SOC_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/arm/armv7m.h"
#include "hw/misc/unimp.h"
#include "hw/char/nrf51_uart.h"
#include "hw/misc/nrf51_rng.h"
#include "hw/nvram/nrf51_nvm.h"


#define TYPE_NRF51_SOC "nrf51-soc"
#define NRF51_SOC(obj) \
    OBJECT_CHECK(NRF51State, (obj), TYPE_NRF51_SOC)

typedef struct NRF51State {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    /* TODO: Change to armv6m when cortex-m0 core is available */
    ARMv7MState armv7m;

    UnimplementedDeviceState mmio;
    Nrf51CODEState code;
    Nrf51UART uart;
    Nrf51NVMCState nvmc;
    Nrf51RNGState rng;

    MemoryRegion container;
    MemoryRegion sram;
    MemoryRegion ficr;
    MemoryRegion uicr;
    MemoryRegion clock;

    /* Properties */
    int32_t part_variant;
} NRF51State;

typedef enum {
    NRF51_VARIANT_INVALID = -1,
    NRF51_VARIANT_AA = 0,
    NRF51_VARIANT_AB = 1,
    NRF51_VARIANT_AC = 2,
    NRF51_VARIANT_MAX = 3
} NRF51Variants;

#endif

