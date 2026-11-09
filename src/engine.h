/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The Bi-Lumen engine as a small C API, shared by the LV2, VST3 and Audio Unit builds.
 * It follows the LV2 model: every port in ports.h is a float pointer the wrapper connects,
 * control ports read one value per block, and the opto lamp outputs are written per block. */
#ifndef BL_ENGINE_H
#define BL_ENGINE_H

#include <stdint.h>

#include "ports.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BlEngine BlEngine;

BlEngine* bl_engine_new(double sample_rate); /* allocates; not real-time safe */
void bl_engine_connect(BlEngine* e, uint32_t port, void* data);
void bl_engine_activate(BlEngine* e); /* resets the circuit; connect the controls first */
void bl_engine_run(BlEngine* e, uint32_t frames); /* in place is fine: each sample is read before it is written */
void bl_engine_free(BlEngine* e);

#ifdef __cplusplus
}
#endif

#endif
