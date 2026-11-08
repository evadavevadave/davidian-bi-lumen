/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Values and helpers shared by every part of the model. The circuit runs on +/-15 V, so
 * audio voltages are relative to ground. Part numbers refer to the schematic
 * dual phaser clone by f.aiseur, V002, Rev 002, 2025-01-25. */
#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <math.h>

#define SUPPLY 15.0 /* +VDC and -VDC. The 12 ohm feed resistors drop almost nothing. */

/* RC4558 output swing on +/-15 V into the loads here. Datasheet typical is +/-14 V into 10k.
 * Estimate, not from the schematic. */
#define OPAMP_SAT 13.5

/* Linear up to 85% of the swing, then rounds off into it. */
static inline double opamp_limit(double v)
{
    const double knee = 0.85 * OPAMP_SAT, room = OPAMP_SAT - knee;
    if (v > knee)
        return knee + room * tanh((v - knee) / room);
    if (v < -knee)
        return -knee - room * tanh((-v - knee) / room);
    return v;
}

static inline double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Two resistors in parallel. */
static inline double par(double a, double b) { return a * b / (a + b); }

#endif
