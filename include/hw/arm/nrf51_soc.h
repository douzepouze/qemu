/*
 * Nordic Semiconductor nRF51  SoC
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef NRF51_SOC_H
#define NRF51_SOC_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/arm/armv7m.h"
#include "hw/char/nrf51_uart.h"
#include "hw/misc/nrf51_rng.h"
#include "hw/nvram/nrf51_nvm.h"
#include "hw/gpio/nrf51_gpio.h"
#include "hw/timer/nrf51_timer.h"

#define TYPE_NRF51_SOC "nrf51-soc"
#define NRF51_SOC(obj) \
    OBJECT_CHECK(NRF51State, (obj), TYPE_NRF51_SOC)

#define NRF51_TIMER_NUM 3

typedef struct NRF51State {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    ARMv7MState cpu;

    MemoryRegion iomem;
    MemoryRegion sram;
    MemoryRegion flash;

    MemoryRegion *board_memory;
    MemoryRegion container;

    MemoryRegion clock;

    Nrf51UART uart;
    Nrf51NVMState nvm;
    Nrf51RNGState rng;
    Nrf51GPIOState gpio;
    Nrf51TimerState timer[NRF51_TIMER_NUM];

    /* Properties */
    int32_t part_variant;
} NRF51State;


/* Variants as described in nRF51 product specification section 10.6 table 73 */
typedef enum {
    NRF51_VARIANT_INVALID = -1,
    NRF51_VARIANT_AA = 0,
    NRF51_VARIANT_AB = 1,
    NRF51_VARIANT_AC = 2,
    NRF51_VARIANT_MAX = 3
} NRF51Variants;

#endif

