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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::shimPitch, 1 }, "Shimmer Pitch",
        Range (-24.0f, 24.0f, 1.0f), 12.0f,
        Attributes().withLabel ("st")
                    .withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      const auto n = juce::roundToInt (v);
                                                      return (n > 0 ? "+" : "") + juce::String (n);
                                                  })));

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
}

} // namespace dying
