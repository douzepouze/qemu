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
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "qemu/error-report.h"
#include "sysemu/qtest.h"

#include "hw/arm/nrf51_soc.h"
#include "hw/display/led_matrix.h"

typedef struct {
    MachineState parent;

    NRF51State nrf51;
    LEDMatrixState matrix;
} MicrobitMachineState;

#define BUTTON_A_PIN 17
#define BUTTON_B_PIN 26

static int32_t const matrix_coords[] = {
    0, 0, 2, 0, 4, 0, 4, 3, 3, 3, 2, 3, 1, 3, 0, 3, 1, 2,
    4, 2, 0, 2, 2, 2, 1, 0, 3, 0, 3, 4, 1, 4, -1, -1, -1, -1,
    2, 4, 4, 4, 0, 4, 0, 1, 1, 1, 2, 1, 3, 1, 4, 1, 3, 2
};


#define TYPE_MICROBIT_MACHINE MACHINE_TYPE_NAME("microbit")

#define MICROBIT_MACHINE(obj) \
    OBJECT_CHECK(MicrobitMachineState, obj, TYPE_MICROBIT_MACHINE)

static void microbit_cpu_reset(void *opaque)
{
    ARMCPU *cpu = opaque;

    cpu_reset(CPU(cpu));
}

static void microbit_init(MachineState *machine)
{
    MicrobitMachineState *s = MICROBIT_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    Object *soc = OBJECT(&s->nrf51);
    size_t i;

    sysbus_init_child_obj(OBJECT(machine), "nrf51", soc, sizeof(s->nrf51),
                          TYPE_NRF51_SOC);
    qdev_prop_set_chr(DEVICE(&s->nrf51), "serial0", serial_hd(0));
    object_property_set_link(soc, OBJECT(system_memory), "memory",
                             &error_fatal);
    object_property_set_bool(soc, true, "realized", &error_fatal);

    object_initialize(&s->matrix, sizeof(s->matrix), TYPE_LED_MATRIX);
    DeviceState *matrix = DEVICE(&s->matrix);
    object_property_set_bool(OBJECT(matrix), true, "strobe-row", &error_fatal);
    qdev_prop_set_uint16(matrix, "rows", 3);
    qdev_prop_set_uint16(matrix, "cols", 9);
    qdev_prop_set_uint32(matrix, "len-matrix-coords", ARRAY_SIZE(matrix_coords));
    for (i = 0; i < ARRAY_SIZE(matrix_coords); i++) {
        char *propname = g_strdup_printf("matrix-coords[%d]", i);
        qdev_prop_set_int32(matrix, propname, matrix_coords[i]);
        g_free(propname);
    }
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

    if (machine->kernel_filename) {
        error_report("-device loader,file=<filename> must be used instead of -kernel");
        exit(1);
    }

    qemu_register_reset(microbit_cpu_reset, ARM_CPU(first_cpu));
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

static void microbit_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "BBC micro:bit";
    mc->init = microbit_init;
    mc->max_cpus = 1;
    mc->reset = microbit_reset;
}

static const TypeInfo microbit_info = {
    .name = TYPE_MICROBIT_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(MicrobitMachineState),
    .class_init = microbit_machine_class_init,
};

static void microbit_machine_init(void)
{
    type_register_static(&microbit_info);
}

type_init(microbit_machine_init);
