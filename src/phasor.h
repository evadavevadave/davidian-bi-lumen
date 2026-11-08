/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* One phasor channel, Phasor A as numbered on the schematic (Phasor B is identical:
 * C24/R59 input, U3A/U3B, U7-U10 stages, VR6, U4B, R60/TR2, U4A).
 *
 *   in -C1 47n-+- R15 390k        input coupling, U1A unity buffer -> d
 *   d -R1 4k7- U1B (R6 4k7 || C4 1n)   inverting; its + input carries the feedback:
 *              VR5 wiper -R13 6k2- + (R14 4k7 || C5 3n3 to ground)
 *   U1B out -> six all-pass stages: in -4k7- (-) -4k7- out, in -C 6n8- (+) -(LDR || 220k)- gnd
 *   last stage -C20 470n- node X, X -VR5 10k- gnd, X -R5 4k7- U2B (R3 4k7 || C6 1n), inverting
 *   U2B out -R16 (LDR) -TR1 -C21 150n -R49 4k7- mix node M
 *   d -R50 12k- M, M -C22 470p- gnd, U2A buffers M to the output
 *
 * Every capacitor uses the trapezoidal rule. The feedback loop through VR5 is solved
 * within the sample, so it adds no delay. The LDRs are time-varying resistors, which keeps
 * the whole channel linear in the audio signal apart from op amp clipping. */
#ifndef PHASOR_H
#define PHASOR_H

#include "circuit.h"

#define PH_C1   47e-9
#define PH_R15  390e3
#define PH_R1   4.7e3
#define PH_R6   4.7e3
#define PH_C4   1e-9
#define PH_R13  6.2e3
#define PH_R14  4.7e3
#define PH_C5   3.3e-9
#define PH_VR5  10e3
#define PH_C20  470e-9
#define PH_R5   4.7e3
#define PH_R3   4.7e3
#define PH_C6   1e-9
#define PH_R49  4.7e3
#define PH_C21  150e-9
#define PH_R50  12e3
#define PH_C22  470e-12
#define PH_AP_C 6.8e-9  /* C14-C19 */
#define PH_AP_R 220e3   /* R43-R48, across each LDR */
#define PH_AP_N 6

typedef struct {
    /* 2C/T for each capacitor */
    double g1, g4, g5, g20, g6, g21, g22, gap;
    /* capacitor histories (g*v + i from the previous sample) */
    double s1, s4, s5, s20, s6, s22, h21;
    double sap[PH_AP_N];
    /* feedback network, from the VR5 position */
    double fb_k;
    double ra, rb;
    double out_a; /* CHA-OUT-A, the U2A output, used for Phasor B's "Out A" input */
} Phasor;

static inline void phasor_reset(Phasor* p)
{
    int i;
    p->s1 = p->s4 = p->s5 = p->s20 = p->s6 = p->s22 = p->h21 = 0.0;
    for (i = 0; i < PH_AP_N; ++i)
        p->sap[i] = 0.0;
    p->out_a = 0.0;
}

/* k is VR5's position, 0 (wiper at ground) to 1 (wiper at node X). A 1 ohm floor keeps the
 * equations finite at the ends of the track; it is far below anything else in the loop. */
static inline void phasor_set_feedback(Phasor* p, double k)
{
    p->fb_k = clampd(k, 0.0, 1.0);
    p->ra = (1.0 - p->fb_k) * PH_VR5 + 1.0;
    p->rb = p->fb_k * PH_VR5 + 1.0;
}

static inline void phasor_init(Phasor* p, double fs)
{
    p->g1 = 2.0 * PH_C1 * fs;
    p->g4 = 2.0 * PH_C4 * fs;
    p->g5 = 2.0 * PH_C5 * fs;
    p->g20 = 2.0 * PH_C20 * fs;
    p->g6 = 2.0 * PH_C6 * fs;
    p->g21 = 2.0 * PH_C21 * fs;
    p->g22 = 2.0 * PH_C22 * fs;
    p->gap = 2.0 * PH_AP_C * fs;
    phasor_set_feedback(p, 0.0);
    phasor_reset(p);
}

/* Node X and P (U1B's + input) as functions of the chain output Y:
 * X = cx*Y + dx, P = cp*(cw*X + dw) + dp. */
typedef struct { double cx, dx, cw, dw, cp, dp; } FbNet;

static inline FbNet phasor_fbnet(const Phasor* p)
{
    FbNet n;
    const double gra = 1.0 / p->ra, grb = 1.0 / p->rb, g13 = 1.0 / PH_R13;
    const double dP = g13 + 1.0 / PH_R14 + p->g5;
    double dW, dX;
    n.cp = g13 / dP;
    n.dp = p->s5 / dP;
    dW = gra + grb + g13 - n.cp * g13;
    n.cw = gra / dW;
    n.dw = n.dp * g13 / dW;
    dX = p->g20 + 1.0 / PH_R5 + gra - n.cw * gra;
    n.cx = p->g20 / dX;
    n.dx = (-p->s20 + n.dw * gra) / dX;
    return n;
}

/* One sample. x is the input in volts, g_ldr the conductance of each stage's LDR
 * (all six share one LED), r_wet the R16 LDR plus TR1 in ohms. Returns the U2A output. */
static inline double phasor_tick(Phasor* p, double x, double g_ldr, double r_wet)
{
    const double gr = g_ldr + 1.0 / PH_AP_R;
    const double ap_d = p->gap + gr;
    const double y6 = 1.0 / PH_R6 + p->g4;
    double d, a1, b1, A = 1.0, B = 0.0, P, out1, Y, X, W, v, out2, geq, m, iw, vc;
    FbNet n;
    int k;

    /* C1 and R15, then U1A as a unity buffer. R2 with C3 rolls off near 1.4 MHz and is left out. */
    d = (p->g1 * x - p->s1) / (p->g1 + 1.0 / PH_R15);
    p->s1 = 2.0 * p->g1 * (x - d) - p->s1;

    /* U1B: out1 = a1*P + b1. */
    a1 = 1.0 + (1.0 / PH_R1) / y6;
    b1 = -(d / PH_R1 + p->s4) / y6;

    /* The six stages as one affine map Y = A*out1 + B. */
    for (k = 0; k < PH_AP_N; ++k) {
        const double pk = p->gap / ap_d, qk = -p->sap[k] / ap_d;
        A = (2.0 * pk - 1.0) * A;
        B = (2.0 * pk - 1.0) * B + 2.0 * qk;
    }

    /* Close the loop through C20, VR5, R13 and back into U1B. */
    n = phasor_fbnet(p);
    {
        const double alpha = n.cp * n.cw * n.cx;
        const double beta = n.cp * (n.cw * n.dx + n.dw) + n.dp;
        P = (alpha * (A * b1 + B) + beta) / (1.0 - alpha * A * a1);
    }
    out1 = opamp_limit(a1 * P + b1);

    /* Run the stages forward from the (possibly clipped) U1B output and update them. */
    v = out1;
    for (k = 0; k < PH_AP_N; ++k) {
        const double vp = (p->gap * v - p->sap[k]) / ap_d;
        p->sap[k] = 2.0 * p->gap * (v - vp) - p->sap[k];
        v = 2.0 * vp - v;
    }
    Y = v;
    X = n.cx * Y + n.dx;
    W = n.cw * X + n.dw;
    P = n.cp * W + n.dp;
    p->s20 = 2.0 * p->g20 * (Y - X) - p->s20;
    p->s5 = 2.0 * p->g5 * P - p->s5;
    p->s4 = 2.0 * p->g4 * (P - out1) - p->s4;

    /* U2B inverts node X (R5 into its virtual ground). */
    out2 = opamp_limit(-(X / PH_R5 + p->s6) / (1.0 / PH_R3 + p->g6));
    p->s6 = 2.0 * p->g6 * (-out2) - p->s6;

    /* Mix node: dry through R50, wet through R16 + TR1 + R49 in series with C21, C22 to ground. */
    geq = 1.0 / (r_wet + PH_R49 + 1.0 / p->g21);
    m = (d / PH_R50 + (out2 - p->h21) * geq + p->s22) / (1.0 / PH_R50 + geq + p->g22);
    iw = (out2 - m - p->h21) * geq;
    vc = p->h21 + iw / p->g21;
    p->h21 = vc + iw / p->g21;
    p->s22 = 2.0 * p->g22 * m - p->s22;

    p->out_a = opamp_limit(m);
    return p->out_a;
}

#endif
