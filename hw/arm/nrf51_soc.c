/*
 * Nordic Semiconductor nRF51 SoC
 * http://infocenter.nordicsemi.com/pdf/nRF51_RM_v3.0.1.pdf
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/arm.h"
#include "hw/sysbus.h"
#include "hw/boards.h"
#include "hw/devices.h"
#include "hw/misc/unimp.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "qemu/log.h"
#include "cpu.h"

#include "hw/arm/nrf51_soc.h"

#define IOMEM_BASE      0x40000000
#define IOMEM_SIZE      0x20000000

#define FICR_BASE       0x10000000
#define FICR_SIZE       0x000000fc

#define FLASH_BASE      0x00000000
#define SRAM_BASE       0x20000000

/* The size and base is for the NRF51822 part. If other parts
 * are supported in the future, add a sub-class of NRF51SoC for
 * the specific variants */
#define NRF51822_FLASH_SIZE     (256 * 1024)
#define NRF51822_SRAM_SIZE      (16 * 1024)

/* IRQ lines can be derived from peripheral base addresses */
#define BASE_TO_IRQ(base) (((base) >> 12) & 0x1F)

static void nrf51_soc_realize(DeviceState *dev_soc, Error **errp)
{
    NRF51State *s = NRF51_SOC(dev_soc);
    Error *err = NULL;

    if (!s->board_memory) {
        error_setg(errp, "memory property was not set");
        return;
    }

    object_property_set_link(OBJECT(&s->cpu), OBJECT(&s->container), "memory",
            &err);
    object_property_set_bool(OBJECT(&s->cpu), true, "realized", &err);

    memory_region_add_subregion_overlap(&s->container, 0, s->board_memory, -1);

    memory_region_init_ram(&s->flash, OBJECT(s), "nrf51.flash", s->flash_size,
            &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_set_readonly(&s->flash, true);
    memory_region_add_subregion(&s->container, FLASH_BASE, &s->flash);

    memory_region_init_ram(&s->sram, NULL, "nrf51.sram", s->sram_size, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(&s->container, SRAM_BASE, &s->sram);

    create_unimplemented_device("nrf51_soc.io", IOMEM_BASE, IOMEM_SIZE);
    create_unimplemented_device("nrf51_soc.ficr", FICR_BASE, FICR_SIZE);
    create_unimplemented_device("nrf51_soc.private", 0xF0000000, 0x10000000);
}

static void nrf51_soc_init(Object *obj)
{
    NRF51State *s = NRF51_SOC(obj);

    memory_region_init(&s->container, obj, "nrf51-container", UINT64_MAX);

    object_initialize(&s->cpu, sizeof(s->cpu), TYPE_ARMV7M);
    object_property_add_child(OBJECT(s), "armv6m", OBJECT(&s->cpu), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->cpu), sysbus_get_default());
    qdev_prop_set_string(DEVICE(&s->cpu), "cpu-type", ARM_CPU_TYPE_NAME("cortex-m0"));
    qdev_prop_set_uint32(DEVICE(&s->cpu), "num-irq", 32);
}

static Property nrf51_soc_properties[] = {
    DEFINE_PROP_LINK("memory", NRF51State, board_memory, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_UINT32("sram-size", NRF51State, sram_size, NRF51822_SRAM_SIZE),
    DEFINE_PROP_UINT32("flash-size", NRF51State, flash_size, NRF51822_FLASH_SIZE),
    DEFINE_PROP_END_OF_LIST(),
};

static void nrf51_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = nrf51_soc_realize;
    dc->props = nrf51_soc_properties;
}

static const TypeInfo nrf51_soc_info = {
    .name          = TYPE_NRF51_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NRF51State),
    .instance_init = nrf51_soc_init,
    .class_init    = nrf51_soc_class_init,
};

static void nrf51_soc_types(void)
{
    type_register_static(&nrf51_soc_info);
}
type_init(nrf51_soc_types)
