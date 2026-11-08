# AI usage

This plugin was built with generative AI.

- **Tool:** Claude Code, Anthropic's coding assistant, using Anthropic's Claude models (Opus).
- **Human role:** Davidian set the requirements and the design direction, supplied the reference schematic, and decides what is released.
- **What the AI did:**
  - Read the component values off the reference schematic.
  - Wrote the circuit model, the LV2 plugin and GUI code, the stroke font and the panel artwork (drawn in code).
  - Wrote the build files, the LV2 metadata, the tests and the documentation.
  - Looked up published specifications online to check the model.
- **How it was checked:**
  - Automated tests in `test/`: transfer function against an independent s-domain solution, sweep rates against the owner's manual, plugin and GUI loading, and the lilv LV2 validator.
  - `build.bat` builds the tests, and the README lists the commands that run them.
  - No listening test in a DAW has been recorded yet.
- **Training data and copyright:** no third-party code was knowingly copied. The only reproduced material is the LV2 interface declarations, credited in [NOTICE.md](NOTICE.md). The GUI artwork and font are generated from code written for this project, not from images.

The AI's output is released under the project licence, GPL-3.0-or-later (see `LICENSE`). Under the GPL, there is no warranty.
