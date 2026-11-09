/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Control metadata for the hosts that don't read bundle/bi_lumen.ttl: the VST3 and Audio Unit
 * builds. Values are the same plain values as the LV2 ports, so a setting means the same thing
 * in every format. test/params_check.c checks this table against the TTL and the panel. */
#ifndef BL_PARAMS_H
#define BL_PARAMS_H

#include <math.h>
#include <stdio.h>

#include "ports.h"

typedef struct {
    int port;          /* also the parameter ID in VST3 and the Audio Unit */
    const char* name;
    const char* short_name;
    float min, max, def;
    int log;           /* logarithmic */
    int steps;         /* 0: continuous; otherwise a switch with steps + 1 positions */
    const char* unit;  /* "Hz", "V", "dB", or "" for 0-1 knobs shown as 0-10 like the panel */
    const char* opts[3];
    int output;        /* read only: the opto lamps */
} BlParam;

static const BlParam BL_PARAMS[] = {
    { PORT_RATE1, "Sweep 1 Rate", "Rate 1", 0.1f, 18.0f, 0.5f, 1, 0, "Hz", { 0, 0, 0 }, 0 },
    { PORT_SHAPE1, "Sweep 1 Shape", "Shape 1", 0, 1, 0, 0, 1, "", { "Sine", "Square", 0 }, 0 },
    { PORT_RATE1_SRC, "Sweep 1 Rate Control", "Rate By", 0, 1, 0, 0, 1, "", { "Knob", "Pedal", 0 }, 0 },
    { PORT_RATE2, "Sweep 2 Rate", "Rate 2", 0.1f, 18.0f, 0.3f, 1, 0, "Hz", { 0, 0, 0 }, 0 },
    { PORT_SHAPE2, "Sweep 2 Shape", "Shape 2", 0, 1, 0, 0, 1, "", { "Sine", "Square", 0 }, 0 },
    { PORT_DEPTH_A, "Phasor A Depth", "Depth A", 0, 1, 0.8f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_FEEDBACK_A, "Phasor A Feedback", "Fdbk A", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_DEPTH_B, "Phasor B Depth", "Depth B", 0, 1, 0.8f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_FEEDBACK_B, "Phasor B Feedback", "Fdbk B", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_SWEEP_A, "Phasor A Sweep", "Sweep A", 0, 1, 0, 0, 1, "", { "Gen 1", "Pedal", 0 }, 0 },
    { PORT_SWEEP_B, "Phasor B Sweep", "Sweep B", 0, 2, 0, 0, 2, "", { "Gen 1", "Gen 2", "Pedal" }, 0 },
    { PORT_SYNC, "Sweep Sync", "Sync", 0, 1, 0, 0, 1, "", { "Normal", "Reverse", 0 }, 0 },
    { PORT_B_INPUT, "Phasor B Input", "B Input", 0, 2, 1, 0, 2, "", { "In A", "Out A", "In B" }, 0 },
    { PORT_ON_A, "Phasor A On", "A On", 0, 1, 1, 0, 1, "", { "Bypass", "On", 0 }, 0 },
    { PORT_ON_B, "Phasor B On", "B On", 0, 1, 1, 0, 1, "", { "Bypass", "On", 0 }, 0 },
    { PORT_PEDAL, "Pedal", "Pedal", 0, 1, 0.5f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_CENTER_A, "Center A Trim", "Center A", 0, 1, 0.15f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_CENTER_B, "Center B Trim", "Center B", 0, 1, 0.15f, 0, 0, "", { 0, 0, 0 }, 0 },
    { PORT_VOLTS, "Volts at 0 dBFS", "Volts", 0.1f, 10.0f, 1.0f, 1, 0, "V", { 0, 0, 0 }, 0 },
    { PORT_OUTPUT, "Output", "Output", -24.0f, 12.0f, 0.0f, 0, 0, "dB", { 0, 0, 0 }, 0 },
    { PORT_ENABLED, "Enabled", "Enabled", 0, 1, 1, 0, 1, "", { "Off", "On", 0 }, 0 },
    { PORT_LED_A, "Opto LED A", "LED A", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 1 },
    { PORT_LED_B, "Opto LED B", "LED B", 0, 1, 0, 0, 0, "", { 0, 0, 0 }, 1 },
};
#define BL_NUM_PARAMS ((int)(sizeof BL_PARAMS / sizeof BL_PARAMS[0]))

/* The first control port and the number of saved values: every input control, in port order. */
#define BL_FIRST_CONTROL PORT_RATE1
#define BL_NUM_INPUTS (PORT_ENABLED - PORT_RATE1 + 1)

static const BlParam* bl_param(int port)
{
    int i;
    for (i = 0; i < BL_NUM_PARAMS; ++i)
        if (BL_PARAMS[i].port == port)
            return &BL_PARAMS[i];
    return NULL;
}

static float bl_clamp(const BlParam* p, float v)
{
    if (!(v >= p->min)) /* also catches NaN */
        v = p->min;
    if (v > p->max)
        v = p->max;
    return p->steps ? floorf(v + 0.5f) : v;
}

/* 0-1 host value to plain value. Switches split 0-1 into equal parts, as the VST3 SDK does. */
static float bl_from_norm(const BlParam* p, double n)
{
    n = n < 0.0 ? 0.0 : (n > 1.0 ? 1.0 : n);
    if (p->steps) {
        int i = (int)(n * (p->steps + 1));
        return p->min + (float)(i > p->steps ? p->steps : i);
    }
    if (p->log)
        return (float)(p->min * pow((double)p->max / p->min, n));
    return (float)(p->min + n * (p->max - p->min));
}

static double bl_to_norm(const BlParam* p, float v)
{
    v = bl_clamp(p, v);
    if (p->steps)
        return (double)(v - p->min) / p->steps;
    if (p->log)
        return log((double)v / p->min) / log((double)p->max / p->min);
    return (double)(v - p->min) / (p->max - p->min);
}

/* Text for a value, matching the panel's display. */
static void bl_format(const BlParam* p, float v, char* buf, size_t n)
{
    v = bl_clamp(p, v);
    if (p->steps)
        snprintf(buf, n, "%s", p->opts[(int)(v - p->min)]);
    else if (p->unit[0] == 'd')
        snprintf(buf, n, "%+.1f", v);
    else if (p->unit[0])
        snprintf(buf, n, "%.2f", v);
    else if (p->output)
        snprintf(buf, n, "%.0f%%", 100.0f * v);
    else
        snprintf(buf, n, "%.1f", 10.0f * v);
}

/* The reverse of bl_format; returns 0 if the text means nothing. */
static int bl_parse(const BlParam* p, const char* s, float* out)
{
    double d;
    int i;
    for (i = 0; p->steps && i <= p->steps; ++i) {
        const char *a = p->opts[i], *b = s;
        while (*a && *b && (*a | 32) == (*b | 32))
            ++a, ++b;
        if (!*a && !*b) {
            *out = p->min + (float)i;
            return 1;
        }
    }
    if (sscanf(s, "%lf", &d) != 1)
        return 0;
    if (!p->unit[0] && !p->steps)
        d /= 10.0; /* 0-10 scale */
    *out = bl_clamp(p, (float)d);
    return 1;
}

#endif
