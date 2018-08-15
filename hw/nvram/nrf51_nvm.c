/*
 * Nordic Semiconductor nRF51 non-volatile memory
 *
 * It provides an interface to erase regions in flash memory.
 * Furthermore it provides the user and factory information registers.
 *
 * Reference Manual: http://infocenter.nordicsemi.com/pdf/nRF51_RM_v3.0.pdf
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
#include "exec/address-spaces.h"
#include "hw/nvram/nrf51_nvm.h"

/* FICR Registers Assignments
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
    return ficr_content[offset / 4];
}

static const MemoryRegionOps ficr_ops = {
    .read = ficr_read,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .impl.unaligned = false,
};

/* UICR Registers Assignments
CLENR0           0x000
RBPCONF          0x004
XTALFREQ         0x008
FWID             0x010
BOOTLOADERADDR   0x014
NRFFW[0]         0x014
NRFFW[1]         0x018
NRFFW[2]         0x01C
NRFFW[3]         0x020
NRFFW[4]         0x024
NRFFW[5]         0x028
NRFFW[6]         0x02C
NRFFW[7]         0x030
NRFFW[8]         0x034
NRFFW[9]         0x038
NRFFW[10]        0x03C
NRFFW[11]        0x040
NRFFW[12]        0x044
NRFFW[13]        0x048
NRFFW[14]        0x04C
NRFHW[0]         0x050
NRFHW[1]         0x054
NRFHW[2]         0x058
NRFHW[3]         0x05C
NRFHW[4]         0x060
NRFHW[5]         0x064
NRFHW[6]         0x068
NRFHW[7]         0x06C
NRFHW[8]         0x070
NRFHW[9]         0x074
NRFHW[10]        0x078
NRFHW[11]        0x07C
CUSTOMER[0]      0x080
CUSTOMER[1]      0x084
CUSTOMER[2]      0x088
CUSTOMER[3]      0x08C
CUSTOMER[4]      0x090
CUSTOMER[5]      0x094
CUSTOMER[6]      0x098
CUSTOMER[7]      0x09C
CUSTOMER[8]      0x0A0
CUSTOMER[9]      0x0A4
CUSTOMER[10]     0x0A8
CUSTOMER[11]     0x0AC
CUSTOMER[12]     0x0B0
CUSTOMER[13]     0x0B4
CUSTOMER[14]     0x0B8
CUSTOMER[15]     0x0BC
CUSTOMER[16]     0x0C0
CUSTOMER[17]     0x0C4
CUSTOMER[18]     0x0C8
CUSTOMER[19]     0x0CC
CUSTOMER[20]     0x0D0
CUSTOMER[21]     0x0D4
CUSTOMER[22]     0x0D8
CUSTOMER[23]     0x0DC
CUSTOMER[24]     0x0E0
CUSTOMER[25]     0x0E4
CUSTOMER[26]     0x0E8
CUSTOMER[27]     0x0EC
CUSTOMER[28]     0x0F0
CUSTOMER[29]     0x0F4
CUSTOMER[30]     0x0F8
CUSTOMER[31]     0x0FC
*/

static const uint32_t uicr_fixture[NRF51_UICR_FIXTURE_SIZE] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static uint64_t uicr_read(void *opaque, hwaddr offset, unsigned int size)
{
    Nrf51NVMState *s = NRF51_NVM(opaque);

    return s->uicr_content[offset / 4];
}

static void uicr_write(void *opaque, hwaddr offset, uint64_t value,
        unsigned int size)
{
    Nrf51NVMState *s = NRF51_NVM(opaque);

    s->uicr_content[offset / 4] = value;
}

static const MemoryRegionOps uicr_ops = {
    .read = uicr_read,
    .write = uicr_write,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .impl.unaligned = false,
};


static uint64_t io_read(void *opaque, hwaddr offset, unsigned int size)
{
    Nrf51NVMState *s = NRF51_NVM(opaque);
    uint64_t r = 0;

    switch (offset) {
    case NRF51_NVMC_READY:
        r = NRF51_NVMC_READY_READY;
        break;
    case NRF51_NVMC_CONFIG:
        r = s->config;
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
    Nrf51NVMState *s = NRF51_NVM(opaque);

    switch (offset) {
    case NRF51_NVMC_CONFIG:
        s->config = value & NRF51_NVMC_CONFIG_MASK;
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
            memset(s->uicr_content, 0xFF, sizeof(s->uicr_content));
        }
        break;
    case NRF51_NVMC_ERASEUICR:
        if (value == NRF51_NVMC_ERASE) {
            memset(s->uicr_content, 0xFF, sizeof(s->uicr_content));
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

static void nrf51_nvm_init(Object *obj)
{
    Nrf51NVMState *s = NRF51_NVM(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->mmio, obj, &io_ops, s, "nrf51_soc.nvmc",
            NRF51_NVMC_SIZE);
    sysbus_init_mmio(sbd, &s->mmio);

    memory_region_init_io(&s->ficr, NULL, &ficr_ops, s, "nrf51_soc.ficr",
            sizeof(ficr_content));
    memory_region_set_readonly(&s->ficr, true);
    sysbus_init_mmio(sbd, &s->ficr);

    memcpy(s->uicr_content, uicr_fixture, sizeof(s->uicr_content));
    memory_region_init_io(&s->uicr, NULL, &uicr_ops, s, "nrf51_soc.uicr",
            sizeof(s->uicr_content));
    sysbus_init_mmio(sbd, &s->uicr);
}

static void nrf51_nvm_realize(DeviceState *dev, Error **errp)
{
    Nrf51NVMState *s = NRF51_NVM(dev);

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

static void nrf51_nvm_unrealize(DeviceState *dev, Error **errp)
{
    Nrf51NVMState *s = NRF51_NVM(dev);

    g_free(s->empty_page);
    s->empty_page = NULL;
}

static Property nrf51_nvm_properties[] = {
    DEFINE_PROP_UINT16("page_size", Nrf51NVMState, page_size, 0x400),
    DEFINE_PROP_UINT32("code_size", Nrf51NVMState, code_size, 0x100),
    DEFINE_PROP_LINK("memory", Nrf51NVMState, mr, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vmstate_nvm = {
    .name = "nrf51_soc.nvm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(uicr_content, Nrf51NVMState,
                NRF51_UICR_FIXTURE_SIZE),
        VMSTATE_UINT32(config, Nrf51NVMState),
        VMSTATE_END_OF_LIST()
    }
};

static void nrf51_nvm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = nrf51_nvm_properties;
    dc->vmsd = &vmstate_nvm;
    dc->realize = nrf51_nvm_realize;
    dc->unrealize = nrf51_nvm_unrealize;
}

static const TypeInfo nrf51_nvm_info = {
    .name = TYPE_NRF51_NVM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Nrf51NVMState),
    .instance_init = nrf51_nvm_init,
    .class_init = nrf51_nvm_class_init
};

static void nrf51_nvm_register_types(void)
{
    type_register_static(&nrf51_nvm_info);
}

type_init(nrf51_nvm_register_types)
