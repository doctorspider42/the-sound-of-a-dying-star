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

#pragma once

#include "Skin.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace dying
{

/** Rotary drawn as a small planet: a recessed dial ring, a glowing value arc in the
    group's accent colour, and a pointer with a bloom around it. The juce::Slider
    underneath keeps all the behaviour hosts expect - drag, wheel, fine-drag with
    ctrl/cmd, double-click to default, automation. */
class CosmicKnob final : public juce::Component
{
public:
    CosmicKnob (juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                const juce::String& caption,
                juce::Colour accent,
                bool bipolar = false);

    ~CosmicKnob() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Diameter of the dial itself; the component adds room for caption and readout. */
    void setKnobDiameter (float d) { diameter = d; resized(); }

    static constexpr float captionHeight = 14.0f;
    static constexpr float readoutHeight = 16.0f;

private:
    class LookAndFeel;

    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::unique_ptr<LookAndFeel> lookAndFeel;

    juce::String caption, suffix;
    juce::Colour accent;
    float diameter = 64.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CosmicKnob)
};

} // namespace dying
