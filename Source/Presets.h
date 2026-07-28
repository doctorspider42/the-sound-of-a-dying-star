/*
    The Sound of a Dying Star - a cosmic shimmer reverb
    Copyright (C) 2026 doctorspider42

    This program is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later version.

    It links against JUCE 8 under the AGPLv3, so every distributed binary additionally
    carries AGPLv3 section 13. See LICENSE and LICENSE.AGPLv3.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
    PARTICULAR PURPOSE. See the GNU General Public License for more details.
*/

/*  Starting points that span the range of the instrument, from a barely-there halo to
    something that eats the source. They exist mostly so that the two extremes the
    plug-in was designed for are one click away instead of being buried in seventeen
    knobs.                                                                          */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace dying
{

struct PresetValue
{
    const char* id;
    float value;
};

struct Preset
{
    juce::String name;
    std::vector<PresetValue> values;
};

const std::vector<Preset>& getFactoryPresets();

/** Applies a preset through the parameters themselves, so the host sees the changes,
    undo works, and any attached UI follows automatically. */
void applyPreset (juce::AudioProcessorValueTreeState& state, int index);

} // namespace dying
