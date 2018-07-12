/*
 * Nordic Semiconductor nRF51 non-volatile memory
 *
 * It provides an interface to erase regions in flash memory.
 * Furthermore it provides the user and factory information registers.
 *
 * QEMU interface:
 * + sysbus MMIO regions 0: NVMC peripheral registers
 * + sysbus MMIO regions 1: FICR peripheral registers
 * + sysbus MMIO regions 2: UICR peripheral registers
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
#ifndef NRF51_NVM_H
#define NRF51_NVM_H

#include "hw/sysbus.h"

#define TYPE_NRF51_NVMC "nrf51_soc.nvmc"
#define NRF51_NVMC(obj) OBJECT_CHECK(Nrf51NVMCState, (obj), TYPE_NRF51_NVMC)

typedef struct Nrf51NVMCState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t config;

} Nrf51NVMCState;

#define TYPE_NRF51_CODE "nrf51_soc.code"
#define NRF51_CODE(obj) OBJECT_CHECK(Nrf51CODEState, (obj), TYPE_NRF51_CODE)

typedef struct Nrf51CODEState {
    SysBusDevice parent_obj;

    MemoryRegion mem;

    void *storage;

    uint32_t code_size; /* Code size in number of pages */

} Nrf51CODEState;

#define TYPE_NRF51_FICR "nrf51_soc.ficr"
#define NRF51_FICR(obj) OBJECT_CHECK(Nrf51FICRState, (obj), TYPE_NRF51_FICR)

typedef struct Nrf51FICRState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;


} Nrf51FICRState;

#define NRF51_UICR_FIXTURE_SIZE 64

#define TYPE_NRF51_UICR "nrf51_soc.uicr"
#define NRF51_UICR(obj) OBJECT_CHECK(Nrf51UICRState, (obj), TYPE_NRF51_UICR)

typedef struct Nrf51UICRState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

} Nrf51UICRState;


#endif
