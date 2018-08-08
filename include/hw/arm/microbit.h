/*
 * BBC micro:bit machine
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */
#ifndef MICROBIT_H
#define MICROBIT_H

#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/arm/nrf51_soc.h"
#include "hw/display/led_matrix.h"

#define TYPE_MICROBIT_MACHINE       MACHINE_TYPE_NAME("microbit")
#define MICROBIT_MACHINE(obj) \
    OBJECT_CHECK(MicrobitMachineState, (obj), TYPE_MICROBIT_MACHINE)

typedef struct MicrobitMachineState {
    /*< private >*/
    MachineState parent_obj;

    NRF51State nrf51;
    LEDMatrixState matrix;
} MicrobitMachineState;

#endif
