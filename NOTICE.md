# Third-party components

## JUCE 8

- Version: 8.0.15 (pinned as the submodule `libs/JUCE`)
- Licence: **AGPLv3**, as used here — see `libs/JUCE/LICENSE.md`
- Home: <https://juce.com>

Because JUCE is linked under the AGPLv3, every binary distributed from this project is
a combined work carrying AGPLv3 section 13 in addition to the GPLv3 terms of the code in
`Source/` and `tools/`. Both texts ship with every release: `LICENSE` (GPLv3) and
`LICENSE.AGPLv3`.

JUCE itself is not redistributed in the archives — the JUCE EULA forbids that. It is
pinned as a submodule so the exact revision behind any binary is recorded in this
repository's history, and every release archive names both SHAs in `BUILD-INFO.txt`.

## VST3 SDK

The VST3 SDK subset bundled with JUCE 8.0.15 lives at
`libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/` and is
distributed by Steinberg under the **MIT licence** (© 2025 Steinberg Media Technologies
GmbH). Check `LICENSE.txt` in that directory for the copy actually vendored here.

> VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and
> other countries.

No Steinberg logo is used anywhere in this project. Using the VST logo requires a
separate usage agreement with Steinberg, which this project does not hold.

## Fonts

No fonts are shipped. The panel asks the operating system for its default sans-serif
face, so it renders with whatever the host machine already has installed.
