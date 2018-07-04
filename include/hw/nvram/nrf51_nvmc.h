/*
 * Nordic Semiconductor nRF51 non-volatile memory
 *
 * This peripheral manages access to flash memory included on the SOC.
 * It provides an interface to erase regions in flash memory.
 * Furthermore it provides the user and factory information registers.
 *
 * See nRF51 reference manual and product sheet sections:
 * + Non-Volatile Memory Controller (NVMC)
 * + Factory Information Configuration Registers (FICR)
 * + User Information Configuration Registers (UICR)
 *
 * QEMU interface:
 * + sysbus MMIO regions 0: Memory Region with registers
 *   to be mapped to the peripherals instance address by the SOC.
 * + page_size property to set the page size in bytes.
 * + code_size property to set the code size in number of pages.
 *
 * Accuracy of the peripheral model:
 * + The NVMC is always ready, all requested erase operations succeed
 *   immediately.
 * + CONFIG.WEN and CONFIG.EEN flags can be written and read back
 *   but are not evaluated to check whether a requested write/erase operation
 *   is legal.
 * + Code regions (MPU configuration) are disregarded.
 *
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 *
 */
#ifndef NRF51_NVMC_H
#define NRF51_NVMC_H

#include "hw/sysbus.h"
#define TYPE_NRF51_NVMC "nrf51_soc.nvmc"
#define NRF51_NVMC(obj) OBJECT_CHECK(Nrf51NVMCState, (obj), TYPE_NRF51_NVMC)

typedef struct Nrf51NVMCState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t code_size;
    uint16_t page_size;
    uint8_t *empty_page;
    MemoryRegion *mr;
    AddressSpace as;

    struct {
        uint32_t config:2;
    } state;

} Nrf51NVMCState;


#endif
