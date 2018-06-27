/*
 * BBC micro:bit machine
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/arm/arm.h"

#include "hw/arm/nrf51_soc.h"

static void microbit_init(MachineState *machine)
{
    DeviceState *dev;

    dev = qdev_create(NULL, TYPE_NRF51_SOC);
    qdev_prop_set_uint32(DEVICE(dev), "VARIANT", NRF51_VARIANT_AA);
    object_property_set_bool(OBJECT(dev), true, "realized", &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename,
            0x00000000);
}

static void microbit_machine_init(MachineClass *mc)
{
    mc->desc = "BBC micro:bit";
    mc->init = microbit_init;
    mc->ignore_memory_transaction_failures = true;
}
DEFINE_MACHINE("microbit", microbit_machine_init);
