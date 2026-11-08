/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Sweep generators and the routing to CHA-MOD and CHB-MOD.
 *
 * Sweep 1 is a voltage-controlled triangle/square oscillator:
 *   rate current -> U13B (R108 39k + D12) and U13A (x-1) give +/- copies of 39k * I,
 *   R115/R117 33k and the D8-D11 bridge steer one of them into integrator U12B (C43 470n),
 *   U12A is a Schmitt trigger (R110 47k from the triangle, R111 68k from its own output).
 *   Rate current: VR1 (B25k from +15 V) through R85 47k, or the pedal through R91/R88/R86,
 *   plus TR8 (SWEEP1 OFFSET) through R112 390k, which sets the slowest rate.
 *   One SW3 position feeds the triangle through R118 47k; the other feeds the square through
 *   R119 120k with C42 470n, which rounds its edges. Either then loads into R120 47k + TR9
 *   (SWEEP1 VOL) and U16A buffers it.
 *
 * Sweep 2 is a fixed-current version: U15A Schmitt (R98 68k, R99 47k), U15B integrator
 * (C44 470n) fed from VR3 (A25k across the square, R101 120R at the bottom) through R102 39k.
 * Its square is rounded by R100 10k and C85 2.2u. TR10 (SWEEP2 LEVEL TRIM) is in series
 * with its output.
 *
 * Pedal path: EXPRESSION-INPUT -> U14B buffer -> R90/R94 39k halve it -> U14A, gain
 * 1 + R95/(R97 + TR7), with TR7 (EXPRESSION OFFSET) pulling R97 towards a negative voltage. */
#ifndef SWEEP_H
#define SWEEP_H

#include "circuit.h"

#define SW_SCHMITT_TH (OPAMP_SAT * 47e3 / 68e3) /* triangle peak, both generators */
#define SW_DIODE      0.6

/* Sweep 1 */
#define S1_C43   470e-9
#define S1_R85   47e3
#define S1_VR1   25e3
#define S1_R108  39e3
#define S1_RBR   33e3   /* R115, R117 */
#define S1_R112  390e3
#define S1_TR8_V (SUPPLY * 10e3 / 130e3) /* top of TR8, from R113 120k */
#define S1_TR8   0.5    /* TR8 position; gives about 0.1 Hz at the bottom of VR1 */
#define S1_R118  47e3
#define S1_R119  120e3
#define S1_C42   470e-9
#define S1_R120  47e3
#define S1_TR9   11e3   /* TR9 SWEEP1 VOL, mid-travel of 22k */
#define S1_R86   10e3
#define S1_R88   130.0
#define S1_R91   4.7e3

/* Sweep 2 */
#define S2_C44   470e-9
#define S2_VR3   25e3
#define S2_R101  120.0
#define S2_R102  39e3
#define S2_R100  10e3
#define S2_C85   2.2e-6
#define S2_TR10  8.5e3  /* TR10, set so Sweep 2 matches Sweep 1's level; estimate */

/* Pedal: the original expression pedal sends 0-5 V (Polyphony, Feb 1978). */
#define PEDAL_V  5.0
#define CV_R92   1e6
#define CV_R93   3.9e3
#define CV_R95   39e3
#define CV_R97   36e3
#define CV_TR7   0.0    /* EXPRESSION OFFSET at zero */

/* Depth pots: VR2/VR4 B10k with R121/R103 560R below. */
#define DEPTH_POT 10e3
#define DEPTH_R   560.0
#define SYNC_R105 39e3  /* U16B input resistor, always on VR4's wiper */
#define SYNC_R107 47e3  /* added in Reverse to keep VR4's load the same */

typedef struct {
    double dt;
    /* Sweep 1 */
    double tri1, sq1, c42;
    double i1; /* integrator current */
    /* Sweep 2 */
    double tri2, sq2, c85;
    double vr3; /* VR3 position */
} Sweep;

/* ---- Sweep 1 rate ---- */

static inline double s1_offset_current(void) { return S1_TR8 * S1_TR8_V / S1_R112; }

/* Integrator current from U13B's input current, through the two inverters and the bridge. */
static inline double s1_integrator_current(double i_in)
{
    double v = S1_R108 * i_in + SW_DIODE;
    if (v > OPAMP_SAT)
        v = OPAMP_SAT;
    return (v - SW_DIODE) / S1_RBR;
}

static inline double s1_knob_current(double x)
{
    const double rth = x * (1.0 - x) * S1_VR1;
    return SUPPLY * x / (S1_R85 + rth) + s1_offset_current();
}

static inline double s1_pedal_current(double pedal)
{
    const double vexp = PEDAL_V * clampd(pedal, 0.0, 1.0) * CV_R92 / (CV_R92 + CV_R93);
    const double vth = vexp * S1_R88 / (S1_R91 + S1_R88);
    return vth / (S1_R86 + par(S1_R91, S1_R88)) + s1_offset_current();
}

static inline double sweep_hz(double i_integrator, double c)
{
    return i_integrator / (4.0 * c * SW_SCHMITT_TH);
}

/* VR1 position for a rate in Hz. Below the slowest rate it returns 0. */
static inline double s1_knob_for_hz(double hz)
{
    double lo = 0.0, hi = 1.0;
    int i;
    for (i = 0; i < 50; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (sweep_hz(s1_integrator_current(s1_knob_current(mid)), S1_C43) < hz)
            lo = mid;
        else
            hi = mid;
    }
    return 0.5 * (lo + hi);
}

/* ---- Sweep 2 rate ---- */

/* Audio taper: 10% of the track at mid-travel. */
static inline double a_taper(double x) { return (pow(81.0, x) - 1.0) / 80.0; }

static inline double s2_current(double x, double vsq)
{
    const double rb = a_taper(x) * S2_VR3 + S2_R101;
    const double rt = S2_VR3 - a_taper(x) * S2_VR3;
    const double vw = vsq * rb / (rb + rt);
    return vw / (S2_R102 + par(rt + 1e-3, rb));
}

static inline double s2_knob_for_hz(double hz)
{
    double lo = 0.0, hi = 1.0;
    int i;
    for (i = 0; i < 50; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (sweep_hz(s2_current(mid, OPAMP_SAT), S2_C44) < hz)
            lo = mid;
        else
            hi = mid;
    }
    return 0.5 * (lo + hi);
}

/* ---- Generators ---- */

static inline void sweep_reset(Sweep* s)
{
    s->tri1 = s->tri2 = 0.0;
    s->sq1 = s->sq2 = OPAMP_SAT;
    s->c42 = s->c85 = 0.0;
}

static inline void sweep_init(Sweep* s, double fs)
{
    s->dt = 1.0 / fs;
    s->i1 = s1_integrator_current(s1_knob_current(0.0));
    s->vr3 = 0.0;
    sweep_reset(s);
}

/* Integrator plus Schmitt trigger. The square is high while the triangle falls; a switch
 * point overshot within the sample is reflected back, which keeps the rate exact. */
static inline void tri_step(double* tri, double* sq, double slope)
{
    *tri += *sq > 0.0 ? -slope : slope;
    if (*sq > 0.0 && *tri <= -SW_SCHMITT_TH) {
        *tri = -2.0 * SW_SCHMITT_TH - *tri;
        *sq = -OPAMP_SAT;
    } else if (*sq < 0.0 && *tri >= SW_SCHMITT_TH) {
        *tri = 2.0 * SW_SCHMITT_TH - *tri;
        *sq = OPAMP_SAT;
    }
}

/* RC node driven from v through r, loaded by rl to ground (rl <= 0 means unloaded). */
static inline double rc_step(double node, double v, double r, double rl, double c, double dt)
{
    double target = v, rt = r;
    if (rl > 0.0) {
        target = v * rl / (r + rl);
        rt = par(r, rl);
    }
    return target + (node - target) * exp(-dt / (rt * c));
}

/* Sweep 1 output (U16A) for SW3: square = 0 picks the triangle, 1 the rounded square. */
static inline double sweep1_tick(Sweep* s, int square)
{
    const double rl = S1_R120 + S1_TR9;
    tri_step(&s->tri1, &s->sq1, s->i1 * s->dt / S1_C43);
    s->c42 = rc_step(s->c42, s->sq1, S1_R119, square ? rl : 0.0, S1_C42, s->dt);
    return square ? s->c42 : s->tri1 * rl / (S1_R118 + rl);
}

/* Sweep 2 at SW4, before TR10. rl is what it drives (TR10 plus VR4) when selected. */
static inline double sweep2_tick(Sweep* s, int square, double rl)
{
    tri_step(&s->tri2, &s->sq2, s2_current(s->vr3, OPAMP_SAT) * s->dt / S2_C44);
    s->c85 = rc_step(s->c85, s->sq2, S2_R100, square ? rl : 0.0, S2_C85, s->dt);
    return square ? s->c85 : s->tri2;
}

/* U14A output for a pedal position 0-1. */
static inline double pedal_cv(double pedal)
{
    const double vexp = PEDAL_V * clampd(pedal, 0.0, 1.0) * CV_R92 / (CV_R92 + CV_R93);
    const double vt7 = -SUPPLY * CV_TR7 * 10e3 / 57e3;
    const double rg = CV_R97 + par(CV_TR7 * 10e3 + 1e-3, (1.0 - CV_TR7) * 10e3 + 47e3);
    return 0.5 * vexp * (1.0 + CV_R95 / rg) - vt7 * CV_R95 / rg;
}

/* ---- Depth pots ---- */

typedef struct { double v, r; } Thevenin;

/* Wiper of a depth pot at position k fed from v through rs, loaded by rl to ground
 * (rl <= 0 for no load besides what follows). */
static inline Thevenin depth_wiper(double v, double rs, double k, double rl)
{
    Thevenin t;
    const double rt = rs + (1.0 - k) * DEPTH_POT + 1e-3;
    const double rb = k * DEPTH_POT + DEPTH_R;
    t.v = v * rb / (rt + rb);
    t.r = par(rt, rb);
    if (rl > 0.0) {
        t.v *= rl / (t.r + rl);
        t.r = par(t.r, rl);
    }
    return t;
}

#endif
