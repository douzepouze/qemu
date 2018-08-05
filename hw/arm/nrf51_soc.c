/*
 * Nordic Semiconductor nRF51 SoC
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


#define FLASH_BASE      0x00000000
#define FICR_BASE       0x10000000
#define UICR_BASE       0x10001000
#define SRAM_BASE       0x20000000

#define IOMEM_BASE      0x40000000
#define IOMEM_SIZE      0x20000000

#define UART_BASE       0x40002000
#define TIMER_BASE      0x40008000
#define RNG_BASE        0x4000D000
#define NVMC_BASE       0x4001E000
#define GPIO_BASE       0x50000000

#define PAGE_SIZE       1024

#define BASE_TO_IRQ(base) ((base >> 12) & 0x1F)


struct {
  hwaddr ram_size;
  hwaddr flash_size;
} NRF51VariantAttributes[] = {
        {.ram_size = 16, .flash_size = 256 },
        {.ram_size = 16, .flash_size = 128 },
        {.ram_size = 32, .flash_size = 256 },
};


static uint64_t clock_read(void *opaque, hwaddr addr, unsigned int size)
{
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n",
            __func__, addr, size);
    return 1;
}

static void clock_write(void *opaque, hwaddr addr, uint64_t data, unsigned int size)
{
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n", __func__, addr, data, size);
}

static const MemoryRegionOps clock_ops = {
    .read = clock_read,
    .write = clock_write
};

static void nrf51_soc_init(Object *obj)
{
    NRF51State *s = NRF51_SOC(obj);

    memory_region_init(&s->container, obj, "microbit-container",
            UINT64_MAX);

    /* TODO: Change to armv6m when cortex-m0 core is available */
    object_initialize(&s->armv7m, sizeof(s->armv7m), TYPE_ARMV7M);
    object_property_add_child(obj, "armv7m", OBJECT(&s->armv7m), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->armv7m), sysbus_get_default());
    qdev_prop_set_string(DEVICE(&s->armv7m), "cpu-type",
                         ARM_CPU_TYPE_NAME("cortex-m3"));

    object_initialize(&s->mmio, sizeof(s->mmio), TYPE_UNIMPLEMENTED_DEVICE);
    object_property_add_child(obj, "iomem", OBJECT(&s->mmio), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->mmio), sysbus_get_default());
    qdev_prop_set_string(DEVICE(&s->mmio), "name", "nrf51.iomem");
    qdev_prop_set_uint64(DEVICE(&s->mmio), "size", IOMEM_SIZE);

    object_initialize(&s->uart, sizeof(s->uart), TYPE_NRF51_UART);
    object_property_add_child(obj, "uart", OBJECT(&s->uart), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->uart), sysbus_get_default());

    object_initialize(&s->nvm, sizeof(s->nvm), TYPE_NRF51_NVM);
    object_property_add_child(obj, "nvm", OBJECT(&s->nvm), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->nvm), sysbus_get_default());

    object_initialize(&s->rng, sizeof(s->rng), TYPE_NRF51_RNG);
    object_property_add_child(obj, "rng", OBJECT(&s->rng), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->rng), sysbus_get_default());

    object_initialize(&s->gpio, sizeof(s->gpio), TYPE_NRF51_GPIO);
    object_property_add_child(obj, "gpio", OBJECT(&s->gpio), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->gpio), sysbus_get_default());

    object_initialize(&s->timer, sizeof(s->timer), TYPE_NRF51_TIMER);
    object_property_add_child(obj, "timer0", OBJECT(&s->timer), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->timer), sysbus_get_default());

}

static void nrf51_soc_realize(DeviceState *dev_soc, Error **errp)
{
    NRF51State *s = NRF51_SOC(dev_soc);
    Error *err = NULL;
    MemoryRegion *mr = NULL;

    if (!(s->part_variant > NRF51_VARIANT_INVALID
            && s->part_variant < NRF51_VARIANT_MAX)) {
        error_setg(errp, "VARIANT not set or invalid");
        return;
    }

    /* SRAM */
    memory_region_init_ram(&s->sram, NULL, "nrf51_soc.sram",
            NRF51VariantAttributes[s->part_variant].ram_size * PAGE_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(&s->container, SRAM_BASE, &s->sram);

    /* FLASH */
    memory_region_init_ram(&s->flash, NULL, "nrf51_soc.flash",
            NRF51VariantAttributes[s->part_variant].flash_size * PAGE_SIZE,
            &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(&s->container, FLASH_BASE, &s->flash);

    /* MCU */
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "num-irq", 60);
    object_property_set_link(OBJECT(&s->armv7m), OBJECT(&s->container),
                                         "memory", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    object_property_set_bool(OBJECT(&s->armv7m), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    /* IO space */
    object_property_set_bool(OBJECT(&s->mmio), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->mmio), 0);
    memory_region_add_subregion_overlap(&s->container, IOMEM_BASE, mr, -1500);

    /* UART */
    qdev_prop_set_chr(DEVICE(&s->uart), "chardev", serial_hd(0));
    object_property_set_bool(OBJECT(&s->uart), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->uart), 0);
    memory_region_add_subregion_overlap(&s->container, UART_BASE, mr, 0);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart), 0,
                       qdev_get_gpio_in(DEVICE(&s->armv7m),
                       BASE_TO_IRQ(UART_BASE)));

    /* TIMER0 */
     object_property_set_bool(OBJECT(&s->timer), true, "realized", &err);
     if (err) {
        error_propagate(errp, err);
        return;
     }

     mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->timer), 0);
     memory_region_add_subregion_overlap(&s->container, TIMER_BASE, mr, 0);
     sysbus_connect_irq(SYS_BUS_DEVICE(&s->timer), 0,
                        qdev_get_gpio_in(DEVICE(&s->armv7m),
                        BASE_TO_IRQ(TIMER_BASE)));

    /* NVMC */
    object_property_set_link(OBJECT(&s->nvm), OBJECT(&s->container),
                                         "memory", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    object_property_set_uint(OBJECT(&s->nvm),
            NRF51VariantAttributes[s->part_variant].flash_size, "code_size",
            &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    object_property_set_bool(OBJECT(&s->nvm), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 0);
    memory_region_add_subregion_overlap(&s->container, NVMC_BASE, mr, 0);
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 1);
    memory_region_add_subregion_overlap(&s->container, FICR_BASE, mr, 0);
    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 2);
    memory_region_add_subregion_overlap(&s->container, UICR_BASE, mr, 0);

    /* RNG */
    object_property_set_bool(OBJECT(&s->rng), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->rng), 0);
    memory_region_add_subregion_overlap(&s->container, RNG_BASE, mr, 0);
    qdev_connect_gpio_out_named(DEVICE(&s->rng), "irq", 0,
            qdev_get_gpio_in(DEVICE(&s->armv7m), BASE_TO_IRQ(RNG_BASE)));

    /* GPIO */
    object_property_set_bool(OBJECT(&s->gpio), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->gpio), 0);
    memory_region_add_subregion_overlap(&s->container, GPIO_BASE, mr, 0);

    /* Pass all GPIOs to the SOC layer so they are available to the board */
    qdev_pass_gpios(DEVICE(&s->gpio), dev_soc, NULL);

    /* STUB Peripherals */
    memory_region_init_io(&s->clock, NULL, &clock_ops, NULL, "nrf51_soc.clock", 0x1000);
    memory_region_add_subregion_overlap(&s->container, IOMEM_BASE, &s->clock, -1);
}

static Property nrf51_soc_properties[] = {
    DEFINE_PROP_INT32("VARIANT", NRF51State, part_variant,
            NRF51_VARIANT_INVALID),
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
