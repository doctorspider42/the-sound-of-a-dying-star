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

#include "Parameters.h"

#include "dsp/VoidDelay.h"   // the delay's own limits, so the range is stated once

namespace dying
{

namespace
{
    using Range = juce::NormalisableRange<float>;
    using Attributes = juce::AudioParameterFloatAttributes;

    /** Skewed so the useful half of the range occupies the useful half of the knob
        travel. Frequencies want a log law; times and percentages want the small
        values spread out. */
    Range logRange (float lo, float hi)
    {
        auto r = Range (lo, hi);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    Range skewedRange (float lo, float hi, float centre)
    {
        auto r = Range (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    /** An interval, written the way a musician writes one. Whole semitones carry no
        decimals at all, so a control that is on the grid says so at a glance and only a
        control that has been taken off it spends the space saying where. */
    juce::String semitoneText (float v, int)
    {
        const auto n = juce::roundToInt (v);
        const auto onTheGrid = std::abs (v - (float) n) < 0.005f;
        const auto text = onTheGrid ? juce::String (n) : juce::String (v, 2);

        return (v > 0.0f ? "+" : "") + text;
    }

    std::unique_ptr<juce::AudioParameterFloat> percent (const char* id, const juce::String& name,
                                                        float defaultValue, const char* suffix = "%")
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, Range (0.0f, 100.0f, 0.1f), defaultValue,
            Attributes().withLabel (suffix)
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); }));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (percent (pid::mix, "Mix", 45.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::preDelay, 1 }, "Pre-Delay",
        skewedRange (0.0f, 500.0f, 90.0f), 40.0f,
        Attributes().withLabel ("ms")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    layout.add (percent (pid::size, "Size", 60.0f));
    layout.add (percent (pid::decay, "Decay", 65.0f));

    // Defaults to zero: at 0 % the engine behaves exactly as it did before this
    // control existed, so no saved session changes character on upgrade.
    layout.add (percent (pid::feedback, "Feedback", 0.0f));
    layout.add (percent (pid::damping, "Damping", 40.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::lowCut, 1 }, "Low Cut",
        logRange (20.0f, 2000.0f), 60.0f,
        Attributes().withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int)
                                                  { return juce::String (juce::roundToInt (v)); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::highCut, 1 }, "High Cut",
        logRange (500.0f, 20000.0f), 12000.0f,
        Attributes().withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      return v >= 1000.0f
                                                           ? juce::String (v * 0.001f, 1) + "k"
                                                           : juce::String (juce::roundToInt (v));
                                                  })));

    layout.add (percent (pid::diffusion, "Diffusion", 70.0f));
    layout.add (percent (pid::shimmer, "Shimmer", 30.0f));

    // Continuous, with the semitone grid moved out into Free Pitch. The range and its
    // law are exactly what they were, so every stored value - preset, session or host
    // automation - still lands on the same interval it always did.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::shimPitch, 1 }, "Shimmer Pitch",
        Range (-24.0f, 24.0f, 0.01f), 12.0f,
        Attributes().withLabel ("st").withStringFromValueFunction (semitoneText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::detune, 1 }, "Detune",
        Range (0.0f, 100.0f, 0.1f), 12.0f,
        Attributes().withLabel ("ct")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::modRate, 1 }, "Mod Rate",
        logRange (0.01f, 6.0f), 0.35f,
        Attributes().withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));

    layout.add (percent (pid::modDepth, "Mod Depth", 30.0f));
    layout.add (percent (pid::collapse, "Collapse", 20.0f));
    layout.add (percent (pid::mass, "Mass", 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::width, 1 }, "Width",
        Range (0.0f, 200.0f, 0.1f), 120.0f,
        Attributes().withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::output, 1 }, "Output",
        skewedRange (-24.0f, 12.0f, -3.0f), 0.0f,
        Attributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::freeze, 1 }, "Freeze", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::bypass, 1 }, "Bypass", false));

    // ---- delay ---------------------------------------------------------------
    // Off by default, and the whole section is inert in that state, so a session saved
    // before it existed reloads sounding exactly as it did.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::delayOn, 1 }, "Delay", false));

    // Two milliseconds at the bottom, which is a comb filter rather than an echo, and
    // two seconds at the top. The centre sits low so the short half of the range - the
    // half where the repeats stop being countable - gets half the knob travel.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::delayTime, 1 }, "Delay Time",
        skewedRange (dsp::kMinDelayMs, dsp::kMaxDelayMs, 180.0f), 420.0f,
        Attributes().withLabel ("ms")
                    .withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      return v < 10.0f
                                                           ? juce::String (v, 1)
                                                           : juce::String (juce::roundToInt (v));
                                                  })));

    layout.add (percent (pid::delayFeed,    "Delay Feedback", 45.0f));
    layout.add (percent (pid::delaySpread,  "Delay Spread",   60.0f));
    layout.add (percent (pid::delayShimmer, "Delay Shimmer",  25.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::delayPitch, 1 }, "Delay Pitch",
        Range (-24.0f, 24.0f, 1.0f), 12.0f,
        Attributes().withLabel ("st")
                    .withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      const auto n = juce::roundToInt (v);
                                                      return (n > 0 ? "+" : "") + juce::String (n);
                                                  })));

    layout.add (percent (pid::delayTone,   "Delay Tone",   40.0f));
    layout.add (percent (pid::delayWobble, "Delay Wobble", 25.0f));
    layout.add (percent (pid::delayAbyss,  "Delay Abyss",   0.0f));
    layout.add (percent (pid::delayMix,    "Delay Mix",    35.0f));
    layout.add (percent (pid::delayMorph,  "Delay Morph",  20.0f));

    // The delay's own image, on top of whatever Spread has done with the repeats and
    // independent of the Width in Emission - so the echoes can be wider than the space
    // they are echoing into, or narrower than it.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::delayWidth, 1 }, "Delay Width",
        Range (0.0f, 200.0f, 0.1f), 100.0f,
        Attributes().withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));

    // Bipolar: to the right the repeats land sooner and sooner, to the left they spread
    // apart. Zero is a delay whose spacing does not move, which is what a delay is.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::delayBounce, 1 }, "Delay Bounce",
        Range (-100.0f, 100.0f, 0.1f), 0.0f,
        Attributes().withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    // ---- the early field ------------------------------------------------------
    // The one control here whose default is not its inert position. Zero is the reverb
    // as it was before this existed, and a session saved back then is migrated to it
    // explicitly - see DyingStarProcessor::setStateInformation - but a fresh instance
    // gets a room, because a reverb with no early reflections is the thing everybody
    // describes as lacking depth.
    layout.add (percent (pid::space, "Space", 40.0f));

    layout.add (percent (pid::reverbLevel, "Reverb", 100.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::mono, 1 }, "Mono", false));

    // ---- tempo sync -----------------------------------------------------------
    // Off by default, so the Time knob keeps meaning milliseconds until somebody asks
    // for bars, and a session saved before this existed reloads at the time it was set
    // to rather than at whatever the project tempo happens to be today.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::delaySync, 1 }, "Delay Sync", false));

    {
        juce::StringArray divisions;

        for (const auto& d : tempo::kDivisions)
            divisions.add (d.label);

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { pid::delayDiv, 1 }, "Delay Division",
            divisions, tempo::kDefaultDivision));
    }

    // Also off by default: the shimmer has always snapped to semitones, and a control
    // that quietly stopped doing that would change every patch built on it.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::shimFree, 1 }, "Free Pitch", false));

    return layout;
}

void ParamPointers::attach (juce::AudioProcessorValueTreeState& state)
{
    auto get = [&state] (const char* id)
    {
        auto* p = state.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    mix       = get (pid::mix);
    preDelay  = get (pid::preDelay);
    size      = get (pid::size);
    decay     = get (pid::decay);
    feedback  = get (pid::feedback);
    damping   = get (pid::damping);
    lowCut    = get (pid::lowCut);
    highCut   = get (pid::highCut);
    diffusion = get (pid::diffusion);
    shimmer   = get (pid::shimmer);
    shimPitch = get (pid::shimPitch);
    detune    = get (pid::detune);
    modRate   = get (pid::modRate);
    modDepth  = get (pid::modDepth);
    collapse  = get (pid::collapse);
    mass      = get (pid::mass);
    width     = get (pid::width);
    output    = get (pid::output);
    freeze    = get (pid::freeze);
    bypass    = get (pid::bypass);

    delayOn      = get (pid::delayOn);
    delayTime    = get (pid::delayTime);
    delayFeed    = get (pid::delayFeed);
    delaySpread  = get (pid::delaySpread);
    delayShimmer = get (pid::delayShimmer);
    delayPitch   = get (pid::delayPitch);
    delayTone    = get (pid::delayTone);
    delayWobble  = get (pid::delayWobble);
    delayAbyss   = get (pid::delayAbyss);
    delayMix     = get (pid::delayMix);
    delayMorph   = get (pid::delayMorph);
    delayBounce  = get (pid::delayBounce);
    delayWidth   = get (pid::delayWidth);
    mono         = get (pid::mono);

    space        = get (pid::space);
    reverbLevel  = get (pid::reverbLevel);

    delaySync    = get (pid::delaySync);
    delayDiv     = get (pid::delayDiv);
    shimFree     = get (pid::shimFree);
}

} // namespace dying
