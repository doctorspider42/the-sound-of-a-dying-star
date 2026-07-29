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

/** The freeze latch. Big, obviously a switch, and lit from inside when it is holding -
    a control that stops time should not look like the other seventeen knobs. */
class FreezePill final : public juce::Button,
                         private juce::Timer
{
public:
    FreezePill (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID);
    ~FreezePill() override;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;
    void timerCallback() override;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    float lit = 0.0f;
    float phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezePill)
};

/** Latching switch for a whole section. The same idea as the freeze pill with none of
    the ceremony: a section that can be switched off has to say which it is at a glance,
    but it should not shout louder than the control that stops time. */
class EngagePill final : public juce::Button,
                         private juce::Timer
{
public:
    EngagePill (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                const juce::String& caption, juce::Colour accent);
    ~EngagePill() override;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;
    void timerCallback() override;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    juce::String caption;
    juce::Colour accent;
    float lit = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngagePill)
};

/** Horizontal readout of how much energy is still circulating in the network. With
    decay near the top, or frozen, this is the only way to see that the tail is still
    alive after the source has stopped. */
class TailMeter final : public juce::Component,
                        private juce::Timer
{
public:
    TailMeter();

    void setProvider (std::function<float()> fn);
    void refresh();
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;

    std::function<float()> provider;
    float level = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TailMeter)
};

/** Small triangular stepper, used either side of the preset name. */
class GlyphButton final : public juce::Button
{
public:
    explicit GlyphButton (bool pointsRight);

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    bool right;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlyphButton)
};

/** Preset name plus steppers. Clicking the name opens the full list. */
class PresetBar final : public juce::Component
{
public:
    PresetBar();

    std::function<void (int)> onSelect;          // index
    std::function<int()> currentIndex;
    std::function<juce::StringArray()> getNames;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

private:
    void step (int delta);

    GlyphButton prev { false }, next { true };
    juce::LookAndFeel_V4 menuLookAndFeel;
    juce::String displayName;
    bool hovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace dying
