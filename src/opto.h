/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The light path: U11B/Q2 (U11A/Q3 for Phasor B) driving the PM3 (PM4) LED, and the LDRs
 * that LED lights.
 *
 * Driver: CHA-MOD -R67 47k- (+) and the Center trim -R82 47k- (+), with R73 3k9 to ground.
 * Q2 follows the op amp, its emitter feeds the LED, and the LED current returns through
 * TR5 and R72 220R. That sense voltage comes back to (-) through R71 220k, with R69 68k and
 * R68 10k + D4 to ground. D4 lowers the feedback once it conducts, so the LED current
 * rises faster than the control voltage at the top of the sweep.
 *
 * Center trim (TR3, TR4): both 10k trims sit in parallel between R81 2k2 from +15 V and
 * R70 2k2 to ground, so their wipers span 3.51 V to 11.49 V. */
#ifndef OPTO_H
#define OPTO_H

#include "circuit.h"

#define DRV_R_MOD   47e3  /* R67, R74 */
#define DRV_R_OFF   47e3  /* R82, R77 */
#define DRV_R_GND   3.9e3 /* R73, R80 */
#define DRV_R71     220e3
#define DRV_R69     68e3
#define DRV_R68     10e3
#define DRV_R72     220.0
#define DRV_TR5     500.0 /* TR5, TR6 "Q bias" (1k). Mid-travel; estimate */
#define DRV_TRIM_LO (SUPPLY * 2.2e3 / 9.4e3)          /* 3.51 V */
#define DRV_TRIM_HI (SUPPLY * (2.2e3 + 5e3) / 9.4e3)  /* 11.49 V */

/* 1N4148 (D4): SPICE-model values. */
#define D_IS 2.52e-9
#define D_NVT (1.752 * 0.025852)

/* Highest LED cathode voltage before U11's output reaches its swing: 13.5 V less Q2's
 * base-emitter drop and the LED's forward voltage. LED colour isn't given; 2.0 V assumed. */
#define DRV_VBE   0.65
#define LED_VF    2.0
#define DRV_VS_MAX (OPAMP_SAT - DRV_VBE - LED_VF)

/* Photocell model. CdS resistance follows illumination as R = R_REF * (I / 1 mA)^-GAMMA.
 * Nothing on the schematic gives the LED/LDR coupling, so these are estimates chosen to put
 * the sweep in a typical phaser range; see README. */
#define LDR_R_REF  25e3   /* ohms at 1 mA LED current */
#define LDR_GAMMA  0.75
#define LDR_R_DARK 2e6
#define LDR_T_ATTACK  0.005 /* resistance falling (light rising) */
#define LDR_T_RELEASE 0.025 /* resistance rising (light falling) */

/* Bypass LED (LDR-DRIVE-A1): R134 1k8 from ground to -15 V when the footswitch closes. */
#define BYPASS_LED_I ((SUPPLY - LED_VF) / 1.8e3)

#define DRV_LUT_N   2048
#define DRV_LUT_MAX 1.5 /* volts at U11's + input; the driver saturates well before this */

typedef struct {
    double vs[DRV_LUT_N + 1]; /* sense voltage for an input voltage, before saturation */
} DriverLut;

/* D4 with R68 in series, at v across the pair: solve for the diode voltage. */
static inline double d4_current(double v)
{
    double vd = v < 0.6 ? v : 0.6;
    int i;
    if (v <= 0.0)
        return 0.0;
    for (i = 0; i < 60; ++i) {
        const double e = exp(vd / D_NVT);
        const double f = D_IS * (e - 1.0) - (v - vd) / DRV_R68;
        const double df = D_IS * e / D_NVT + 1.0 / DRV_R68;
        double next = vd - f / df;
        if (next > v) next = 0.5 * (vd + v);
        if (next < 0.0) next = 0.5 * vd;
        if (fabs(next - vd) < 1e-12) { vd = next; break; }
        vd = next;
    }
    return (v - vd) / DRV_R68;
}

static inline void driver_lut_init(DriverLut* t)
{
    int i;
    for (i = 0; i <= DRV_LUT_N; ++i) {
        const double v = DRV_LUT_MAX * i / DRV_LUT_N;
        t->vs[i] = v + DRV_R71 * (v / DRV_R69 + d4_current(v));
    }
}

/* Voltage at U11's + input. vm/rm is the modulation source's Thevenin equivalent at CHA-MOD,
 * voff the Center trim's wiper. */
static inline double driver_vplus(double vm, double rm, double voff)
{
    const double gm = 1.0 / (rm + DRV_R_MOD), go = 1.0 / DRV_R_OFF;
    return (vm * gm + voff * go) / (gm + go + 1.0 / DRV_R_GND);
}

/* LED current in amps for U11's + input voltage. */
static inline double driver_led_current(const DriverLut* t, double v)
{
    double vs, x, f;
    int i;
    if (v <= 0.0)
        return 0.0;
    x = v / DRV_LUT_MAX * DRV_LUT_N;
    if (x >= DRV_LUT_N) {
        vs = DRV_VS_MAX;
    } else {
        i = (int)x;
        f = x - i;
        vs = t->vs[i] + f * (t->vs[i + 1] - t->vs[i]);
        if (vs > DRV_VS_MAX)
            vs = DRV_VS_MAX;
    }
    /* Emitter current: through TR5 + R72, plus what R71 carries back to (-). */
    return vs / (DRV_R72 + DRV_TR5) + (vs - v) / DRV_R71;
}

static inline double trim_voltage(double position)
{
    return DRV_TRIM_LO + clampd(position, 0.0, 1.0) * (DRV_TRIM_HI - DRV_TRIM_LO);
}

/* ---- Photocell ---- */

typedef struct {
    double lg;       /* log of the conductance */
    double k_attack;
    double k_release;
} Ldr;

static inline double ldr_target_log(double i_led)
{
    double g = 1.0 / LDR_R_DARK;
    if (i_led > 0.0)
        g += pow(i_led / 1e-3, LDR_GAMMA) / LDR_R_REF;
    return log(g);
}

static inline void ldr_init(Ldr* l, double fs, double i_led)
{
    l->k_attack = 1.0 - exp(-1.0 / (LDR_T_ATTACK * fs));
    l->k_release = 1.0 - exp(-1.0 / (LDR_T_RELEASE * fs));
    l->lg = ldr_target_log(i_led);
}

static inline void ldr_set(Ldr* l, double i_led) { l->lg = ldr_target_log(i_led); }

/* Advances one sample and returns the conductance in siemens. */
static inline double ldr_tick(Ldr* l, double i_led)
{
    const double t = ldr_target_log(i_led);
    l->lg += (t > l->lg ? l->k_attack : l->k_release) * (t - l->lg);
    return exp(l->lg);
}

/* Resistance of an LDR held at a steady LED current. */
static inline double ldr_steady_r(double i_led) { return exp(-ldr_target_log(i_led)); }

#endif
