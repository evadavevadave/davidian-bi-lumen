# Notices and licences

## Davidian Bi-Lumen

Copyright (C) 2026 Davidian

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program (`LICENSE`). If not, see <https://www.gnu.org/licenses/>.

GPL-3.0-or-later covers everything in this repository unless listed below: the DSP and GUI source, the tests, the build files, the LV2 metadata (`bundle/`), the GUI artwork and stroke font (both drawn in code), `docs/gui.png` (rendered by the plugin's own GUI code) and the documentation.

AI was used to make this software. See [AI_USAGE.md](AI_USAGE.md).

## Third-party material

### LV2 interface declarations (ISC)

`src/lv2_abi.h` and the UI ABI block at the top of `src/ui/ui_lv2.c` declare the LV2 C interface with the same names and layout as the LV2 headers, so the build needs no LV2 SDK. No other LV2 code is included.

- `lv2/core/lv2.h`: Copyright 2006-2020 Steve Harris, David Robillard
- `lv2/ui/ui.h`: Copyright 2009-2016 David Robillard, Lars Luthman

> Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
>
> THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

ISC is compatible with the GPL, so the combined work is distributed under GPL-3.0-or-later.

### Used by the build but not included

| Component | Licence | How it is used |
|---|---|---|
| Microsoft C runtime | Microsoft Visual Studio licence | Linked statically into the Windows DLLs by `build.bat` (`/MT`). It is a system library under GPL-3.0 section 1. |
| Xlib (libX11) | MIT/X11 | Linked dynamically by the Linux GUI. |
| Cocoa, Core Graphics | Apple system frameworks | Linked dynamically by the macOS GUI. |
| GitHub Actions (actions/checkout, upload-artifact, download-artifact) | MIT | Used to build the release bundles. Not shipped. |
| lilv, from the Ardour 9 install | ISC | Loaded at run time by `test/lilv_check.c` only. Not shipped in the bundle. |
| Win32 user32/gdi32, C maths library | System libraries | Linked dynamically. |

## Circuit and reference sources

- **Circuit:** the model follows the component values and topology of a DIY clone schematic by f.aiseur (V002, Rev 002, 2025-01-25). Component values and circuit topology are facts used to write an original simulation. None of the schematic image is included in this repository or the bundle, and the copy used during development has been deleted.
- **Published figures:** rate ranges, input impedance and pedal voltage come from the original unit's owner's manual and a 1978 magazine article (linked in the README). Only those figures were used, and no text or images from them are included.
- **Trademarks:** the original hardware's brand and model names are trademarks of their owners. They are not used in the plugin's name, URI, files or GUI, and Davidian is not affiliated with them. "Bi-Lumen" is an original name.
- **GUI design:** original. It does not copy the original unit's logo, wordmark, lettering or panel artwork.
