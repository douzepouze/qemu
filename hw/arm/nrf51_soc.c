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
#define FICR_SIZE       0x100

#define UICR_BASE       0x10001000
#define UICR_SIZE       0x100

#define SRAM_BASE       0x20000000

#define IOMEM_BASE      0x40000000
#define IOMEM_SIZE      0x20000000

#define UART_BASE       0x40002000
#define UART_SIZE       0x1000
#define UART_INT        2

#define RNG_BASE        0x4000D000

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

/*
FICR Registers Assignments
CODEPAGESIZE      0x010      [4,
CODESIZE          0x014       5,
CLENR0            0x028       10,
PPFC              0x02C       11,
NUMRAMBLOCK       0x034       13,
SIZERAMBLOCKS     0x038       14,
SIZERAMBLOCK[0]   0x038       14,
SIZERAMBLOCK[1]   0x03C       15,
SIZERAMBLOCK[2]   0x040       16,
SIZERAMBLOCK[3]   0x044       17,
CONFIGID          0x05C       23,
DEVICEID[0]       0x060       24,
DEVICEID[1]       0x064       25,
ER[0]             0x080       32,
ER[1]             0x084       33,
ER[2]             0x088       34,
ER[3]             0x08C       35,
IR[0]             0x090       36,
IR[1]             0x094       37,
IR[2]             0x098       38,
IR[3]             0x09C       39,
DEVICEADDRTYPE    0x0A0       40,
DEVICEADDR[0]     0x0A4       41,
DEVICEADDR[1]     0x0A8       42,
OVERRIDEEN        0x0AC       43,
NRF_1MBIT[0]      0x0B0       44,
NRF_1MBIT[1]      0x0B4       45,
NRF_1MBIT[2]      0x0B8       46,
NRF_1MBIT[3]      0x0BC       47,
NRF_1MBIT[4]      0x0C0       48,
BLE_1MBIT[0]      0x0EC       59,
BLE_1MBIT[1]      0x0F0       60,
BLE_1MBIT[2]      0x0F4       61,
BLE_1MBIT[3]      0x0F8       62,
BLE_1MBIT[4]      0x0FC       63]
*/

static const uint32_t ficr_content[64] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0x00000400, 0x00000100, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000002,
        0x00002000, 0x00002000, 0x00002000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000003, 0x12345678, 0x9ABCDEF1,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, };

static uint64_t ficr_read(void *opaque, hwaddr offset, unsigned int size)
{
    qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " [%u]\n",
            __func__, offset, size);

    if (offset > (ARRAY_SIZE(ficr_content) - size)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad read offset 0x%" HWADDR_PRIx "\n", __func__, offset);
        return 0;
    }

    return ficr_content[offset >> 2];
}

static const MemoryRegionOps ficr_ops = {
    .read = ficr_read,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .impl.unaligned = false,
};

static const uint32_t uicr_content[64] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, };

static uint64_t uicr_read(void *opaque, hwaddr offset, unsigned int size)
{
    qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " [%u]\n",
            __func__, offset, size);

    if (offset > (ARRAY_SIZE(uicr_content) - size)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad read offset 0x%" HWADDR_PRIx "\n", __func__, offset);
        return 0;
    }

    return uicr_content[offset >> 2];
}

static const MemoryRegionOps uicr_ops = {
    .read = uicr_read,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .impl.unaligned = false,
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

    object_initialize(&s->rng, sizeof(s->rng), TYPE_NRF51_RNG);
    object_property_add_child(obj, "rng", OBJECT(&s->rng), &error_abort);
    qdev_set_parent_bus(DEVICE(&s->rng), sysbus_get_default());
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

    /* FICR */
    memory_region_init_io(&s->ficr, NULL, &ficr_ops, NULL, "nrf51_soc.ficr",
            FICR_SIZE);
    memory_region_set_readonly(&s->ficr, true);
    memory_region_add_subregion_overlap(&s->container, FICR_BASE, &s->ficr, 0);

    /* UICR */
    memory_region_init_io(&s->uicr, NULL, &uicr_ops, NULL, "nrf51_soc.uicr",
            UICR_SIZE);
    memory_region_set_readonly(&s->uicr, true);
    memory_region_add_subregion_overlap(&s->container, UICR_BASE, &s->uicr, 0);

    /* UART */
    qdev_prop_set_chr(DEVICE(&s->uart), "chardev", serial_hd(0));
    object_property_set_bool(OBJECT(&s->uart), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->uart), 0);
    memory_region_add_subregion_overlap(&s->container, UART_BASE, mr, 0);
    qdev_connect_gpio_out_named(DEVICE(&s->uart), "irq", 0,
            qdev_get_gpio_in(DEVICE(&s->armv7m), BASE_TO_IRQ(UART_BASE)));

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

    /* STUB Peripherals */
    memory_region_init_io(&s->clock, NULL, &clock_ops, NULL, "nrf51_soc.clock", 0x1000);
    memory_region_add_subregion_overlap(&s->container, IOMEM_BASE, &s->clock, -1);

    memory_region_init_io(&s->nvmc, NULL, &nvmc_ops, NULL, "nrf51_soc.nvmc", 0x1000);
    memory_region_add_subregion_overlap(&s->container, 0x4001E000, &s->nvmc, -1);
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
