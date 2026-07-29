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

/*  Every preset sets every parameter, so recalling one is never a partial state on top
    of whatever was there before.

    The presets that predate the delay section keep it switched off - they sound as they
    always did - but they still park its controls somewhere musical, so Engage is a
    decision the panel makes easy rather than a patch somebody has to build from
    scratch. The four at the end are built around it.                                */

const std::vector<Preset>& getFactoryPresets()
{
    static const std::vector<Preset> presets =
    {
        { "Distant Nebula", {
            { pid::mix, 38 }, { pid::preDelay, 60 }, { pid::size, 62 }, { pid::space, 45 },
            { pid::decay, 58 }, { pid::diffusion, 74 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 52 }, { pid::lowCut, 90 }, { pid::highCut, 9000 },
            { pid::shimmer, 22 }, { pid::shimPitch, 12 },
            { pid::detune, 9 }, { pid::modRate, 0.22f }, { pid::modDepth, 26 },
            { pid::collapse, 8 }, { pid::mass, 0 }, { pid::width, 130 }, { pid::output, 0 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 480 }, { pid::delayFeed, 38 },
            { pid::delaySpread, 70 }, { pid::delayShimmer, 26 }, { pid::delayPitch, 12 },
            { pid::delayTone, 45 }, { pid::delayWobble, 25 }, { pid::delayMorph, 15 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 0 }, { pid::delayMix, 28 } } },

        { "Cathedral of Ice", {
            { pid::mix, 46 }, { pid::preDelay, 24 }, { pid::size, 78 }, { pid::space, 62 },
            { pid::decay, 74 }, { pid::diffusion, 86 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 22 }, { pid::lowCut, 160 }, { pid::highCut, 16000 },
            { pid::shimmer, 44 }, { pid::shimPitch, 12 },
            { pid::detune, 6 }, { pid::modRate, 0.15f }, { pid::modDepth, 18 },
            { pid::collapse, 5 }, { pid::mass, 0 }, { pid::width, 150 }, { pid::output, -1 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 620 }, { pid::delayFeed, 34 },
            { pid::delaySpread, 65 }, { pid::delayShimmer, 40 }, { pid::delayPitch, 12 },
            { pid::delayTone, 25 }, { pid::delayWobble, 18 }, { pid::delayMorph, 20 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 0 }, { pid::delayMix, 26 } } },

        { "Whisper of Light", {
            { pid::mix, 24 }, { pid::preDelay, 12 }, { pid::size, 40 }, { pid::space, 35 },
            { pid::decay, 44 }, { pid::diffusion, 68 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 60 }, { pid::lowCut, 120 }, { pid::highCut, 7500 },
            { pid::shimmer, 14 }, { pid::shimPitch, 19 },
            { pid::detune, 4 }, { pid::modRate, 0.4f }, { pid::modDepth, 14 },
            { pid::collapse, 0 }, { pid::mass, 0 }, { pid::width, 110 }, { pid::output, 0 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 320 }, { pid::delayFeed, 28 },
            { pid::delaySpread, 55 }, { pid::delayShimmer, 20 }, { pid::delayPitch, 19 },
            { pid::delayTone, 50 }, { pid::delayWobble, 20 }, { pid::delayMorph, 10 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 0 }, { pid::delayMix, 22 } } },

        { "Solar Wind", {
            { pid::mix, 52 }, { pid::preDelay, 140 }, { pid::size, 70 }, { pid::space, 50 },
            { pid::decay, 72 }, { pid::diffusion, 62 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 38 }, { pid::lowCut, 70 }, { pid::highCut, 13000 },
            { pid::shimmer, 34 }, { pid::shimPitch, 7 },
            { pid::detune, 46 }, { pid::modRate, 0.9f }, { pid::modDepth, 58 },
            { pid::collapse, 18 }, { pid::mass, 8 }, { pid::width, 165 }, { pid::output, -1 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 540 }, { pid::delayFeed, 48 },
            { pid::delaySpread, 80 }, { pid::delayShimmer, 30 }, { pid::delayPitch, 7 },
            { pid::delayTone, 40 }, { pid::delayWobble, 45 }, { pid::delayMorph, 30 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 8 }, { pid::delayMix, 32 } } },

        { "Supernova", {
            { pid::mix, 68 }, { pid::preDelay, 30 }, { pid::size, 88 }, { pid::space, 55 },
            { pid::decay, 88 }, { pid::diffusion, 82 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 26 }, { pid::lowCut, 110 }, { pid::highCut, 17000 },
            { pid::shimmer, 78 }, { pid::shimPitch, 12 },
            { pid::detune, 22 }, { pid::modRate, 0.5f }, { pid::modDepth, 44 },
            { pid::collapse, 34 }, { pid::mass, 12 }, { pid::width, 175 }, { pid::output, -3 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 380 }, { pid::delayFeed, 55 },
            { pid::delaySpread, 60 }, { pid::delayShimmer, 50 }, { pid::delayPitch, 12 },
            { pid::delayTone, 30 }, { pid::delayWobble, 35 }, { pid::delayMorph, 25 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 15 }, { pid::delayMix, 36 } } },

        { "Event Horizon", {
            { pid::mix, 76 }, { pid::preDelay, 0 }, { pid::size, 92 }, { pid::space, 40 },
            { pid::decay, 100 }, { pid::diffusion, 90 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 30 }, { pid::lowCut, 45 }, { pid::highCut, 11000 },
            { pid::shimmer, 40 }, { pid::shimPitch, 12 },
            { pid::detune, 30 }, { pid::modRate, 0.12f }, { pid::modDepth, 34 },
            { pid::collapse, 26 }, { pid::mass, 30 }, { pid::width, 160 }, { pid::output, -4 },
            { pid::freeze, 1 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 760 }, { pid::delayFeed, 70 },
            { pid::delaySpread, 50 }, { pid::delayShimmer, 35 }, { pid::delayPitch, 12 },
            { pid::delayTone, 45 }, { pid::delayWobble, 30 }, { pid::delayMorph, 35 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 35 }, { pid::delayMix, 40 } } },

        { "Gravity Well", {
            { pid::mix, 64 }, { pid::preDelay, 80 }, { pid::size, 84 }, { pid::space, 30 },
            { pid::decay, 86 }, { pid::diffusion, 78 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 62 }, { pid::lowCut, 25 }, { pid::highCut, 4200 },
            { pid::shimmer, 10 }, { pid::shimPitch, -12 },
            { pid::detune, 34 }, { pid::modRate, 0.08f }, { pid::modDepth, 40 },
            { pid::collapse, 44 }, { pid::mass, 62 }, { pid::width, 90 }, { pid::output, -5 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 900 }, { pid::delayFeed, 62 },
            { pid::delaySpread, 45 }, { pid::delayShimmer, 12 }, { pid::delayPitch, -12 },
            { pid::delayTone, 65 }, { pid::delayWobble, 40 }, { pid::delayMorph, 25 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 55 }, { pid::delayMix, 38 } } },

        { "Black Hole Roar", {
            { pid::mix, 92 }, { pid::preDelay, 0 }, { pid::size, 96 }, { pid::space, 20 },
            { pid::decay, 97 }, { pid::diffusion, 94 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 74 }, { pid::lowCut, 20 }, { pid::highCut, 2600 },
            { pid::shimmer, 26 }, { pid::shimPitch, -12 },
            { pid::detune, 68 }, { pid::modRate, 0.05f }, { pid::modDepth, 66 },
            { pid::collapse, 88 }, { pid::mass, 92 }, { pid::width, 70 }, { pid::output, -8 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 240 }, { pid::delayFeed, 78 },
            { pid::delaySpread, 30 }, { pid::delayShimmer, 15 }, { pid::delayPitch, -12 },
            { pid::delayTone, 78 }, { pid::delayWobble, 55 }, { pid::delayMorph, 40 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 88 }, { pid::delayMix, 46 } } },
        // The one meant to be left running. Feedback past unity, so the network
        // regenerates instead of decaying, and the governor holds the level while new
        // material keeps landing on top of what is already circulating.
        { "Heat Death", {
            { pid::mix, 70 }, { pid::preDelay, 120 }, { pid::size, 90 }, { pid::space, 50 },
            { pid::decay, 92 }, { pid::diffusion, 88 }, { pid::reverbLevel, 100 },
            { pid::feedback, 88 },
            { pid::damping, 44 }, { pid::lowCut, 55 }, { pid::highCut, 8500 },
            { pid::shimmer, 30 }, { pid::shimPitch, 12 },
            { pid::detune, 26 }, { pid::modRate, 0.06f }, { pid::modDepth, 48 },
            { pid::collapse, 14 }, { pid::mass, 18 }, { pid::width, 155 }, { pid::output, -4 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 1200 }, { pid::delayFeed, 90 },
            { pid::delaySpread, 60 }, { pid::delayShimmer, 30 }, { pid::delayPitch, 12 },
            { pid::delayTone, 40 }, { pid::delayWobble, 40 }, { pid::delayMorph, 30 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 20 }, { pid::delayMix, 40 } } },
        // One note and walk away. Long lines, low diffusion so the repeats stay
        // separable rather than smearing into a wash, and feedback at maximum.
        { "Light Echo", {
            { pid::mix, 68 }, { pid::preDelay, 0 }, { pid::size, 92 }, { pid::space, 45 },
            { pid::decay, 100 }, { pid::diffusion, 26 }, { pid::reverbLevel, 100 },
            { pid::feedback, 100 },
            { pid::damping, 32 }, { pid::lowCut, 60 }, { pid::highCut, 9500 },
            { pid::shimmer, 22 }, { pid::shimPitch, 12 },
            { pid::detune, 18 }, { pid::modRate, 0.05f }, { pid::modDepth, 34 },
            { pid::collapse, 10 }, { pid::mass, 12 }, { pid::width, 150 }, { pid::output, -5 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 0 }, { pid::delayTime, 1500 }, { pid::delayFeed, 96 },
            { pid::delaySpread, 85 }, { pid::delayShimmer, 25 }, { pid::delayPitch, 12 },
            { pid::delayTone, 35 }, { pid::delayWobble, 30 }, { pid::delayMorph, 20 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 10 }, { pid::delayMix, 45 } } },
        // ---- built around the delay ----------------------------------------
        // The gentle end of it: repeats that arrive an octave up, drift, and are gone
        // before they ever crowd the source.
        { "Slow Light", {
            { pid::mix, 34 }, { pid::preDelay, 40 }, { pid::size, 58 }, { pid::space, 55 },
            { pid::decay, 62 }, { pid::diffusion, 76 }, { pid::reverbLevel, 100 },
            { pid::feedback, 0 },
            { pid::damping, 48 }, { pid::lowCut, 80 }, { pid::highCut, 11000 },
            { pid::shimmer, 18 }, { pid::shimPitch, 12 },
            { pid::detune, 10 }, { pid::modRate, 0.18f }, { pid::modDepth, 24 },
            { pid::collapse, 4 }, { pid::mass, 0 }, { pid::width, 140 }, { pid::output, -1 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 1 }, { pid::delayTime, 620 }, { pid::delayFeed, 40 },
            { pid::delaySpread, 75 }, { pid::delayShimmer, 34 }, { pid::delayPitch, 12 },
            { pid::delayTone, 38 }, { pid::delayWobble, 28 }, { pid::delayMorph, 35 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 0 }, { pid::delayMix, 36 } } },
        // Feedback past unity in the delay as well as a long reverb: the repeats never
        // stop, and Morph makes sure no two of them are the same interval.
        { "Redshift", {
            { pid::mix, 60 }, { pid::preDelay, 20 }, { pid::size, 78 }, { pid::space, 60 },
            { pid::decay, 88 }, { pid::diffusion, 80 }, { pid::reverbLevel, 100 },
            { pid::feedback, 40 },
            { pid::damping, 40 }, { pid::lowCut, 60 }, { pid::highCut, 9500 },
            { pid::shimmer, 26 }, { pid::shimPitch, 12 },
            { pid::detune, 24 }, { pid::modRate, 0.1f }, { pid::modDepth, 40 },
            { pid::collapse, 12 }, { pid::mass, 14 }, { pid::width, 160 }, { pid::output, -4 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 1 }, { pid::delayTime, 950 }, { pid::delayFeed, 94 },
            { pid::delaySpread, 60 }, { pid::delayShimmer, 46 }, { pid::delayPitch, 12 },
            { pid::delayTone, 52 }, { pid::delayWobble, 45 }, { pid::delayMorph, 55 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 26 }, { pid::delayMix, 48 } } },
        // Eighteen milliseconds, which is not an echo at all: down there the line is a
        // resonator and Feedback is its Q. Morph keeps sliding what it resonates at, so
        // instead of one metallic ring the pitch keeps moving off somewhere else.
        { "Photon Sphere", {
            { pid::mix, 55 }, { pid::preDelay, 0 }, { pid::size, 70 }, { pid::space, 65 },
            { pid::decay, 70 }, { pid::diffusion, 70 }, { pid::reverbLevel, 85 },
            { pid::feedback, 0 },
            { pid::damping, 40 }, { pid::lowCut, 70 }, { pid::highCut, 12000 },
            { pid::shimmer, 20 }, { pid::shimPitch, 12 },
            { pid::detune, 20 }, { pid::modRate, 0.2f }, { pid::modDepth, 30 },
            { pid::collapse, 6 }, { pid::mass, 6 }, { pid::width, 150 }, { pid::output, -3 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 1 }, { pid::delayTime, 18 }, { pid::delayFeed, 92 },
            { pid::delaySpread, 45 }, { pid::delayShimmer, 45 }, { pid::delayPitch, 12 },
            { pid::delayTone, 30 }, { pid::delayWobble, 25 }, { pid::delayMorph, 75 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 0 }, { pid::delayMix, 45 } } },
        // One strike and it bounces. Seven hundred milliseconds to the first repeat and
        // five per cent off the gap every time round, so it starts as separate hits and
        // runs down into a rattle over about fifteen seconds - and the feedback is high
        // enough to still be there when it gets there. Diffusion is low and the reverb
        // pulled back on purpose: this only reads as bouncing if the repeats stay
        // separate instead of smearing into the wash.
        { "Free Fall", {
            { pid::mix, 55 }, { pid::preDelay, 0 }, { pid::size, 60 }, { pid::space, 55 },
            { pid::decay, 55 }, { pid::diffusion, 45 }, { pid::reverbLevel, 70 },
            { pid::feedback, 0 },
            { pid::damping, 45 }, { pid::lowCut, 60 }, { pid::highCut, 12000 },
            { pid::shimmer, 10 }, { pid::shimPitch, 12 },
            { pid::detune, 10 }, { pid::modRate, 0.15f }, { pid::modDepth, 20 },
            { pid::collapse, 0 }, { pid::mass, 0 }, { pid::width, 145 }, { pid::output, -3 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 1 }, { pid::delayTime, 700 }, { pid::delayFeed, 90 },
            { pid::delaySpread, 55 }, { pid::delayShimmer, 0 }, { pid::delayPitch, 12 },
            { pid::delayTone, 35 }, { pid::delayWobble, 15 }, { pid::delayMorph, 0 },
            { pid::delayBounce, 45 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 0 }, { pid::delayMix, 55 } } },
        // The other end entirely. Abyss almost at maximum, so every repeat lands lower,
        // darker and harder driven than the one before it, into a reverb that is doing
        // the same thing to the wash underneath.
        { "Singularity", {
            { pid::mix, 82 }, { pid::preDelay, 0 }, { pid::size, 94 }, { pid::space, 25 },
            { pid::decay, 96 }, { pid::diffusion, 88 }, { pid::reverbLevel, 100 },
            { pid::feedback, 60 },
            { pid::damping, 70 }, { pid::lowCut, 25 }, { pid::highCut, 3200 },
            { pid::shimmer, 20 }, { pid::shimPitch, -12 },
            { pid::detune, 60 }, { pid::modRate, 0.05f }, { pid::modDepth, 60 },
            { pid::collapse, 80 }, { pid::mass, 86 }, { pid::width, 80 }, { pid::output, -9 },
            { pid::freeze, 0 }, { pid::mono, 0 },
            { pid::delayOn, 1 }, { pid::delayTime, 260 }, { pid::delayFeed, 97 },
            { pid::delaySpread, 35 }, { pid::delayShimmer, 16 }, { pid::delayPitch, -12 },
            { pid::delayTone, 74 }, { pid::delayWobble, 62 }, { pid::delayMorph, 70 },
            { pid::delayBounce, 0 }, { pid::delayWidth, 100 },
            { pid::delayAbyss, 94 }, { pid::delayMix, 62 } } },
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
