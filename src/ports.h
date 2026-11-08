/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Port indices, shared by the plugin and its GUI. They must match bundle/bi_lumen.ttl. */
#ifndef PORTS_H
#define PORTS_H

#define BL_URI    "urn:davidian:bi-lumen"
#define BL_UI_URI "urn:davidian:bi-lumen#ui"

enum {
    PORT_IN_A, PORT_IN_B, PORT_OUT_A, PORT_OUT_B,
    PORT_RATE1, PORT_SHAPE1, PORT_RATE1_SRC, PORT_RATE2, PORT_SHAPE2,
    PORT_DEPTH_A, PORT_FEEDBACK_A, PORT_DEPTH_B, PORT_FEEDBACK_B,
    PORT_SWEEP_A, PORT_SWEEP_B, PORT_SYNC, PORT_B_INPUT,
    PORT_ON_A, PORT_ON_B, PORT_PEDAL, PORT_CENTER_A, PORT_CENTER_B,
    PORT_VOLTS, PORT_OUTPUT, PORT_ENABLED,
    PORT_LED_A, PORT_LED_B, /* outputs: opto LED brightness 0-1, for the GUI */
    PORT_COUNT
};

/* LED current shown as full brightness on the GUI's opto lamps. */
#define LED_FULL_SCALE 12e-3

#endif
