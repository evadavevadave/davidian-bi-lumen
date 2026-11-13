# Davidian Bi-Lumen

A circuit-modelled phaser pedal plugin (LV2, VST3 and Audio Unit), modelled part by part on the schematic of a classic dual opto phaser: two six-stage LDR phasors with regeneration, two triangle/square sweep generators, the opto LED drivers, and LDR footswitch bypass.

**A close model, not an exact copy.** It is a simulation of the circuit from its component values, not a sampled or measured pedal, and some details are reasoned estimates (listed under [Estimates](#estimates)). It has not been compared side by side with a built unit, so expect it to sound close to the hardware rather than identical.

![The GUI](docs/gui.png)

Stereo in and out (A and B) with no latency; a mono input feeds both phasor inputs. Every format is built from the same circuit code, shows the same GUI and sounds identical. GPL-3.0-or-later.

| Format | Windows | macOS | Linux |
|---|---|---|---|
| LV2 | yes | yes | yes |
| VST3 | yes | | |
| Audio Unit (AUv2) | | yes | |

## Download

Ready-made builds are on the [Releases page](https://github.com/evadavevadave/davidian-bi-lumen/releases):

| Download | Contains | For |
|---|---|---|
| `…-windows-x64.zip` | LV2, VST3 | Windows 10/11, 64-bit hosts |
| `…-macos-universal.zip` | LV2, Audio Unit | macOS 10.13 or later, Apple silicon and Intel |
| `…-linux-x64.zip` | LV2 | 64-bit Intel/AMD Linux (glibc 2.29 or later, e.g. Ubuntu 20.04 or Debian 11; X11 or XWayland for the GUI) |
| `…-linux-arm64.zip` | LV2 | 64-bit ARM Linux, e.g. Raspberry Pi 4/5 with a 64-bit OS |

Unzip, copy the plugin folder for your host's format into its plugin folder, and restart your host (or rescan plugins):

| Format | Copy | Into |
|---|---|---|
| VST3 (Windows) | `Davidian Bi-Lumen.vst3` | `C:\Program Files\Common Files\VST3` |
| Audio Unit (macOS) | `Davidian Bi-Lumen.component` | `~/Library/Audio/Plug-Ins/Components` (or `/Library/Audio/Plug-Ins/Components` for all users) |
| LV2 (Windows) | `bi_lumen.lv2` | `%APPDATA%\LV2` (create it if it doesn't exist) |
| LV2 (macOS) | `bi_lumen.lv2` | `~/Library/Audio/Plug-Ins/LV2` |
| LV2 (Linux) | `bi_lumen.lv2` | `~/.lv2` |

In the host it is listed as **Davidian Bi-Lumen** (VST3, LV2; under Modulation effects in VST3 hosts) or under **Davidian** as **Bi-Lumen** (Audio Unit; manufacturer code `Dvdn`, subtype `Bilm`). Replace any older copy instead of keeping two. The LV2 URI is `urn:davidian:bi-lumen`. It changed in the 2026-11 rename, so sessions saved before then need the plugin re-inserted.

The macOS build isn't notarized, so macOS may block it after download. Run these once in Terminal after copying:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Davidian Bi-Lumen.component"
```

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/LV2/bi_lumen.lv2
```

Logic Pro and GarageBand only show an Audio Unit once macOS has registered it. If it doesn't appear, log out and back in, or restart the Mac. In Logic, Plug-in Manager can rescan it, and `auval -v aufx Bilm Dvdn` in Terminal shows why it fails if it does.

## Controls

| Panel | Control | Circuit part |
|---|---|---|
| Sweep Gen 1 | Rate | VR1 (B25k), 0.1 to 18 Hz, default 0.5. Set in Hz; the plugin works out the pot position |
| Sweep Gen 1 | Shape: Sine / Square | SW3 |
| Sweep Gen 1 | Rate By: Knob / Pedal | SW2 "Rate control" |
| Sweep Gen 2 | Rate | VR3 (A25k), 0.1 to 18 Hz, default 0.3 |
| Sweep Gen 2 | Shape: Sine / Square | SW4 |
| Phasor A, B | Depth | VR2 / VR4 (B10k), "Sweep vol", default 0.8 |
| Phasor A, B | Feedback | VR5 / VR6 (B10k), default 0 |
| Phasor A | Sweep: Gen 1 / Pedal | SW6 |
| Phasor B | Sweep: Gen 1 / Gen 2 / Pedal | SW5 |
| Phasor B | Input: In A / Out A / In B | SW1, default Out A |
| Phasor B | Sync: Normal / Reverse | SW7, the U16B inverter |
| Expression pedal | Pedal | Expression input, 0–5 V, default 0.5 |
| Calibrate | Center A, B | TR3 / TR4 (the "??" trims), default 0.15. Moves the whole sweep up or down |
| Levels | Input V | Volts at the input jack for a 0 dBFS sample (0.1 to 10, default 1.0) |
| Levels | Output | Not on the unit: -24 to +12 dB, default 0 dB |
| Footswitches | Phasor A, Phasor B | SW8 / SW9 footswitches, with red LEDs |
| (host) | Enabled | Plugin bypass with a 20 ms crossfade. In VST3 it is the host's bypass switch, and in an Audio Unit host's bypass turns it off. |

"Sine" is the triangle wave. The LED driver curves it, so the sweep sounds rounded. "Square" is the RC-rounded square. The SW3 labels on the schematic are on the opposite throws: the throw labelled "square" gets the triangle through R118. The plugin names each position by the waveform it actually passes. With the default Out A setting, Out B is all twelve stages in series.

The amber OPTO lamps glow with each phasor's LED current, read from the plugin's `led_a` and `led_b` output ports. The readout shows the name and value of the last control you touched.

Drag a knob up or down, hold Shift to fine-tune, use the mouse wheel, and double-click to reset. Click the upper, middle or lower part of a toggle to pick that position, and click or drag along the expression pedal slider. In VST3 and Audio Unit hosts every move on the panel is a proper edit, so automation records it.

## How it is modelled

All with the schematic's part values, in `src/phasor.h`, `src/opto.h` and `src/sweep.h`.

- **Audio path (per phasor):** C1/R15 (390k input impedance), U1A buffer, then U1B inverting with the feedback injected on its + input. Six all-pass stages follow, each with 6.8 nF and an LDR in parallel with 220k. C20 470 nF then feeds the VR5 feedback pot and the U2B inverter. U2A mixes the result with the dry signal: dry through R50 12k, wet through R16 + TR1 + C21 + R49. Notches fall where the wet and dry signals cancel. The feedback loop is solved within each sample.
- **Bypass:** R16 on the schematic is an LDR. The footswitch lights LDR-DRIVE-A1 at 7.2 mA, which turns R16 on and lets the wet signal reach the mix. With the switch off, R16 goes dark and only the dry signal is left (wet leak about -45 dB). The LDR's own lag fades the effect in and out without a click.
- **Sweep 1:** a voltage-controlled triangle/square oscillator (U13 rectifier and inverter, D8–D11 bridge, U12B integrator, U12A Schmitt trigger). TR8 sets its slowest rate.
- **Sweep 2:** a fixed op amp function generator (U15).
- **LED drivers:** each is U11 with a BC550C follower. D4 in the feedback network expands the curve, so the LED current rises faster near the top of the sweep.

### Checked against the schematic and published specs

| Item | Model | Source |
|---|---|---|
| Sweep 1 rate range | 0.100–21.6 Hz | Owner's manual: 0.1–18 Hz |
| Sweep 2 rate range | 0.094–19.7 Hz | Owner's manual: 0.1–18 Hz |
| Input impedance | 390k, or 195k with B on In A | Manual: 390k single, 180k parallel |
| Pedal voltage | 0–5 V | Polyphony, Feb 1978: pedal sends 0–5 V |
| Phasor structure | 2 × 6 stages, 12 in series | Manual |
| Phasor B sweep source | Gen 1 / Gen 2 / Pedal; Sync Normal/Rev | Manual |

The upper rate limits come out 10–20% above the manual's figure. That fits the RC4558's real output swing and the trim positions. `test_dsp` also compares the simulated audio path with the schematic's s-domain transfer function. They agree to within 0.04% across LDR values, feedback settings and sample rates.

Default sweep (Center 0.15, Depth 0.8): the LED current runs from 0.28 to 10.2 mA. The lowest notch sweeps from about 205 Hz to 1.5 kHz, with the others at about 3× and 10× that. Feedback adds peaks of about +4.7 dB between the notches.

### Estimates

These values are not on the schematic, so they are reasoned estimates:

- **LDR response** (`src/opto.h`): R = 25k at 1 mA LED current, scaling with current to the power -0.75. Dark resistance 2 MΩ, 5 ms attack and 25 ms release. The LED and LDR are hand-coupled 5 mm parts with no stated values, and this sets the sweep range more than anything else.
- LED forward voltage 2.0 V and RC4558 swing ±13.5 V (`src/opto.h`, `src/circuit.h`).
- Trim positions: TR5/TR6 500 Ω, TR8 0.5 (which gives the manual's 0.1 Hz minimum), TR9 11k, TR10 8.5k (matches Sweep 2's level to Sweep 1), TR7 0. TR1/TR2 are set so the wet and dry mix resistors match (about 1.6k).
- The Center trims default to 0.15. At the schematic's midpoint the top of the sweep drives U11 into saturation.

### Simplifications

- Op amps are ideal apart from clipping at U1B, U2B and U2A.
- All six LDRs in a module are identical.
- The LED indicator transistors (Q1, Q4) and D7 in the pedal rate path are left out. They don't affect the sound.
- The pedal rate path tops out near 1 Hz, as the R91/R88 divider dictates.

## Build

The LV2 build is plain C11 (the macOS window layer is compiled as Objective-C) and needs no LV2 SDK or other libraries. The VST3 and the Audio Unit need their SDKs, which CMake downloads while configuring: the Steinberg VST3 SDK 3.8.1 (MIT) and Apple's AudioUnitSDK 1.2.0 (Apache 2.0).

| Source | What it is |
|---|---|
| `src/phasor.h`, `src/opto.h`, `src/sweep.h`, `src/circuit.h` | The circuit model: phasor audio path, LED driver and LDR, sweep generators, shared constants and op amp clipping |
| `src/engine.c`, `src/engine.h` | Routing, controls and the per-sample loop, behind a small C API that every format uses |
| `src/params.h`, `src/ports.h` | Parameter names, ranges and text for VST3 and Audio Units, and the LV2 port numbers they match |
| `src/plugin.c`, `src/ui/ui_lv2.c` | LV2 plugin and GUI |
| `src/vst3/` | VST3 processor, controller and editor, and its CMake build |
| `src/au/` | Audio Unit, its Cocoa view and its Info.plist |
| `src/ui/panel.h`, `src/ui/view.c` | The panel and the window layer under it (Win32, Cocoa, X11), shared by every GUI |
| `src/ui/canvas.h`, `src/ui/font.h`, `src/ui/style.h` | Davidian GUI house style: software renderer, stroke font and palette |
| `src/lv2_abi.h` | The LV2 C interface, so no SDK is needed |
| `bundle/` | LV2 metadata |
| `licenses/` | The VST3 SDK and AudioUnitSDK licences, shipped inside the bundles that use them |

### Windows (Visual Studio)

Needs Visual Studio 2017 or later with the C++ tools. `build.bat` builds the LV2 bundle, `build\bi_lumen.lv2`, and the Windows-only LV2 checks, without CMake.

```bash
build.bat
```

### Every platform (CMake)

Needs CMake 3.25 or later and a C/C++ compiler. On macOS, the Xcode command-line tools; add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for a universal build. On Linux, gcc or clang plus the X11 headers (`libx11-dev` on Debian/Ubuntu) for the GUI; without them CMake builds the plugin alone.

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build-cmake --config Release
```

The VST3 lands in `build-cmake\VST3\Release` (Windows) and the Audio Unit in `build-cmake/AU` (macOS, signed ad hoc). They are on by default on Windows and macOS respectively; turn them off with `-DBI_LUMEN_VST3=OFF` or `-DBI_LUMEN_AU=OFF`, for example to build offline. `-DBI_LUMEN_VST3=ON` also builds a VST3 on macOS, untested.

To copy the LV2 bundle into `~/.lv2` (Linux):

```bash
cmake --install build-cmake --prefix ~/.lv2 -DLV2_INSTALL_DIR=.
```

Without `-DLV2_INSTALL_DIR=.`, `cmake --install` puts it in `<prefix>/lib/lv2`.

## Tests

Windows, with `build.bat`:

```bash
build-test\test_dsp.exe build\bi_lumen.lv2
```

```bash
build-test\lilv_check.exe build
```

```bash
build-test\ui_check.exe build\bi_lumen.lv2
```

Every platform, with CMake:

```bash
ctest --test-dir build-cmake -C Release
```

- `test_dsp` checks the circuit against its transfer function, the sweep rates and ranges, and the built LV2 library.
- `ui_render` draws the panel to a BMP file without opening a window, on any platform: `build-test\ui_render.exe out.bmp`.
- `ui_check` (Windows) hosts the built GUI and drives it with mouse messages.
- `lilv_check` (Windows) validates the bundle with the lilv LV2 parser, without installing anything.
- `params_check` checks the VST3 and Audio Unit parameters against the LV2 ports and the panel.
- `vst3_check` (Windows) hosts the VST3, clicks and drags on its editor and checks the edits it reports, then compares its sound with the LV2 build. The VST3 build also runs Steinberg's validator.
- `au_check` (macOS) loads the Audio Unit from its bundle, compares its sound with the LV2 build (stereo, and mono input), and opens its view. CI also runs Apple's `auval` on it.

Both sound comparisons cover parameter changes and the host's bypass, and expect the output to match the LV2 build exactly.

GitHub Actions builds and tests all four platforms from the same source on every push (see `.github/workflows/build.yml`), and publishes a release when a version is tagged.

## Use of AI

Made with generative AI (Claude Code), directed and reviewed by Davidian. See [AI_USAGE.md](AI_USAGE.md) for what the author and the AI each did, and what has and hasn't been checked.

## References

- [Owner's manual of the original unit (Soundgas)](https://soundgas.com/resources/mutron-biphase-owners-manual): rates, input impedance, switch functions.
- [Polyphony, Feb 1978: modifying the original unit (muzines)](https://www.muzines.co.uk/articles/modifying-the-mu-tron-bi-phase/12300): pedal voltage range.
- [Effects Database entry for the original unit](https://www.effectsdatabase.com/model/musitronics/mutron/biphase): background.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md) and [AI_USAGE.md](AI_USAGE.md). Every source file carries an `SPDX-License-Identifier` line. `src/lv2_abi.h` is under the ISC licence, which is GPL-compatible, because it reproduces the LV2 interface declarations. The VST3 plugin includes code from the Steinberg VST3 SDK (MIT) and the Audio Unit from Apple's AudioUnitSDK (Apache 2.0), both GPL-3.0-compatible. The build copies `LICENSE`, `NOTICE.md` and `AI_USAGE.md` into every bundle. VST is a registered trademark of Steinberg Media Technologies GmbH.
