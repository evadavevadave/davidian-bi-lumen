# AI usage

This plugin was built with generative AI.

- **Tool:** Claude Code, Anthropic's coding assistant, using Anthropic's Claude models (Opus).
- **Human role:** Davidian set the requirements and the design direction, supplied the reference schematic, and decides what is released.
- **What the AI did:**
  - Read the component values off the reference schematic.
  - Wrote the circuit model, the LV2 plugin and GUI code, the stroke font and the panel artwork (drawn in code).
  - Wrote the VST3 and Audio Unit wrappers around the same circuit and panel code.
  - Wrote the build files, the LV2 metadata, the tests and the documentation.
  - Looked up published specifications online to check the model.
- **How it was checked:**
  - Automated tests in `test/`: transfer function against an independent s-domain solution, sweep rates against the owner's manual, plugin and GUI loading, and the lilv LV2 validator.
  - The VST3 and the Audio Unit are hosted by test programs that compare their output with the LV2 build sample for sample, and drive the VST3 editor with simulated mouse input. They also pass Steinberg's VST3 validator and Apple's `auval`.
  - CMake (`ctest`) and `build.bat` build the tests, and the README lists the commands that run them.
  - No listening test in a DAW has been recorded yet.
- **Training data and copyright:** no third-party code was knowingly copied. The only reproduced material is the LV2 interface declarations, credited in [NOTICE.md](NOTICE.md). The VST3 and Audio Unit builds link the Steinberg VST3 SDK and Apple's AudioUnitSDK unmodified, under their own licences (also in NOTICE.md). The GUI artwork and font are generated from code written for this project, not from images.

The AI's output is released under the project licence, GPL-3.0-or-later (see `LICENSE`). Under the GPL, there is no warranty.
