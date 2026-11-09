/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Checks for the Bi-Lumen. Usage: test_dsp <bundle folder>
 * Part 1 drives the circuit code directly: the audio path against the schematic's transfer
 * function (worked out below in the s-domain, independently of the solver), the sweep
 * generators against their rate formula and the owner's manual, and a report of the
 * sweep range. Part 2 loads the built plugin library the way a host does. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/lv2_abi.h"
#include "../src/opto.h"
#include "../src/phasor.h"
#include "../src/sweep.h"

#ifdef _WIN32
#    include <windows.h>
#    define LIB_NAME "bi_lumen.dll"
#    define LIB_OPEN(p) ((void*)LoadLibraryA(p))
#    define LIB_SYM(l, s) ((void*)GetProcAddress((HMODULE)(l), s))
#else
#    include <dlfcn.h>
#    ifdef __APPLE__
#        define LIB_NAME "bi_lumen.dylib"
#    else
#        define LIB_NAME "bi_lumen.so"
#    endif
#    define LIB_OPEN(p) dlopen(p, RTLD_NOW)
#    define LIB_SYM(l, s) dlsym(l, s)
#endif

#define PI 3.14159265358979323846
#define DEFAULT_CENTER 0.15

static int failures;

static void check(const char* what, int ok)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

/* ---- Complex helpers ---- */

typedef struct { double re, im; } C;
static C cx(double r, double i) { C c; c.re = r; c.im = i; return c; }
static C cre(double r) { return cx(r, 0.0); }
static C cadd(C a, C b) { return cx(a.re + b.re, a.im + b.im); }
static C csub(C a, C b) { return cx(a.re - b.re, a.im - b.im); }
static C cmul(C a, C b) { return cx(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
static C cdiv(C a, C b)
{
    double d = b.re * b.re + b.im * b.im;
    return cx((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
}
static C cinv(C a) { return cdiv(cre(1.0), a); }
static double cabs2(C a) { return sqrt(a.re * a.re + a.im * a.im); }

/* Solves the 3x3 complex system m*v = b by Gaussian elimination. */
static void csolve3(C m[3][3], C b[3], C v[3])
{
    int i, j, k;
    for (i = 0; i < 3; ++i) {
        for (j = i + 1; j < 3; ++j) {
            C f = cdiv(m[j][i], m[i][i]);
            for (k = i; k < 3; ++k)
                m[j][k] = csub(m[j][k], cmul(f, m[i][k]));
            b[j] = csub(b[j], cmul(f, b[i]));
        }
    }
    for (i = 2; i >= 0; --i) {
        C s = b[i];
        for (k = i + 1; k < 3; ++k)
            s = csub(s, cmul(m[i][k], v[k]));
        v[i] = cdiv(s, m[i][i]);
    }
}

/* The schematic's response from the input to U2A, for a fixed LDR conductance, wet-path
 * resistance and VR5 position. */
static C expected(double f, double fs, double g_ldr, double r_wet, double k)
{
    const double w = 2.0 * fs * tan(PI * f / fs); /* the trapezoidal rule's frequency warp */
    const C s = cx(0.0, w);
    const double r_ap = 1.0 / (g_ldr + 1.0 / PH_AP_R);
    const double ra = (1.0 - k) * PH_VR5 + 1.0, rb = k * PH_VR5 + 1.0;
    C hin, ap, ap6, z6, a1, bd, alpha, y, xnode, z3, out2, yw, m;
    C mat[3][3], rhs[3], v[3];
    C src;
    int i;

    /* C1 into R15. */
    hin = cdiv(cmul(s, cre(PH_C1 * PH_R15)), cadd(cre(1.0), cmul(s, cre(PH_C1 * PH_R15))));
    /* One stage: (sRC - 1) / (sRC + 1). */
    ap = cdiv(csub(cmul(s, cre(r_ap * PH_AP_C)), cre(1.0)), cadd(cmul(s, cre(r_ap * PH_AP_C)), cre(1.0)));
    ap6 = cre(1.0);
    for (i = 0; i < 6; ++i)
        ap6 = cmul(ap6, ap);
    /* U1B: out1 = P (1 + Z6/R1) - d Z6/R1. */
    z6 = cinv(cadd(cre(1.0 / PH_R6), cmul(s, cre(PH_C4))));
    a1 = cadd(cre(1.0), cmul(z6, cre(1.0 / PH_R1)));
    bd = cmul(z6, cre(1.0 / PH_R1));
    /* Nodes X, W, P for Y = 1: nodal equations of C20, VR5, R5, R13, R14 and C5. */
    mat[0][0] = cadd(cmul(s, cre(PH_C20)), cre(1.0 / PH_R5 + 1.0 / ra));
    mat[0][1] = cre(-1.0 / ra);
    mat[0][2] = cre(0.0);
    mat[1][0] = cre(-1.0 / ra);
    mat[1][1] = cre(1.0 / ra + 1.0 / rb + 1.0 / PH_R13);
    mat[1][2] = cre(-1.0 / PH_R13);
    mat[2][0] = cre(0.0);
    mat[2][1] = cre(-1.0 / PH_R13);
    mat[2][2] = cadd(cre(1.0 / PH_R13 + 1.0 / PH_R14), cmul(s, cre(PH_C5)));
    src = cmul(s, cre(PH_C20));
    rhs[0] = src;
    rhs[1] = cre(0.0);
    rhs[2] = cre(0.0);
    csolve3(mat, rhs, v);
    alpha = v[2];
    /* Y = ap6 * (a1 * alpha * Y - bd * d)  ->  Y per unit d. */
    y = cdiv(cmul(ap6, cmul(bd, cre(-1.0))), csub(cre(1.0), cmul(ap6, cmul(a1, alpha))));
    xnode = cmul(v[0], y);
    z3 = cinv(cadd(cre(1.0 / PH_R3), cmul(s, cre(PH_C6))));
    out2 = cmul(cre(-1.0), cmul(xnode, cmul(z3, cre(1.0 / PH_R5))));
    yw = cinv(cadd(cre(r_wet + PH_R49), cinv(cmul(s, cre(PH_C21)))));
    m = cdiv(cadd(cre(1.0 / PH_R50), cmul(out2, yw)),
             cadd(cadd(cre(1.0 / PH_R50), yw), cmul(s, cre(PH_C22))));
    return cmul(hin, m);
}

static C measure(Phasor* p, double f, double fs, double g_ldr, double r_wet, double amp)
{
    const long settle = (long)(0.5 * fs);
    const long cycles = (long)ceil(0.1 * f) + 4;
    const long n = (long)floor(cycles * fs / f);
    const double used_cycles = floor(n * f / fs);
    const long m = (long)floor(used_cycles * fs / f);
    double re = 0.0, im = 0.0;
    long i;
    phasor_reset(p);
    for (i = 0; i < settle + m; ++i) {
        double ph = 2.0 * PI * f * (double)i / fs;
        double y = phasor_tick(p, amp * sin(ph), g_ldr, r_wet);
        if (i >= settle) {
            re += y * sin(ph);
            im += y * cos(ph);
        }
    }
    return cx(2.0 * re / (double)m / amp, 2.0 * im / (double)m / amp);
}

static void test_transfer_function(double fs)
{
    static const double r_ldr[] = { 2e3, 10e3, 50e3, 1e6 };
    static const double fb[] = { 0.0, 0.5, 0.95 };
    const double r_wet = 7.3e3;
    Phasor* p = (Phasor*)malloc(sizeof(Phasor));
    double worst = 0.0, worst_f = 0.0, worst_r = 0.0, worst_k = 0.0;
    int a, b;
    char label[200];
    phasor_init(p, fs);
    for (b = 0; b < 3; ++b) {
        phasor_set_feedback(p, fb[b]);
        for (a = 0; a < 4; ++a) {
            double f;
            for (f = 30.0; f < 18000.0 && f < 0.45 * fs; f *= 1.4) {
                C got = measure(p, f, fs, 1.0 / r_ldr[a], r_wet, 1e-3);
                C want = expected(f, fs, 1.0 / r_ldr[a], r_wet, fb[b]);
                /* The small constant keeps deep notches from dominating the figure. */
                double e = cabs2(csub(got, want)) / (cabs2(want) + 0.02);
                if (e > worst) {
                    worst = e;
                    worst_f = f;
                    worst_r = r_ldr[a];
                    worst_k = fb[b];
                }
            }
        }
    }
    snprintf(label, sizeof label,
             "%.0f Hz: matches the schematic's response (worst %.4f%% at %.0f Hz, LDR %.0f ohm, feedback %.2f)",
             fs, worst * 100.0, worst_f, worst_r, worst_k);
    check(label, worst < 1e-3);
    free(p);
}

/* Notches of the steady-state response at one LDR resistance. */
static void print_notches(double r_ldr, double k)
{
    double f, prev2 = 1e9, prev1 = 1e9, fprev = 0.0, peak = 0.0;
    printf("      LDR %7.0f ohm:", r_ldr);
    for (f = 20.0; f < 20000.0; f *= 1.01) {
        double m = cabs2(expected(f, 1e7, 1.0 / r_ldr, PH_R50 - PH_R49, k));
        if (m > peak)
            peak = m;
        if (prev1 < prev2 && prev1 < m && fprev > 20.0)
            printf(" %.0f Hz (%.0f dB)", fprev, 20.0 * log10(prev1 + 1e-12));
        prev2 = prev1;
        prev1 = m;
        fprev = f;
    }
    printf("   peak %+.1f dB\n", 20.0 * log10(peak));
}

/* ---- Sweep generators ---- */

static double rate_of(Sweep* s, int which, double fs, double expect_hz)
{
    const double seconds = 6.0 / expect_hz + 1.0;
    long i, n = (long)(seconds * fs);
    double prev = which == 1 ? s->sq1 : s->sq2, first = -1.0, last = -1.0;
    int edges = 0;
    for (i = 0; i < n; ++i) {
        double q;
        if (which == 1) {
            sweep1_tick(s, 0);
            q = s->sq1;
        } else {
            sweep2_tick(s, 0, 0.0);
            q = s->sq2;
        }
        if (prev < 0.0 && q > 0.0) {
            /* The edge falls between samples; place it from the triangle's overshoot. */
            double tri = which == 1 ? s->tri1 : s->tri2;
            double slope = which == 1 ? s->i1 / (fs * S1_C43) : s2_current(s->vr3, OPAMP_SAT) / (fs * S2_C44);
            double t = (double)i - (SW_SCHMITT_TH - tri) / slope;
            if (first < 0.0)
                first = t;
            last = t;
            ++edges;
        }
        prev = q;
    }
    return edges > 1 ? (edges - 1) * fs / (last - first) : 0.0;
}

static void test_sweep_rates(void)
{
    static const double rates[] = { 0.1, 0.5, 2.0, 8.0, 18.0 };
    static const double sample_rates[] = { 44100.0, 96000.0 };
    char label[160];
    int r, k, w;
    Sweep s;
    double lo1, hi1, lo2, hi2;

    lo1 = sweep_hz(s1_integrator_current(s1_knob_current(0.0)), S1_C43);
    hi1 = sweep_hz(s1_integrator_current(s1_knob_current(1.0)), S1_C43);
    lo2 = sweep_hz(s2_current(0.0, OPAMP_SAT), S2_C44);
    hi2 = sweep_hz(s2_current(1.0, OPAMP_SAT), S2_C44);
    printf("  circuit values give Sweep 1 %.3f-%.1f Hz and Sweep 2 %.3f-%.1f Hz; the manual says 0.1-18 Hz\n",
           lo1, hi1, lo2, hi2);
    check("Sweep 1 range covers the manual's 0.1-18 Hz within 25%", lo1 < 0.125 && lo1 > 0.075 && hi1 > 18.0 && hi1 < 22.5);
    check("Sweep 2 range covers the manual's 0.1-18 Hz within 25%", lo2 < 0.125 && lo2 > 0.075 && hi2 > 18.0 && hi2 < 22.5);
    printf("  pedal at full travel runs Sweep 1 at %.2f Hz\n",
           sweep_hz(s1_integrator_current(s1_pedal_current(1.0)), S1_C43));

    for (w = 1; w <= 2; ++w) {
        for (k = 0; k < 2; ++k) {
            const double fs = sample_rates[k];
            for (r = 0; r < 5; ++r) {
                double hz;
                sweep_init(&s, fs);
                if (w == 1)
                    s.i1 = s1_integrator_current(s1_knob_current(s1_knob_for_hz(rates[r])));
                else
                    s.vr3 = s2_knob_for_hz(rates[r]);
                hz = rate_of(&s, w, fs, rates[r]);
                snprintf(label, sizeof label, "%.0f Hz sample rate: Sweep %d set to %.1f Hz runs at %.4f Hz",
                         fs, w, rates[r], hz);
                check(label, fabs(hz - rates[r]) < 0.005 * rates[r]);
            }
        }
    }
}

/* ---- Sweep range ---- */

static void report_sweep_range(void)
{
    DriverLut* lut = (DriverLut*)malloc(sizeof(DriverLut));
    static const double centers[] = { 0.0, 0.1, 0.15, 0.25, 0.5 };
    double v, prev_i = -1.0;
    int c, mono = 1;
    driver_lut_init(lut);

    for (v = 0.0; v < 1.5; v += 0.001) {
        double i = driver_led_current(lut, v);
        if (i < prev_i - 1e-6) /* past saturation R71 carries a few uA less */
            mono = 0;
        prev_i = i;
    }
    check("LED current rises with the control voltage (to within 1 uA once saturated)", mono);
    printf("  LED driver: %.2f mA at 0.2 V, %.2f mA at 0.5 V, %.2f mA at 0.7 V, saturates at %.2f mA\n",
           1e3 * driver_led_current(lut, 0.2), 1e3 * driver_led_current(lut, 0.5),
           1e3 * driver_led_current(lut, 0.7), 1e3 * driver_led_current(lut, 1.5));
    printf("  wet LDR (R16) lit by the footswitch LED at %.1f mA: %.0f ohm, so TR1 = %.0f ohm\n",
           1e3 * BYPASS_LED_I, ldr_steady_r(BYPASS_LED_I),
           PH_R50 - PH_R49 - ldr_steady_r(BYPASS_LED_I));

    for (c = 0; c < 5; ++c) {
        /* Sweep 1 triangle into VR2 at 0.8, as at the default settings. */
        const double peak = SW_SCHMITT_TH * (S1_R120 + S1_TR9) / (S1_R118 + S1_R120 + S1_TR9);
        Thevenin lo = depth_wiper(-peak, 0.0, 0.8, 0.0), hi = depth_wiper(peak, 0.0, 0.8, 0.0);
        double ilo = driver_led_current(lut, driver_vplus(lo.v, lo.r, trim_voltage(centers[c])));
        double ihi = driver_led_current(lut, driver_vplus(hi.v, hi.r, trim_voltage(centers[c])));
        double rlo = par(ldr_steady_r(ilo), PH_AP_R), rhi = par(ldr_steady_r(ihi), PH_AP_R);
        printf("  Center %.2f, Depth 0.8: LED %.2f-%.2f mA, stage R %.0f-%.0f ohm, stage centre %.0f-%.0f Hz\n",
               centers[c], 1e3 * ilo, 1e3 * ihi, rlo, rhi,
               1.0 / (2.0 * PI * rlo * PH_AP_C), 1.0 / (2.0 * PI * rhi * PH_AP_C));
        if (centers[c] == DEFAULT_CENTER) {
            printf("    notches at the two ends of the default sweep, feedback 0:\n");
            print_notches(rlo, 0.0);
            print_notches(rhi, 0.0);
            printf("    and with feedback at 0.8:\n");
            print_notches(rlo, 0.8);
            print_notches(rhi, 0.8);
        }
    }
    free(lut);
}

/* ---- Part 2: the built library ---- */

#define NPORTS 27
static const float defaults[NPORTS] = {
    0, 0, 0, 0,
    0.5f, 0, 0, 0.3f, 0,
    0.8f, 0.0f, 0.8f, 0.0f,
    0, 0, 0, 1,
    1, 1, 0.5f, 0.15f, 0.15f,
    1.0f, 0.0f, 1,
    0, 0 /* opto LED outputs */
};

typedef struct {
    const LV2_Descriptor* d;
    LV2_Handle h;
    float c[NPORTS];
} Inst;

static void inst_open(Inst* x, const LV2_Descriptor* d, double fs)
{
    int i;
    x->d = d;
    x->h = d->instantiate(d, fs, "", NULL);
    memcpy(x->c, defaults, sizeof defaults);
    for (i = 4; i < NPORTS; ++i)
        d->connect_port(x->h, (uint32_t)i, &x->c[i]);
    d->activate(x->h);
}

/* Stereo render; in_b may be NULL to feed silence. */
static void inst_render(Inst* x, const float* ia, const float* ib, float* oa, float* ob, long n, int block)
{
    static float zero[4096];
    long pos = 0;
    while (pos < n) {
        int m = n - pos < block ? (int)(n - pos) : block;
        x->d->connect_port(x->h, 0, (void*)(ia + pos));
        x->d->connect_port(x->h, 1, ib ? (void*)(ib + pos) : (void*)zero);
        x->d->connect_port(x->h, 2, oa + pos);
        x->d->connect_port(x->h, 3, ob + pos);
        x->d->run(x->h, (uint32_t)m);
        pos += m;
    }
}

static void inst_close(Inst* x)
{
    x->d->deactivate(x->h);
    x->d->cleanup(x->h);
}

static void fill_sine(float* b, long n, double f, double fs, double amp)
{
    long i;
    for (i = 0; i < n; ++i)
        b[i] = (float)(amp * sin(2.0 * PI * f * (double)i / fs));
}

static int all_finite(const float* b, long n, double limit, double* peak)
{
    long i;
    double p = 0.0;
    int ok = 1;
    for (i = 0; i < n; ++i) {
        if (!isfinite(b[i]))
            ok = 0;
        else if (fabs(b[i]) > p)
            p = fabs(b[i]);
    }
    *peak = p;
    return ok && p <= limit;
}

/* Smallest and largest 20 ms RMS after the first second. */
static void level_range(const float* b, long n, double fs, double* lo, double* hi)
{
    long w = (long)(0.02 * fs), p, i;
    *lo = 1e9;
    *hi = 0.0;
    for (p = (long)fs; p + w < n; p += w) {
        double e = 0.0;
        for (i = 0; i < w; ++i)
            e += b[p + i] * b[p + i];
        e = sqrt(e / w);
        if (e < *lo) *lo = e;
        if (e > *hi) *hi = e;
    }
}

static double rms_diff(const float* a, const float* b, long from, long n)
{
    double e = 0.0, r = 0.0;
    long i;
    for (i = from; i < n; ++i) {
        e += (a[i] - b[i]) * (a[i] - b[i]);
        r += b[i] * b[i];
    }
    return sqrt(e / (r + 1e-30));
}

static void test_plugin(const char* bundle)
{
    char path[1024], label[200];
    const double fs = 48000.0;
    const long n = (long)(10.0 * fs);
    float* in = (float*)malloc(sizeof(float) * n);
    float* oa = (float*)malloc(sizeof(float) * n);
    float* ob = (float*)malloc(sizeof(float) * n);
    float* oa2 = (float*)malloc(sizeof(float) * n);
    float* ob2 = (float*)malloc(sizeof(float) * n);
    const LV2_Descriptor* (*get)(uint32_t);
    const LV2_Descriptor* d;
    void* lib;
    Inst x;
    double peak, lo, hi;
    static const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    int r;

    snprintf(path, sizeof path, "%s/%s", bundle, LIB_NAME);
    lib = LIB_OPEN(path);
    check("library loads", lib != NULL);
    if (!lib)
        return;
    *(void**)&get = LIB_SYM(lib, "lv2_descriptor");
    check("exports lv2_descriptor", get != NULL);
    if (!get)
        return;
    d = get(0);
    check("descriptor has the plugin's URI", d && strcmp(d->URI, "urn:davidian:bi-lumen") == 0);
    check("only one plugin in the library", get(1) == NULL);

    fill_sine(in, n, 1000.0, fs, 0.1);

    /* Defaults: B takes A's output, so Out B is twelve stages. */
    inst_open(&x, d, fs);
    inst_render(&x, in, NULL, oa, ob, n, 256);
    inst_close(&x);
    level_range(oa, n, fs, &lo, &hi);
    snprintf(label, sizeof label, "sweep moves a 1 kHz tone at Out A by %.1f dB", 20.0 * log10(hi / lo));
    check(label, hi / lo > 2.0);
    level_range(ob, n, fs, &lo, &hi);
    snprintf(label, sizeof label, "sweep moves a 1 kHz tone at Out B (series) by %.1f dB", 20.0 * log10(hi / lo));
    check(label, hi / lo > 2.0);

    /* The opto LED outputs for the GUI follow the sweep, block by block. */
    {
        float lmin = 2.0f, lmax = -1.0f;
        long pos;
        inst_open(&x, d, fs);
        for (pos = 0; pos + 256 <= (long)(4.0 * fs); pos += 256) {
            inst_render(&x, in + pos, NULL, oa + pos, ob + pos, 256, 256);
            if (x.c[25] < lmin) lmin = x.c[25];
            if (x.c[25] > lmax) lmax = x.c[25];
        }
        inst_close(&x);
        snprintf(label, sizeof label, "opto LED A output swings %.2f to %.2f over a sweep", lmin, lmax);
        check(label, lmin >= 0.0f && lmax <= 1.0f && lmax - lmin > 0.5f);
    }

    /* Block size must not change the audio. */
    inst_open(&x, d, fs);
    inst_render(&x, in, NULL, oa, ob, n, 1);
    inst_close(&x);
    inst_open(&x, d, fs);
    inst_render(&x, in, NULL, oa2, ob2, n, 512);
    inst_close(&x);
    check("output is the same at block sizes 1 and 512",
          memcmp(oa, oa2, sizeof(float) * n) == 0 && memcmp(ob, ob2, sizeof(float) * n) == 0);

    /* Hosts may pass the same buffer for input and output. */
    memcpy(oa2, in, sizeof(float) * n);
    inst_open(&x, d, fs);
    inst_render(&x, oa2, NULL, oa2, ob2, n, 256);
    inst_close(&x);
    check("works with the same buffer for in and out", memcmp(oa, oa2, sizeof(float) * n) == 0);

    /* Host bypass passes audio untouched. */
    inst_open(&x, d, fs);
    x.c[24] = 0.0f;
    d->activate(x.h);
    inst_render(&x, in, in, oa, ob, n, 256);
    inst_close(&x);
    check("host bypass output is identical to input",
          memcmp(in, oa, sizeof(float) * n) == 0 && memcmp(in, ob, sizeof(float) * n) == 0);

    /* Footswitches off: R16 goes dark and the dry path is all that's left. */
    inst_open(&x, d, fs);
    x.c[17] = x.c[18] = 0.0f;
    x.c[16] = 2.0f;
    d->activate(x.h);
    inst_render(&x, in, in, oa, ob, n, 256);
    inst_close(&x);
    {
        double ea = rms_diff(oa, in, (long)fs, n), eb = rms_diff(ob, in, (long)fs, n);
        snprintf(label, sizeof label, "footswitches off leave the dry signal (residual %.1f dB, %.1f dB)",
                 20.0 * log10(ea), 20.0 * log10(eb));
        check(label, ea < 0.03 && eb < 0.03);
    }

    /* Footswitch on from off: the LDR fades the wet path in, so no step on silence. */
    {
        float* silence = (float*)calloc(n, sizeof(float));
        inst_open(&x, d, fs);
        x.c[17] = 0.0f;
        d->activate(x.h);
        inst_render(&x, silence, NULL, oa, ob, (long)fs, 256);
        x.c[17] = 1.0f;
        x.c[24] = 1.0f;
        inst_render(&x, silence, NULL, oa, ob, n, 256);
        all_finite(oa, n, 1e9, &peak);
        snprintf(label, sizeof label, "no output on silence while switching (peak %.2g)", peak);
        check(label, peak < 1e-4);
        inst_close(&x);
        free(silence);
    }

    /* Every switch position and feedback at full, with Reverse sync. */
    {
        int a, b, ok = 1;
        for (a = 0; a < 3; ++a) {
            for (b = 0; b < 2; ++b) {
                inst_open(&x, d, fs);
                x.c[14] = (float)a;      /* Phasor B sweep */
                x.c[13] = (float)b;      /* Phasor A sweep */
                x.c[5] = x.c[8] = (float)b;
                x.c[6] = (float)b;
                x.c[15] = 1.0f;
                x.c[16] = (float)a;
                x.c[10] = x.c[12] = 1.0f;
                d->activate(x.h);
                inst_render(&x, in, in, oa, ob, (long)(3.0 * fs), 256);
                inst_close(&x);
                ok = ok && all_finite(oa, (long)(3.0 * fs), 14.0, &peak) && all_finite(ob, (long)(3.0 * fs), 14.0, &peak);
            }
        }
        check("every switch position with full feedback stays finite and bounded", ok);
    }

    /* Reverse sync: with both phasors on Gen 1 from the same input, the levels move apart. */
    {
        double fwd = 0.0, rev = 0.0;
        long i;
        for (r = 0; r < 2; ++r) {
            double c = 0.0, sa = 0.0, sb = 0.0, ma = 0.0, mb = 0.0;
            long w = (long)(0.02 * fs), p, cnt = 0;
            inst_open(&x, d, fs);
            x.c[16] = 0.0f;
            x.c[15] = (float)r;
            d->activate(x.h);
            inst_render(&x, in, NULL, oa, ob, n, 256);
            inst_close(&x);
            for (p = (long)fs; p + w < n; p += w, ++cnt) {
                double ea = 0.0, eb = 0.0;
                for (i = 0; i < w; ++i) {
                    ea += oa[p + i] * oa[p + i];
                    eb += ob[p + i] * ob[p + i];
                }
                oa2[cnt] = (float)sqrt(ea / w);
                ob2[cnt] = (float)sqrt(eb / w);
                ma += oa2[cnt];
                mb += ob2[cnt];
            }
            ma /= cnt;
            mb /= cnt;
            for (i = 0; i < cnt; ++i) {
                c += (oa2[i] - ma) * (ob2[i] - mb);
                sa += (oa2[i] - ma) * (oa2[i] - ma);
                sb += (ob2[i] - mb) * (ob2[i] - mb);
            }
            if (r == 0)
                fwd = c / sqrt(sa * sb);
            else
                rev = c / sqrt(sa * sb);
        }
        snprintf(label, sizeof label, "Sweep Sync: level correlation A/B %.2f in Normal, %.2f in Reverse", fwd, rev);
        check(label, fwd > 0.95 && rev < fwd - 0.3);
    }

    /* Too-hot input stays bounded by the rails. */
    inst_open(&x, d, fs);
    x.c[22] = 10.0f;
    x.c[10] = 1.0f;
    d->activate(x.h);
    fill_sine(in, n, 200.0, fs, 4.0);
    inst_render(&x, in, in, oa, ob, n, 64);
    inst_close(&x);
    check("very hot input stays finite and bounded",
          all_finite(oa, n, 4.0, &peak) && all_finite(ob, n, 4.0, &peak));
    printf("         peak %.3f\n", peak);

    /* Bad input must not leave the plugin stuck. */
    fill_sine(in, n, 1000.0, fs, 0.1);
    in[1000] = (float)NAN;
    in[2000] = (float)INFINITY;
    inst_open(&x, d, fs);
    inst_render(&x, in, NULL, oa, ob, n, 256);
    inst_close(&x);
    check("recovers from NaN and infinity in the input", all_finite(oa, n, 4.0, &peak) && peak > 0.01);
    in[1000] = in[2000] = 0.0f;

    /* Every sample rate, with controls at their extremes. */
    for (r = 0; r < 5; ++r) {
        const double rate = rates[r];
        const long m = (long)(2.0 * rate);
        float* a = (float*)malloc(sizeof(float) * m);
        float* b = (float*)malloc(sizeof(float) * m);
        float* c2 = (float*)malloc(sizeof(float) * m);
        int ok = 1, s, t;
        fill_sine(a, m, 440.0, rate, 0.5);
        for (s = 0; s < 2; ++s) {
            for (t = 0; t < 2; ++t) {
                Inst y;
                inst_open(&y, d, rate);
                y.c[4] = y.c[7] = s ? 18.0f : 0.1f;
                y.c[20] = y.c[21] = t ? 1.0f : 0.0f;
                y.c[9] = y.c[11] = t ? 1.0f : 0.0f;
                y.c[10] = y.c[12] = s ? 1.0f : 0.0f;
                inst_render(&y, a, a, b, c2, m, 128);
                inst_close(&y);
                ok = ok && all_finite(b, m, 4.0, &peak) && all_finite(c2, m, 4.0, &peak);
            }
        }
        free(a);
        free(b);
        free(c2);
        snprintf(label, sizeof label, "%.0f Hz: stable at control extremes", rate);
        check(label, ok);
    }

    /* CPU, both phasors running. */
    fill_sine(in, n, 220.0, fs, 0.25);
    inst_open(&x, d, fs);
    {
        clock_t t0 = clock();
        double seconds;
        inst_render(&x, in, in, oa, ob, n, 256);
        seconds = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  CPU: %.2f%% of one core at 48 kHz\n", 100.0 * seconds / 10.0);
        check("runs far faster than real time", seconds < 1.0);
    }
    inst_close(&x);

    free(in);
    free(oa);
    free(ob);
    free(oa2);
    free(ob2);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: test_dsp <bundle folder>\n");
        return 2;
    }
    printf("Audio path against the schematic's transfer function\n");
    test_transfer_function(44100.0);
    test_transfer_function(48000.0);
    test_transfer_function(96000.0);
    printf("Sweep generators\n");
    test_sweep_rates();
    printf("Sweep range\n");
    report_sweep_range();
    printf("Built plugin\n");
    test_plugin(argv[1]);
    printf(failures ? "\n%d FAILED\n" : "\nAll passed\n", failures);
    return failures ? 1 : 0;
}
