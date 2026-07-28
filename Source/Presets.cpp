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

#include "Presets.h"
#include "Parameters.h"

namespace dying
{

const std::vector<Preset>& getFactoryPresets()
{
    static const std::vector<Preset> presets =
    {
        { "Distant Nebula", {
            { pid::mix, 38 }, { pid::preDelay, 60 }, { pid::size, 62 }, { pid::decay, 58 },
            { pid::damping, 52 }, { pid::lowCut, 90 }, { pid::highCut, 9000 },
            { pid::diffusion, 74 }, { pid::shimmer, 22 }, { pid::shimPitch, 12 },
            { pid::detune, 9 }, { pid::modRate, 0.22f }, { pid::modDepth, 26 },
            { pid::collapse, 8 }, { pid::mass, 0 }, { pid::width, 130 }, { pid::output, 0 },
            { pid::freeze, 0 } } },

        { "Cathedral of Ice", {
            { pid::mix, 46 }, { pid::preDelay, 24 }, { pid::size, 78 }, { pid::decay, 74 },
            { pid::damping, 22 }, { pid::lowCut, 160 }, { pid::highCut, 16000 },
            { pid::diffusion, 86 }, { pid::shimmer, 44 }, { pid::shimPitch, 12 },
            { pid::detune, 6 }, { pid::modRate, 0.15f }, { pid::modDepth, 18 },
            { pid::collapse, 5 }, { pid::mass, 0 }, { pid::width, 150 }, { pid::output, -1 },
            { pid::freeze, 0 } } },

        { "Whisper of Light", {
            { pid::mix, 24 }, { pid::preDelay, 12 }, { pid::size, 40 }, { pid::decay, 44 },
            { pid::damping, 60 }, { pid::lowCut, 120 }, { pid::highCut, 7500 },
            { pid::diffusion, 68 }, { pid::shimmer, 14 }, { pid::shimPitch, 19 },
            { pid::detune, 4 }, { pid::modRate, 0.4f }, { pid::modDepth, 14 },
            { pid::collapse, 0 }, { pid::mass, 0 }, { pid::width, 110 }, { pid::output, 0 },
            { pid::freeze, 0 } } },

        { "Solar Wind", {
            { pid::mix, 52 }, { pid::preDelay, 140 }, { pid::size, 70 }, { pid::decay, 72 },
            { pid::damping, 38 }, { pid::lowCut, 70 }, { pid::highCut, 13000 },
            { pid::diffusion, 62 }, { pid::shimmer, 34 }, { pid::shimPitch, 7 },
            { pid::detune, 46 }, { pid::modRate, 0.9f }, { pid::modDepth, 58 },
            { pid::collapse, 18 }, { pid::mass, 8 }, { pid::width, 165 }, { pid::output, -1 },
            { pid::freeze, 0 } } },

        { "Supernova", {
            { pid::mix, 68 }, { pid::preDelay, 30 }, { pid::size, 88 }, { pid::decay, 88 },
            { pid::damping, 26 }, { pid::lowCut, 110 }, { pid::highCut, 17000 },
            { pid::diffusion, 82 }, { pid::shimmer, 78 }, { pid::shimPitch, 12 },
            { pid::detune, 22 }, { pid::modRate, 0.5f }, { pid::modDepth, 44 },
            { pid::collapse, 34 }, { pid::mass, 12 }, { pid::width, 175 }, { pid::output, -3 },
            { pid::freeze, 0 } } },

        { "Event Horizon", {
            { pid::mix, 76 }, { pid::preDelay, 0 }, { pid::size, 92 }, { pid::decay, 100 },
            { pid::damping, 30 }, { pid::lowCut, 45 }, { pid::highCut, 11000 },
            { pid::diffusion, 90 }, { pid::shimmer, 40 }, { pid::shimPitch, 12 },
            { pid::detune, 30 }, { pid::modRate, 0.12f }, { pid::modDepth, 34 },
            { pid::collapse, 26 }, { pid::mass, 30 }, { pid::width, 160 }, { pid::output, -4 },
            { pid::freeze, 1 } } },

        { "Gravity Well", {
            { pid::mix, 64 }, { pid::preDelay, 80 }, { pid::size, 84 }, { pid::decay, 86 },
            { pid::damping, 62 }, { pid::lowCut, 25 }, { pid::highCut, 4200 },
            { pid::diffusion, 78 }, { pid::shimmer, 10 }, { pid::shimPitch, -12 },
            { pid::detune, 34 }, { pid::modRate, 0.08f }, { pid::modDepth, 40 },
            { pid::collapse, 44 }, { pid::mass, 62 }, { pid::width, 90 }, { pid::output, -5 },
            { pid::freeze, 0 } } },

        { "Black Hole Roar", {
            { pid::mix, 92 }, { pid::preDelay, 0 }, { pid::size, 96 }, { pid::decay, 97 },
            { pid::damping, 74 }, { pid::lowCut, 20 }, { pid::highCut, 2600 },
            { pid::diffusion, 94 }, { pid::shimmer, 26 }, { pid::shimPitch, -12 },
            { pid::detune, 68 }, { pid::modRate, 0.05f }, { pid::modDepth, 66 },
            { pid::collapse, 88 }, { pid::mass, 92 }, { pid::width, 70 }, { pid::output, -8 },
            { pid::freeze, 0 } } },
    };

    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, int index)
{
    const auto& presets = getFactoryPresets();

    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (const auto& v : presets[(size_t) index].values)
    {
        if (auto* p = state.getParameter (v.id))
            p->setValueNotifyingHost (p->convertTo0to1 (v.value));
    }
}

} // namespace dying
