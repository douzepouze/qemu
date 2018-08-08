/*
 * BBC micro:bit machine
 * http://tech.microbit.org/hardware/
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/arm/arm.h"
#include "sysemu/qtest.h"
#include "exec/address-spaces.h"

#include "hw/arm/microbit.h"

#define BUTTON_A_PIN 17
#define BUTTON_B_PIN 26

static void microbit_init(MachineState *machine)
{
    MicrobitMachineState *s = MICROBIT_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *soc, *matrix;

    object_initialize(&s->nrf51, sizeof(s->nrf51), TYPE_NRF51_SOC);
    soc = DEVICE(&s->nrf51);
    object_property_add_child(OBJECT(machine), "nrf51", OBJECT(soc),
                              &error_fatal);
    object_property_set_link(OBJECT(soc), OBJECT(system_memory),
                             "memory", &error_abort);
    qdev_prop_set_uint32(soc, "variant", NRF51_VARIANT_AA);

    object_property_set_bool(OBJECT(soc), true, "realized", &error_abort);

    object_initialize(&s->matrix, sizeof(s->matrix), TYPE_LED_MATRIX);
    matrix = DEVICE(&s->matrix);
    qdev_prop_set_uint16(matrix, "rows", 3);
    qdev_prop_set_uint16(matrix, "cols", 9);
    object_property_set_bool(OBJECT(matrix), true, "realized", &error_fatal);

    qdev_connect_gpio_out(soc, 4, qdev_get_gpio_in_named(matrix, "col", 0));
    qdev_connect_gpio_out(soc, 5, qdev_get_gpio_in_named(matrix, "col", 1));
    qdev_connect_gpio_out(soc, 6, qdev_get_gpio_in_named(matrix, "col", 2));
    qdev_connect_gpio_out(soc, 7, qdev_get_gpio_in_named(matrix, "col", 3));
    qdev_connect_gpio_out(soc, 8, qdev_get_gpio_in_named(matrix, "col", 4));
    qdev_connect_gpio_out(soc, 9, qdev_get_gpio_in_named(matrix, "col", 5));
    qdev_connect_gpio_out(soc, 10, qdev_get_gpio_in_named(matrix, "col", 6));
    qdev_connect_gpio_out(soc, 11, qdev_get_gpio_in_named(matrix, "col", 7));
    qdev_connect_gpio_out(soc, 12, qdev_get_gpio_in_named(matrix, "col", 8));

    qdev_connect_gpio_out(soc, 13, qdev_get_gpio_in_named(matrix, "row", 0));
    qdev_connect_gpio_out(soc, 14, qdev_get_gpio_in_named(matrix, "row", 1));
    qdev_connect_gpio_out(soc, 15, qdev_get_gpio_in_named(matrix, "row", 2));

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename, 0x00);
}

static void microbit_reset(void)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    MicrobitMachineState *s = MICROBIT_MACHINE(machine);

    qemu_devices_reset();

    /* Board level pull-up */
    if (!qtest_enabled()) {
        qemu_set_irq(qdev_get_gpio_in(DEVICE(&s->nrf51), BUTTON_A_PIN), 1);
        qemu_set_irq(qdev_get_gpio_in(DEVICE(&s->nrf51), BUTTON_B_PIN), 1);
    }
}

static void microbit_machine_init(MachineClass *mc)
{
    mc->desc = "BBC micro:bit";
    mc->init = microbit_init;
    mc->max_cpus = 1;
    mc->reset = microbit_reset;
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

