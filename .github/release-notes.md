Ready-made **Davidian Bi-Lumen** bundles for Windows, macOS and Linux, built and tested from the same source by GitHub Actions.

## Downloads

| File | For |
|---|---|
| `...-windows-x64.zip` | 64-bit Windows 10 and 11 |
| `...-macos-universal.zip` | macOS 10.13 and later, Apple silicon and Intel |
| `...-linux-x64.zip` | 64-bit Linux on x86 (glibc 2.29 or later, such as Ubuntu 20.04 or Debian 11) |
| `...-linux-arm64.zip` | 64-bit Linux on ARM, such as a Raspberry Pi 4 or 5 |

## Install

1. Unzip and copy the `bi_lumen.lv2` folder into your LV2 folder:
   - Windows: `%APPDATA%\LV2`
   - macOS: `~/Library/Audio/Plug-Ins/LV2`
   - Linux: `~/.lv2`
2. **macOS only:** the libraries aren't notarised, so clear the download quarantine once with `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/LV2/bi_lumen.lv2`.
3. Restart your host and look for **Davidian Bi-Lumen**.

## Changes since 1.0

- **macOS and Linux builds** are now provided, with the native GUI on every platform: a new Cocoa (NSView) window layer on macOS, and X11 on Linux.
- **Denormal flushing on 64-bit ARM** (Apple silicon, Linux arm64) as well as x86, so CPU use stays flat as sounds decay.
- **`build.bat` finds any Visual Studio version** (2017 or later) automatically.
- **`cmake --install`** support.

The sound and the controls are unchanged from 1.0.

## Testing status

- Every build passes the DSP tests and the GUI render test.
- The Linux bundles are also checked with the lilv LV2 parser.
- The Windows bundle has also been run in a test host, with the GUI driven by simulated mouse input.
- The macOS and Linux GUIs compile, but haven't yet been opened in a DAW.

## Licence and AI usage

- **Licence:** GPL-3.0-or-later. The licence, `NOTICE.md` and `AI_USAGE.md` are inside each bundle.
- **AI usage:** this plugin was made with generative AI (Claude Code). See `AI_USAGE.md`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
