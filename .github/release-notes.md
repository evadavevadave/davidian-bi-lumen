Ready-made **Davidian Bi-Lumen** plugins for Windows, macOS and Linux, built and tested from the same source by GitHub Actions. New in 1.2: a **VST3** for Windows and an **Audio Unit** for macOS.

## Downloads

| File | For | Contains |
|---|---|---|
| `...-windows-x64.zip` | 64-bit Windows 10 and 11 | VST3 and LV2 |
| `...-macos-universal.zip` | macOS 10.13 and later, Apple silicon and Intel | Audio Unit and LV2 |
| `...-linux-x64.zip` | 64-bit Linux on x86 (glibc 2.29 or later, such as Ubuntu 20.04 or Debian 11) | LV2 |
| `...-linux-arm64.zip` | 64-bit Linux on ARM, such as a Raspberry Pi 4 or 5 | LV2 |

## Install

Unzip, close your host, and copy the format it uses:

- **VST3 (Windows):** copy the `Davidian Bi-Lumen.vst3` folder into `C:\Program Files\Common Files\VST3`.
- **Audio Unit (macOS):** copy `Davidian Bi-Lumen.component` into `~/Library/Audio/Plug-Ins/Components`. It isn't notarised, so clear the download quarantine once in Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Davidian Bi-Lumen.component"`.
- **LV2:** copy the `bi_lumen.lv2` folder into `%APPDATA%\LV2` (Windows), `~/Library/Audio/Plug-Ins/LV2` (macOS, then clear the quarantine the same way) or `~/.lv2` (Linux).

Restart the host (or rescan its plugins) and look for **Bi-Lumen** by **Davidian**.

## Changes since 1.1

- **VST3 for Windows:** the same circuit and panel. Every move on the panel is recorded as automation, and the host's bypass crossfades to the dry signal.
- **Audio Unit (AUv2) for macOS**, a universal binary for Apple silicon and Intel: the same circuit and panel as a Cocoa view, with stereo or mono input.
- All formats share one engine and give identical output. Settings mean the same thing in every format.
- The LV2 plugin's sound, controls and URI are unchanged from 1.1.

## Testing status

- Every build passes the DSP tests and the GUI render test.
- **VST3:** passes Steinberg's validator. A test host drives its editor with simulated mouse input, and its output matches the LV2 build sample for sample.
- **Audio Unit:** passes Apple's `auval` in strict mode, for both Apple silicon and Intel. A test host renders it, and its output matches the LV2 build sample for sample. It also checks a mono input and opens the Cocoa view.
- The Linux bundles are also checked with the lilv LV2 parser.
- Not yet tried in a DAW by a person: the new VST3 and Audio Unit, and the macOS and Linux GUIs.

## Licence and AI usage

- **Licence:** GPL-3.0-or-later. The licence, `NOTICE.md` and `AI_USAGE.md` are inside each bundle. The VST3 includes the Steinberg VST3 SDK (MIT) and the Audio Unit includes Apple's AudioUnitSDK (Apache 2.0). Their licences are in the bundles too.
- **AI usage:** this plugin was made with generative AI (Claude Code). See `AI_USAGE.md`.

VST is a registered trademark of Steinberg Media Technologies GmbH.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
