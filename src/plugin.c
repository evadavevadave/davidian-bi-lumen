/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* LV2 entry points, controls and routing for the Bi-Lumen. */
#include <stdlib.h>

#include "circuit.h"
#include "lv2_abi.h"
#include "opto.h"
#include "phasor.h"
#include "sweep.h"

#if defined(_M_X64) || defined(__SSE2__)
#    include <xmmintrin.h>
#    define BL_HAS_CSR 1
#endif

#include "ports.h"

#define SMOOTH_SECONDS 0.01
#define BYPASS_SECONDS 0.02

/* Smoothed controls. */
enum { SM_VOLTS, SM_GAIN, SM_PEDAL, SM_DEPTH_A, SM_DEPTH_B, SM_FB_A, SM_FB_B, SM_CENTER_A, SM_CENTER_B, SM_COUNT };

typedef struct {
    const float* port[PORT_COUNT];

    double fs;
    double smooth;
    double fade_step;
    double fade;
    double sm[SM_COUNT];
    double tr1; /* TR1/TR2, set so the wet and dry mix resistors match */

    float  last_rate1, last_rate2;
    double knob1;
    int    pedal_rate; /* Sweep 1 rate from the pedal instead of VR1 */

    DriverLut lut;
    Sweep  sweep;
    Phasor a, b;
    Ldr    ldr_a, ldr_b, wet_a, wet_b;
} Plugin;

static double ctl(const Plugin* p, int port, double lo, double hi, double fallback)
{
    const float* v = p->port[port];
    if (!v || !isfinite(*v))
        return fallback;
    return clampd(*v, lo, hi);
}

static int sel(const Plugin* p, int port, int max)
{
    return (int)(ctl(p, port, 0.0, max, 0.0) + 0.5);
}

static void targets(const Plugin* p, double* t)
{
    t[SM_VOLTS] = ctl(p, PORT_VOLTS, 0.1, 10.0, 1.0);
    t[SM_GAIN] = pow(10.0, ctl(p, PORT_OUTPUT, -24.0, 12.0, 0.0) / 20.0);
    t[SM_PEDAL] = ctl(p, PORT_PEDAL, 0.0, 1.0, 0.5);
    t[SM_DEPTH_A] = ctl(p, PORT_DEPTH_A, 0.0, 1.0, 0.8);
    t[SM_DEPTH_B] = ctl(p, PORT_DEPTH_B, 0.0, 1.0, 0.8);
    t[SM_FB_A] = ctl(p, PORT_FEEDBACK_A, 0.0, 1.0, 0.0);
    t[SM_FB_B] = ctl(p, PORT_FEEDBACK_B, 0.0, 1.0, 0.0);
    t[SM_CENTER_A] = ctl(p, PORT_CENTER_A, 0.0, 1.0, 0.15);
    t[SM_CENTER_B] = ctl(p, PORT_CENTER_B, 0.0, 1.0, 0.15);
}

static void update_rates(Plugin* p, int force)
{
    const float r1 = (float)ctl(p, PORT_RATE1, 0.1, 18.0, 0.5);
    const float r2 = (float)ctl(p, PORT_RATE2, 0.1, 18.0, 0.5);
    if (force || r1 != p->last_rate1) {
        p->knob1 = s1_knob_for_hz(r1);
        p->last_rate1 = r1;
    }
    if (force || r2 != p->last_rate2) {
        p->sweep.vr3 = s2_knob_for_hz(r2);
        p->last_rate2 = r2;
    }
    p->pedal_rate = sel(p, PORT_RATE1_SRC, 1);
    p->sweep.i1 = s1_integrator_current(p->pedal_rate ? s1_pedal_current(p->sm[SM_PEDAL])
                                                       : s1_knob_current(p->knob1));
}

static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double sample_rate,
                              const char* bundle_path, const LV2_Feature* const* features)
{
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    (void)descriptor;
    (void)bundle_path;
    (void)features;
    if (!p)
        return NULL;
    p->fs = sample_rate;
    p->smooth = 1.0 - exp(-1.0 / (SMOOTH_SECONDS * sample_rate));
    p->fade_step = 1.0 / (BYPASS_SECONDS * sample_rate);
    p->tr1 = PH_R50 - PH_R49 - ldr_steady_r(BYPASS_LED_I);
    if (p->tr1 < 0.0)
        p->tr1 = 0.0;
    driver_lut_init(&p->lut);
    sweep_init(&p->sweep, sample_rate);
    phasor_init(&p->a, sample_rate);
    phasor_init(&p->b, sample_rate);
    ldr_init(&p->ldr_a, sample_rate, 0.0);
    ldr_init(&p->ldr_b, sample_rate, 0.0);
    ldr_init(&p->wet_a, sample_rate, BYPASS_LED_I);
    ldr_init(&p->wet_b, sample_rate, BYPASS_LED_I);
    return p;
}

static void connect_port(LV2_Handle h, uint32_t port, void* data)
{
    Plugin* p = (Plugin*)h;
    if (port < PORT_COUNT)
        p->port[port] = (const float*)data;
}

/* Modulation voltages at CHA-MOD and CHB-MOD as Thevenin sources. */
typedef struct { Thevenin a, b; } Mods;

static Mods route(Plugin* p, int shape1, int shape2)
{
    Mods m;
    const double gen1 = sweep1_tick(&p->sweep, shape1);
    const double cv = pedal_cv(p->sm[SM_PEDAL]);
    const int src_b = sel(p, PORT_SWEEP_B, 2);
    const int rev = sel(p, PORT_SYNC, 1);
    const double rl2 = S2_TR10 + DEPTH_POT + DEPTH_R;
    const double gen2 = sweep2_tick(&p->sweep, shape2, src_b == 1 ? rl2 : 0.0);
    double vb, rs = 0.0;

    m.a = depth_wiper(sel(p, PORT_SWEEP_A, 1) ? cv : gen1, 0.0, p->sm[SM_DEPTH_A], 0.0);

    if (src_b == 0) {
        vb = gen1;
    } else if (src_b == 1) {
        vb = gen2;
        rs = S2_TR10;
    } else {
        vb = cv;
    }
    if (rev) {
        /* U16B inverts the wiper (R105/R104 39k); R107 keeps the wiper's load matched. */
        m.b = depth_wiper(vb, rs, p->sm[SM_DEPTH_B], par(SYNC_R105, SYNC_R107));
        m.b.v = -m.b.v;
        m.b.r = 0.0;
    } else {
        m.b = depth_wiper(vb, rs, p->sm[SM_DEPTH_B], SYNC_R105);
    }
    return m;
}

static double led_current(const Plugin* p, Thevenin m, double center)
{
    return driver_led_current(&p->lut, driver_vplus(m.v, m.r, trim_voltage(center)));
}

static void activate(LV2_Handle h)
{
    Plugin* p = (Plugin*)h;
    Mods m;
    targets(p, p->sm);
    sweep_reset(&p->sweep);
    update_rates(p, 1);
    phasor_reset(&p->a);
    phasor_reset(&p->b);
    phasor_set_feedback(&p->a, p->sm[SM_FB_A]);
    phasor_set_feedback(&p->b, p->sm[SM_FB_B]);
    /* Settle the rounded-square nodes and start the cells where the sweep starts. */
    p->sweep.c42 = p->sweep.sq1 * (S1_R120 + S1_TR9) / (S1_R119 + S1_R120 + S1_TR9);
    p->sweep.c85 = p->sweep.sq2;
    m = route(p, sel(p, PORT_SHAPE1, 1), sel(p, PORT_SHAPE2, 1));
    ldr_set(&p->ldr_a, led_current(p, m.a, p->sm[SM_CENTER_A]));
    ldr_set(&p->ldr_b, led_current(p, m.b, p->sm[SM_CENTER_B]));
    ldr_set(&p->wet_a, ctl(p, PORT_ON_A, 0, 1, 1) > 0.5 ? BYPASS_LED_I : 0.0);
    ldr_set(&p->wet_b, ctl(p, PORT_ON_B, 0, 1, 1) > 0.5 ? BYPASS_LED_I : 0.0);
    p->fade = ctl(p, PORT_ENABLED, 0, 1, 1) > 0.5 ? 1.0 : 0.0;
}

static void run(LV2_Handle h, uint32_t n)
{
    Plugin* p = (Plugin*)h;
    const float* in_a = p->port[PORT_IN_A];
    const float* in_b = p->port[PORT_IN_B];
    float* out_a = (float*)p->port[PORT_OUT_A];
    float* out_b = (float*)p->port[PORT_OUT_B];
    const int shape1 = sel(p, PORT_SHAPE1, 1), shape2 = sel(p, PORT_SHAPE2, 1);
    const int b_input = sel(p, PORT_B_INPUT, 2);
    const double i_wet_a = ctl(p, PORT_ON_A, 0, 1, 1) > 0.5 ? BYPASS_LED_I : 0.0;
    const double i_wet_b = ctl(p, PORT_ON_B, 0, 1, 1) > 0.5 ? BYPASS_LED_I : 0.0;
    const double fade_to = ctl(p, PORT_ENABLED, 0, 1, 1) > 0.5 ? 1.0 : 0.0;
    double t[SM_COUNT], led_a = 0.0, led_b = 0.0;
    uint32_t i;
    int k;
#ifdef BL_HAS_CSR
    unsigned int csr;
#endif

    if (!in_a || !in_b || !out_a || !out_b)
        return;
#ifdef BL_HAS_CSR
    csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8040); /* flush denormals so decaying signals don't slow the CPU */
#endif
    targets(p, t);
    update_rates(p, 0);

    for (i = 0; i < n; ++i) {
        double xa = in_a[i], xb = in_b[i], ya, yb, va, vb, ga, gb;
        Mods m;
        if (!isfinite(xa)) xa = 0.0;
        if (!isfinite(xb)) xb = 0.0;
        for (k = 0; k < SM_COUNT; ++k)
            p->sm[k] += p->smooth * (t[k] - p->sm[k]);
        if (p->fade < fade_to)
            p->fade = fade_to < p->fade + p->fade_step ? fade_to : p->fade + p->fade_step;
        else if (p->fade > fade_to)
            p->fade = fade_to > p->fade - p->fade_step ? fade_to : p->fade - p->fade_step;
        phasor_set_feedback(&p->a, p->sm[SM_FB_A]);
        phasor_set_feedback(&p->b, p->sm[SM_FB_B]);
        if (p->pedal_rate)
            p->sweep.i1 = s1_integrator_current(s1_pedal_current(p->sm[SM_PEDAL]));

        /* The circuit keeps running while the host bypasses it, so the sweeps don't stop. */
        m = route(p, shape1, shape2);
        led_a = led_current(p, m.a, p->sm[SM_CENTER_A]);
        led_b = led_current(p, m.b, p->sm[SM_CENTER_B]);
        ga = ldr_tick(&p->ldr_a, led_a);
        gb = ldr_tick(&p->ldr_b, led_b);

        va = p->sm[SM_VOLTS];
        ya = phasor_tick(&p->a, xa * va, ga, 1.0 / ldr_tick(&p->wet_a, i_wet_a) + p->tr1);
        vb = b_input == 0 ? xa * va : (b_input == 1 ? ya : xb * va);
        yb = phasor_tick(&p->b, vb, gb, 1.0 / ldr_tick(&p->wet_b, i_wet_b) + p->tr1);

        ya = ya / va * p->sm[SM_GAIN];
        yb = yb / va * p->sm[SM_GAIN];
        if (!isfinite(ya) || !isfinite(yb)) {
            phasor_reset(&p->a);
            phasor_reset(&p->b);
            ya = yb = 0.0;
        }
        out_a[i] = (float)((1.0 - p->fade) * xa + p->fade * ya);
        out_b[i] = (float)((1.0 - p->fade) * xb + p->fade * yb);
    }

    /* Opto LED brightness for the GUI. */
    if (p->port[PORT_LED_A])
        *(float*)p->port[PORT_LED_A] = (float)clampd(led_a / LED_FULL_SCALE, 0.0, 1.0);
    if (p->port[PORT_LED_B])
        *(float*)p->port[PORT_LED_B] = (float)clampd(led_b / LED_FULL_SCALE, 0.0, 1.0);

#ifdef BL_HAS_CSR
    _mm_setcsr(csr);
#endif
}

static void deactivate(LV2_Handle h) { (void)h; }
static void cleanup(LV2_Handle h) { free(h); }
static const void* extension_data(const char* uri) { (void)uri; return NULL; }

static const LV2_Descriptor descriptor = {
    BL_URI, instantiate, connect_port, activate, run, deactivate, cleanup, extension_data
};

LV2_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : NULL;
}
