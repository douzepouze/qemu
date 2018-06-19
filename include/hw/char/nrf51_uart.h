/*
 * nRF51 SoC UART emulation
 *
 * Copyright (c) 2018 Julia Suvorova <jusual@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 *
 * QEMU interface:
 * + sysbus MMIO regions 0: Memory Region with tasks, events and registers
 *   to be mapped to the peripherals instance address by the SOC.
 * + Named GPIO output "irq": Interrupt line of the peripheral. Must be
 *   connected to the respective peripheral interrupt line of the NVIC.
 *
 */

#ifndef NRF51_UART_H
#define NRF51_UART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define UART_FIFO_LENGTH 6

#define TYPE_NRF51_UART "nrf51_soc.uart"
#define NRF51_UART(obj) OBJECT_CHECK(Nrf51UART, (obj), TYPE_NRF51_UART)

typedef struct Nrf51UART {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    CharBackend chr;
    qemu_irq irq;
    guint watch_tag;

    uint8_t rx_fifo[UART_FIFO_LENGTH];
    unsigned int rx_fifo_pos;
    unsigned int rx_fifo_len;

    uint32_t reg[0x1000];
} Nrf51UART;

#endif
