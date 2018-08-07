/*
 * BBC micro:bit machine
 * http://tech.microbit.org/hardware/
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
#include "exec/address-spaces.h"

#include "hw/arm/microbit.h"

static void microbit_init(MachineState *machine)
{
    MicrobitMachineState *s = MICROBIT_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    Object *soc;

    object_initialize(&s->nrf51, sizeof(s->nrf51), TYPE_NRF51_SOC);
    soc = OBJECT(&s->nrf51);
    object_property_add_child(OBJECT(machine), "nrf51", soc, &error_fatal);
    object_property_set_link(soc, OBJECT(system_memory),
                             "memory", &error_abort);

    object_property_set_bool(soc, true, "realized", &error_abort);

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename,
            NRF51_SOC(soc)->flash_size);
}


static void microbit_machine_init(MachineClass *mc)
{
    mc->desc = "BBC micro:bit";
    mc->init = microbit_init;
    mc->max_cpus = 1;
}

static void microbit_machine_init_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    microbit_machine_init(mc);
}

static const TypeInfo microbit_machine_info = {
    .name       = TYPE_MICROBIT_MACHINE,
    .parent     = TYPE_MACHINE,
    .instance_size = sizeof(MicrobitMachineState),
    .class_init = microbit_machine_init_class_init,
};

static void microbit_machine_types(void)
{
    type_register_static(&microbit_machine_info);
}

type_init(microbit_machine_types)

