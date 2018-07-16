/*
 * LED Matrix Demultiplexer
 *
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */
#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include "hw/sysbus.h"
#include "qemu/timer.h"
#define TYPE_LED_MATRIX "led_matrix"
#define LED_MATRIX(obj) OBJECT_CHECK(LEDMatrixState, (obj), TYPE_LED_MATRIX)

typedef struct LEDMatrixState {
    SysBusDevice parent_obj;

    QemuConsole *con;
    bool redraw;

    uint8_t num_rows_io;
    uint8_t num_cols_io;
    uint32_t num_matrix_coords;
    int32_t *matrix_coords;
    bool strobe_row;

    QEMUTimer timer;
    int64_t timestamp;
    int64_t regeneration_start;
    int64_t regeneration_period;

    uint64_t row;
    uint64_t col;
    int64_t *led_working_dc; /* Current LED duty cycle acquisition */
    int64_t *led_frame_dc; /* Last complete LED duty cycle acquisition */
} LEDMatrixState;


#endif
