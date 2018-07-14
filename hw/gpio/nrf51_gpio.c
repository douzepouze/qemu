/*
 * nRF51 System-on-Chip general purpose input/output register definition
 *
 * Reference Manual: http://infocenter.nordicsemi.com/pdf/nRF51_RM_v3.0.pdf
 * Product Spec: http://infocenter.nordicsemi.com/pdf/nRF51822_PS_v3.1.pdf
 *
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/gpio/nrf51_gpio.h"
#include "trace.h"

#define NRF51_GPIO_SIZE 0x1000

#define NRF51_GPIO_REG_OUT          0x504
#define NRF51_GPIO_REG_OUTSET       0x508
#define NRF51_GPIO_REG_OUTCLR       0x50C
#define NRF51_GPIO_REG_IN           0x510
#define NRF51_GPIO_REG_DIR          0x514
#define NRF51_GPIO_REG_DIRSET       0x518
#define NRF51_GPIO_REG_DIRCLR       0x51C
#define NRF51_GPIO_REG_CNF_START    0x700
#define NRF51_GPIO_REG_CNF_END      0x77F

#define GPIO_PULLDOWN 1
#define GPIO_PULLUP 3

/**
 * Check if the output driver is connected to the direction switch
 * given the current configuration and logic level.
 * It is not differentiated between standard and "high"(-power) drive modes.
 */
static bool is_connected(uint32_t config, uint32_t level)
{
    bool state;
    uint32_t drive_config = extract32(config, 8, 3);

    switch (drive_config) {
    case 0 ... 3:
        state = true;
        break;
    case 4 ... 5:
        state = level != 0;
        break;
    case 6 ... 7:
        state = level == 0;
        break;
    }

    return state;
}

static void update_output_irq(Nrf51GPIOState *s, size_t i,
                              bool connected, bool level)
{
    int64_t irq_level = connected ? level : -1;
    bool old_connected = extract32(s->old_out_connected, i, 1);
    bool old_level = extract32(s->old_out, i, 1);

    if ((old_connected != connected) || (old_level != level)) {
        qemu_set_irq(s->output[i], irq_level);
        trace_nrf51_gpio_update_output_irq(i, irq_level);
    }

    s->old_out = deposit32(s->old_out, i, 1, level);
    s->old_out_connected = deposit32(s->old_out_connected, i, 1, connected);
}

static void update_state(Nrf51GPIOState *s)
{
    uint32_t pull;
    bool connected_out, dir, connected_in, out, input;

    for (size_t i = 0; i < NRF51_GPIO_PINS; i++) {
        pull = extract32(s->cnf[i], 2, 2);
        dir = extract32(s->cnf[i], 0, 1);
        connected_in = extract32(s->in_mask, i, 1);
        out = extract32(s->out, i, 1);
        input = !extract32(s->cnf[i], 1, 1);
        connected_out = is_connected(s->cnf[i], out) && dir;

        update_output_irq(s, i, connected_out, out);

        /** Pin both driven externally and internally */
        if (connected_out && connected_in) {
            qemu_log_mask(LOG_GUEST_ERROR, "GPIO pin %zu short circuited\n", i);
        }

        /**
         * Input buffer disconnected from internal/external drives, so
         * pull-up/pull-down becomes relevant
         */
        if (!input || (input && !connected_in && !connected_out)) {
            if (pull == GPIO_PULLDOWN) {
                s->in = deposit32(s->in, i, 1, 0);
            } else if (pull == GPIO_PULLUP) {
                s->in = deposit32(s->in, i, 1, 1);
            }
        }

        /** Self stimulation through internal output driver **/
        if (connected_out && !connected_in && input) {
            s->in = deposit32(s->in, i, 1, out);
        }
    }

}

/**
 * Direction is exposed in both the DIR register and the DIR bit
 * of each PINs CNF configuration register. Reflect bits for pins in DIR
 * to individual pin configuration registers.
 */
static void reflect_dir_bit_in_cnf(Nrf51GPIOState *s)
{
    uint32_t value = s->dir;
    for (size_t i = 0; i < NRF51_GPIO_PINS; i++) {
        s->cnf[i] = (s->cnf[i] & ~(1UL)) | ((value >> i) & 0x01);
    }
}

static uint64_t nrf51_gpio_read(void *opaque, hwaddr offset, unsigned int size)
{
    Nrf51GPIOState *s = NRF51_GPIO(opaque);
    uint64_t r = 0;
    size_t idx;

    switch (offset) {
    case NRF51_GPIO_REG_OUT ... NRF51_GPIO_REG_OUTCLR:
        r = s->out;
        break;

    case NRF51_GPIO_REG_IN:
        r = s->in;
        break;

    case NRF51_GPIO_REG_DIR ... NRF51_GPIO_REG_DIRCLR:
        r = s->dir;
        break;

    case NRF51_GPIO_REG_CNF_START ... NRF51_GPIO_REG_CNF_END:
        idx = (offset - NRF51_GPIO_REG_CNF_START) / 4;
        r = s->cnf[idx];
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad read offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
    }

    trace_nrf51_gpio_read(offset, r);

    return r;
}

static void nrf51_gpio_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned int size)
{
    Nrf51GPIOState *s = NRF51_GPIO(opaque);
    size_t idx;

    trace_nrf51_gpio_write(offset, value);

    switch (offset) {
    case NRF51_GPIO_REG_OUT:
        s->out = value;
        break;

    case NRF51_GPIO_REG_OUTSET:
        s->out |= value;
        break;

    case NRF51_GPIO_REG_OUTCLR:
        s->out &= ~value;
        break;

    case NRF51_GPIO_REG_DIR:
        s->dir = value;
        reflect_dir_bit_in_cnf(s);
        break;

    case NRF51_GPIO_REG_DIRSET:
        s->dir |= value;
        reflect_dir_bit_in_cnf(s);
        break;

    case NRF51_GPIO_REG_DIRCLR:
        s->dir &= ~value;
        reflect_dir_bit_in_cnf(s);
        break;

    case NRF51_GPIO_REG_CNF_START ... NRF51_GPIO_REG_CNF_END:
        idx = (offset - NRF51_GPIO_REG_CNF_START) / 4;
        s->cnf[idx] = value;
        /* direction is exposed in both the DIR register and the DIR bit
         * of each PINs CNF configuration register. */
        s->dir = (s->dir & ~(1UL << idx)) | ((value & 0x01) << idx);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad write offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
    }

    update_state(s);
}

static const MemoryRegionOps gpio_ops = {
    .read =  nrf51_gpio_read,
    .write = nrf51_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

static void nrf51_gpio_set(void *opaque, int line, int value)
{
    Nrf51GPIOState *s = NRF51_GPIO(opaque);

    trace_nrf51_gpio_set(line, value);

    assert(line >= 0 && line < NRF51_GPIO_PINS);

    s->in_mask = deposit32(s->in_mask, line, 1, value >= 0);
    s->in = deposit32(s->in, line, 1, value > 0);

    update_state(s);
}

static void nrf51_gpio_reset(DeviceState *dev)
{
    Nrf51GPIOState *s = NRF51_GPIO(dev);
    size_t i;

    s->out = 0;
    s->old_out = 0;
    s->old_out_connected = 0;
    s->in = 0;
    s->in_mask = 0;
    s->dir = 0;

    for (i = 0; i < NRF51_GPIO_PINS; i++) {
        s->cnf[i] = 0x00000002;
    }
}

static const VMStateDescription vmstate_nrf51_gpio = {
    .name = TYPE_NRF51_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(out, Nrf51GPIOState),
        VMSTATE_UINT32(in, Nrf51GPIOState),
        VMSTATE_UINT32(in_mask, Nrf51GPIOState),
        VMSTATE_UINT32(dir, Nrf51GPIOState),
        VMSTATE_UINT32_ARRAY(cnf, Nrf51GPIOState, NRF51_GPIO_PINS),
        VMSTATE_UINT32(old_out, Nrf51GPIOState),
        VMSTATE_UINT32(old_out_connected, Nrf51GPIOState),
        VMSTATE_END_OF_LIST()
    }
};

static void nrf51_gpio_init(Object *obj)
{
    Nrf51GPIOState *s = NRF51_GPIO(obj);

    memory_region_init_io(&s->mmio, obj, &gpio_ops, s,
            TYPE_NRF51_GPIO, NRF51_GPIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    qdev_init_gpio_in(DEVICE(s), nrf51_gpio_set, NRF51_GPIO_PINS);
    qdev_init_gpio_out(DEVICE(s), s->output, NRF51_GPIO_PINS);
}

static void nrf51_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_nrf51_gpio;
    dc->reset = nrf51_gpio_reset;
    dc->desc = "nRF51 GPIO";
}

static const TypeInfo nrf51_gpio_info = {
    .name = TYPE_NRF51_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Nrf51GPIOState),
    .instance_init = nrf51_gpio_init,
    .class_init = nrf51_gpio_class_init
};

static void nrf51_gpio_register_types(void)
{
    type_register_static(&nrf51_gpio_info);
}

type_init(nrf51_gpio_register_types)
