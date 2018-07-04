/*
 * Nordic Semiconductor nRF51 non-volatile memory
 *
 * This peripheral manages access to flash memory included on the SOC.
 * It provides an interface to erase regions in flash memory.
 * Furthermore it provides the user and factory information registers.
 *
 * See nRF51 reference manual and product sheet sections:
 * + Non-Volatile Memory Controller (NVMC)
 * + Factory Information Configuration Registers (FICR)
 * + User Information Configuration Registers (UICR)
 *
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/nvram/nrf51_nvmc.h"
#include "exec/address-spaces.h"

#define NRF51_NVMC_SIZE         0x1000

#define NRF51_NVMC_READY        0x400
#define NRF51_NVMC_READY_READY  0x01
#define NRF51_NVMC_CONFIG       0x504
#define NRF51_NVMC_CONFIG_MASK  0x03
#define NRF51_NVMC_CONFIG_WEN   0x01
#define NRF51_NVMC_CONFIG_EEN   0x02
#define NRF51_NVMC_ERASEPCR1    0x508
#define NRF51_NVMC_ERASEPCR0    0x510
#define NRF51_NVMC_ERASEALL     0x50C
#define NRF51_NVMC_ERASEUICR    0x512
#define NRF51_NVMC_ERASE        0x01

#define NRF51_FICR_BASE         0x10000000
#define NRF51_FICR_SIZE         0x100

#define NRF51_UICR_OFFSET       0x10001000UL
#define NRF51_UICR_SIZE         0x100


/*
FICR Registers Assignments
CODEPAGESIZE      0x010
CODESIZE          0x014
CLENR0            0x028
PPFC              0x02C
NUMRAMBLOCK       0x034
SIZERAMBLOCKS     0x038
SIZERAMBLOCK[0]   0x038
SIZERAMBLOCK[1]   0x03C
SIZERAMBLOCK[2]   0x040
SIZERAMBLOCK[3]   0x044
CONFIGID          0x05C
DEVICEID[0]       0x060
DEVICEID[1]       0x064
ER[0]             0x080
ER[1]             0x084
ER[2]             0x088
ER[3]             0x08C
IR[0]             0x090
IR[1]             0x094
IR[2]             0x098
IR[3]             0x09C
DEVICEADDRTYPE    0x0A0
DEVICEADDR[0]     0x0A4
DEVICEADDR[1]     0x0A8
OVERRIDEEN        0x0AC
NRF_1MBIT[0]      0x0B0
NRF_1MBIT[1]      0x0B4
NRF_1MBIT[2]      0x0B8
NRF_1MBIT[3]      0x0BC
NRF_1MBIT[4]      0x0C0
BLE_1MBIT[0]      0x0EC
BLE_1MBIT[1]      0x0F0
BLE_1MBIT[2]      0x0F4
BLE_1MBIT[3]      0x0F8
BLE_1MBIT[4]      0x0FC
*/
static const uint32_t ficr_content[64] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000400,
        0x00000100, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000002, 0x00002000,
        0x00002000, 0x00002000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000003,
        0x12345678, 0x9ABCDEF1, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static uint64_t ficr_read(void *opaque, hwaddr offset, unsigned int size)
{
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

static uint64_t io_read(void *opaque, hwaddr offset, unsigned int size)
{
    Nrf51NVMCState *s = NRF51_NVMC(opaque);
    uint64_t r = 0;

    switch (offset) {
    case NRF51_NVMC_READY:
        r = NRF51_NVMC_READY_READY;
        break;
    case NRF51_NVMC_CONFIG:
        r = s->state.config;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad read offset 0x%" HWADDR_PRIx "\n", __func__, offset);
    }

    return r;
}

static void io_write(void *opaque, hwaddr offset, uint64_t value,
        unsigned int size)
{
    Nrf51NVMCState *s = NRF51_NVMC(opaque);

    switch (offset) {
    case NRF51_NVMC_CONFIG:
        s->state.config = value & NRF51_NVMC_CONFIG_MASK;
        break;
    case NRF51_NVMC_ERASEPCR0:
    case NRF51_NVMC_ERASEPCR1:
        value &= ~(s->page_size - 1);
        if (value < (s->code_size * s->page_size)) {
            address_space_write(&s->as, value, MEMTXATTRS_UNSPECIFIED,
                    s->empty_page, s->page_size);
        }
        break;
    case NRF51_NVMC_ERASEALL:
        if (value == NRF51_NVMC_ERASE) {
            for (uint32_t i = 0; i < s->code_size; i++) {
                address_space_write(&s->as, i * s->page_size,
                MEMTXATTRS_UNSPECIFIED, s->empty_page, s->page_size);
            }
        }
        break;
    case NRF51_NVMC_ERASEUICR:
        if (value == NRF51_NVMC_ERASE) {
            address_space_write(&s->as, NRF51_UICR_OFFSET,
            MEMTXATTRS_UNSPECIFIED, s->empty_page, NRF51_UICR_SIZE);
        }
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad write offset 0x%" HWADDR_PRIx "\n", __func__, offset);
    }
}

static const MemoryRegionOps io_ops = {
        .read = io_read,
        .write = io_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
};

static void nrf51_nvmc_init(Object *obj)
{
    Nrf51NVMCState *s = NRF51_NVMC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->mmio, obj, &io_ops, s,
                          TYPE_NRF51_NVMC, NRF51_NVMC_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);

    memory_region_init_io(&s->ficr, NULL, &ficr_ops, NULL, "nrf51_soc.ficr",
            NRF51_FICR_SIZE);
    memory_region_set_readonly(&s->ficr, true);
    sysbus_init_mmio(sbd, &s->ficr);
}

static void nrf51_nvmc_realize(DeviceState *dev, Error **errp)
{
    Nrf51NVMCState *s = NRF51_NVMC(dev);

    if (!s->mr) {
        error_setg(errp, "memory property was not set");
        return;
    }

    if (s->page_size < NRF51_UICR_SIZE) {
        error_setg(errp, "page size too small");
        return;
    }

    s->empty_page = g_malloc(s->page_size);
    memset(s->empty_page, 0xFF, s->page_size);

    address_space_init(&s->as, s->mr, "system-memory");
}

static void nrf51_nvmc_unrealize(DeviceState *dev, Error **errp)
{
    Nrf51NVMCState *s = NRF51_NVMC(dev);

    g_free(s->empty_page);
    s->empty_page = NULL;

}

static Property nrf51_nvmc_properties[] = {
    DEFINE_PROP_UINT16("page_size", Nrf51NVMCState, page_size, 0x400),
    DEFINE_PROP_UINT32("code_size", Nrf51NVMCState, code_size, 0x100),
    DEFINE_PROP_LINK("memory", Nrf51NVMCState, mr, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static void nrf51_nvmc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = nrf51_nvmc_properties;
    dc->realize = nrf51_nvmc_realize;
    dc->unrealize = nrf51_nvmc_unrealize;
}

static const TypeInfo nrf51_nvmc_info = {
    .name = TYPE_NRF51_NVMC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Nrf51NVMCState),
    .instance_init = nrf51_nvmc_init,
    .class_init = nrf51_nvmc_class_init
};

static void nrf51_nvmc_register_types(void)
{
    type_register_static(&nrf51_nvmc_info);
}

type_init(nrf51_nvmc_register_types)
