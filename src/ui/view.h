/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* The Bi-Lumen panel in a native window: a Win32 child window, a Cocoa NSView or an X11 window.
 * The LV2 UI, the VST3 editor and the Audio Unit view all use this, each with its own host
 * callbacks. view.c is compiled as Objective-C on macOS. */
#ifndef BL_VIEW_H_INCLUDED
#define BL_VIEW_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define BL_VIEW_W 960
#define BL_VIEW_H 470

typedef struct BlView BlView;

typedef struct {
    void* host;
    /* A plain value (as in ports.h) the user set on the panel. */
    void (*write)(void* host, int port, float value);
    /* Optional: brackets each edit, begin = 1 then begin = 0. */
    void (*gesture)(void* host, int port, int begin);
    /* Optional: called about 30 times a second on the UI thread, before the panel redraws. */
    void (*tick)(void* host);
} BlViewHost;

/* Opens the panel inside parent (an HWND, NSView* or X11 Window, cast to void*), or as its own
 * view or window when parent is NULL. Returns NULL on failure. */
BlView* bl_view_open(void* parent, const BlViewHost* host);
/* The HWND, NSView* or X11 Window. */
void* bl_view_widget(BlView* v);
/* A value from the host; it shows on the next redraw. */
void bl_view_set(BlView* v, int port, float value);
/* X11 has no event loop of its own here, so the host's idle call pumps it. Returns 0. */
int bl_view_idle(BlView* v);
void bl_view_close(BlView* v);

#ifdef __cplusplus
}
#endif

#endif
