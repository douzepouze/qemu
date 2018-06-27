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
#include "crypto/random.h"

#include "hw/arm/nrf51_soc.h"

#define IOMEM_BASE      0x40000000
#define IOMEM_SIZE      0x20000000

#define FLASH_BASE      0x00000000

#define FICR_BASE       0x10000000
#define FICR_SIZE       0x100

#define SRAM_BASE       0x20000000

#define UART_BASE       0x40002000
#define UART_SIZE       0x1000

#define PAGE_SIZE       0x0400


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
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n", __func__, addr, size);
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

static uint64_t nvmc_read(void *opaque, hwaddr addr, unsigned int size)
{
    qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " [%u]\n", __func__, addr, size);
    return 1;
}

static void nvmc_write(void *opaque, hwaddr addr, uint64_t data, unsigned int size)
{
    qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n", __func__, addr, data, size);
}


static const MemoryRegionOps nvmc_ops = {
    .read = nvmc_read,
    .write = nvmc_write
};

static uint64_t rng_read(void *opaque, hwaddr addr, unsigned int size)
{
    uint64_t r = 0;

    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n", __func__, addr, size);

    switch (addr) {
    case 0x508:
        qcrypto_random_bytes((uint8_t *)&r, 1, NULL);
        break;
    default:
        r = 1;
        break;
    }
    return r;
}

static void rng_write(void *opaque, hwaddr addr, uint64_t data, unsigned int size)
{
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n", __func__, addr, data, size);
}


static const MemoryRegionOps rng_ops = {
    .read = rng_read,
    .write = rng_write
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

    object_initialize(&s->uart, sizeof(s->uart), TYPE_NRF51_UART);
    object_property_add_child(obj, "uart", OBJECT(&s->uart), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->uart), sysbus_get_default());
}


static void nrf51_soc_realize(DeviceState *dev_soc, Error **errp)
{
    NRF51State *s = NRF51_SOC(dev_soc);
    Error *err = NULL;

    if (!(s->part_variant > NRF51_VARIANT_INVALID
            && s->part_variant < NRF51_VARIANT_MAX)) {
        error_setg(errp, "VARIANT not set or invalid");
        return;
    }

    memory_region_init_ram(&s->sram, NULL, "nrf51_soc.sram",
            NRF51VariantAttributes[s->part_variant].ram_size * PAGE_SIZE, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(&s->container, SRAM_BASE, &s->sram);

    memory_region_init_ram(&s->flash, NULL, "nrf51_soc.flash",
            NRF51VariantAttributes[s->part_variant].flash_size * PAGE_SIZE,
            &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(&s->container, FLASH_BASE, &s->flash);

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
    create_unimplemented_device("nrf51_soc.io", IOMEM_BASE, IOMEM_SIZE);

    /* FICR */
    create_unimplemented_device("nrf51_soc.ficr", FICR_BASE, FICR_SIZE);

    qdev_prop_set_chr(DEVICE(&s->uart), "chardev", serial_hd(0));
    qdev_init_nofail(DEVICE(&s->uart));
/*    sysbus_mmio_map(s, 0, UART_BASE);
    sysbus_connect_irq(s, 0, qdev_get_gpio_in(s->nvic, 2)); */

    memory_region_init_io(&s->clock, NULL, &clock_ops, NULL, "nrf51_soc.clock", 0x1000);
    memory_region_add_subregion_overlap(&s->container, IOMEM_BASE, &s->clock, -1);

    memory_region_init_io(&s->nvmc, NULL, &nvmc_ops, NULL, "nrf51_soc.nvmc", 0x1000);
    memory_region_add_subregion_overlap(&s->container, 0x4001E000, &s->nvmc, -1);

    memory_region_init_io(&s->rng, NULL, &rng_ops, NULL, "nrf51_soc.rng", 0x1000);
    memory_region_add_subregion_overlap(&s->container, 0x4000D000, &s->rng, -1);
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
