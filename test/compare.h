/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The reference run for vst3_check and au_check: the LV2 build, driven through its own ABI with
 * a fixed test signal and settings, so the other formats can be compared with it sample for
 * sample. Each host starts from the defaults, changes CMP_CHANGES before the first block and
 * bypasses from block CMP_BYPASS on. */
#ifndef BL_COMPARE_H
#define BL_COMPARE_H

#include <math.h>
#include <stdlib.h>

#include "../src/lv2_abi.h"
#include "../src/params.h"

#ifdef _WIN32
#    include <windows.h>
#    define CMP_OPEN(p) ((void*)LoadLibraryA(p))
#    define CMP_SYM(l, s) ((void*)GetProcAddress((HMODULE)(l), s))
#    define CMP_CLOSE(l) FreeLibrary((HMODULE)(l))
#    define CMP_LIB "bi_lumen.dll"
#else
#    include <dlfcn.h>
#    define CMP_OPEN(p) dlopen(p, RTLD_NOW | RTLD_LOCAL)
#    define CMP_SYM(l, s) dlsym(l, s)
#    define CMP_CLOSE(l) dlclose(l)
#    ifdef __APPLE__
#        define CMP_LIB "bi_lumen.dylib"
#    else
#        define CMP_LIB "bi_lumen.so"
#    endif
#endif

#define CMP_BLOCK 512
#define CMP_BLOCKS 60
#define CMP_BYPASS 40
#define CMP_RATE 48000.0
#define CMP_FRAMES (CMP_BLOCK * CMP_BLOCKS)

typedef struct { int port; float plain; } CmpChange;

static const CmpChange CMP_CHANGES[] = {
    { PORT_FEEDBACK_A, 0.6f }, { PORT_SWEEP_B, 1.0f }, { PORT_B_INPUT, 2.0f }, { PORT_OUTPUT, -6.0f },
};
#define CMP_NUM_CHANGES ((int)(sizeof CMP_CHANGES / sizeof CMP_CHANGES[0]))

/* Two different tones plus a little deterministic noise, so both inputs matter. */
static float cmp_input(int ch, int n)
{
    const double t = n / CMP_RATE;
    unsigned x = (unsigned)n * 2654435761u + (unsigned)ch * 40503u;
    x ^= x >> 13;
    return (float)(0.4 * sin(2 * 3.14159265358979 * (ch ? 330.0 : 220.0) * t)
                   + 0.05 * ((x & 0xffff) / 32768.0 - 1.0));
}

/* Interleaved stereo output of the LV2 build (CMP_FRAMES frames), or NULL. Free it with free(). */
static float* cmp_run_lv2(const char* bundle)
{
    char path[1024];
    void* lib;
    const LV2_Descriptor* (*get)(uint32_t);
    const LV2_Descriptor* d;
    LV2_Handle h;
    float ctl[PORT_COUNT] = { 0 }, in[2][CMP_BLOCK], o[2][CMP_BLOCK];
    float* out;
    int i, b, c;

    snprintf(path, sizeof path, "%s/%s", bundle, CMP_LIB);
    lib = CMP_OPEN(path);
    if (!lib)
        return NULL;
    *(void**)&get = CMP_SYM(lib, "lv2_descriptor");
    d = get ? get(0) : NULL;
    h = d ? d->instantiate(d, CMP_RATE, bundle, NULL) : NULL;
    out = (float*)malloc(sizeof(float) * 2 * CMP_FRAMES);
    if (!h || !out) {
        free(out);
        CMP_CLOSE(lib);
        return NULL;
    }
    for (i = 0; i < BL_NUM_PARAMS; ++i)
        ctl[BL_PARAMS[i].port] = BL_PARAMS[i].def;
    for (i = BL_FIRST_CONTROL; i < PORT_COUNT; ++i)
        d->connect_port(h, (uint32_t)i, &ctl[i]);
    for (c = 0; c < 2; ++c) {
        d->connect_port(h, (uint32_t)(PORT_IN_A + c), in[c]);
        d->connect_port(h, (uint32_t)(PORT_OUT_A + c), o[c]);
    }
    d->activate(h);
    for (i = 0; i < CMP_NUM_CHANGES; ++i)
        ctl[CMP_CHANGES[i].port] = CMP_CHANGES[i].plain;
    for (b = 0; b < CMP_BLOCKS; ++b) {
        if (b == CMP_BYPASS)
            ctl[PORT_ENABLED] = 0.0f;
        for (c = 0; c < 2; ++c)
            for (i = 0; i < CMP_BLOCK; ++i)
                in[c][i] = cmp_input(c, b * CMP_BLOCK + i);
        d->run(h, CMP_BLOCK);
        for (i = 0; i < CMP_BLOCK; ++i) {
            out[2 * (b * CMP_BLOCK + i)] = o[0][i];
            out[2 * (b * CMP_BLOCK + i) + 1] = o[1][i];
        }
    }
    d->cleanup(h);
    CMP_CLOSE(lib);
    return out;
}

/* Largest difference between two interleaved outputs, their peak, and how far the bypassed
 * tail (two blocks after the bypass, to let the 20 ms fade finish) is from the dry input. */
static void cmp_measure(const float* a, const float* ref, double* peak, double* diff, double* bypass_diff)
{
    int i;
    *peak = *diff = *bypass_diff = 0.0;
    for (i = 0; i < 2 * CMP_FRAMES; ++i) {
        *peak = fmax(*peak, fabs((double)a[i]));
        *diff = fmax(*diff, fabs((double)a[i] - ref[i]));
        if (i >= 2 * (CMP_BYPASS + 2) * CMP_BLOCK)
            *bypass_diff = fmax(*bypass_diff, fabs((double)a[i] - cmp_input(i & 1, i / 2)));
    }
}

#endif
