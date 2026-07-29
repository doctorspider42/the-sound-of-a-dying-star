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

#include "PluginProcessor.h"
#include "gui/Backdrop.h"
#include "gui/Controls.h"
#include "gui/CosmicKnob.h"
#include "gui/StarView.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace dying
{

class DyingStarEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DyingStarEditor (DyingStarProcessor&);
    ~DyingStarEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The panel is designed once at this size and then scaled as a whole, so layout
        code never has to think about the zoom factor. */
    static constexpr int kLogicalWidth  = 1020;
    static constexpr int kLogicalHeight = 940;

private:
    /** Header, footer and anything else drawn straight onto the content component. */
    class Chrome final : public juce::Component
    {
    public:
        Chrome() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        juce::Rectangle<float> headerArea, footerArea;
    };

    std::unique_ptr<CosmicKnob> makeKnob (const juce::String& parameterID,
                                          const juce::String& caption,
                                          juce::Colour accent,
                                          bool bipolar = false);

    void layOutContent();
    void storeEditorSize();

    DyingStarProcessor& processor;

    juce::Component content;
    Backdrop backdrop;
    Chrome chrome;
    StarView star;
    TailMeter tailMeter;
    FreezePill freeze;
    EngagePill delayEngage;
    PresetBar presets;

    std::unique_ptr<CosmicKnob> kPreDelay, kSize, kSpace, kDecay, kDiffusion, kReverb;
    std::unique_ptr<CosmicKnob> kDamping, kLowCut, kHighCut, kWidth;
    std::unique_ptr<CosmicKnob> kShimmer, kPitch, kDetune, kModRate, kModDepth;
    std::unique_ptr<CosmicKnob> kCollapse, kMass;
    std::unique_ptr<CosmicKnob> kDelayTime, kDelayBounce, kDelayFeed, kDelaySpread,
                                kDelayShimmer, kDelayPitch, kDelayMorph, kDelayTone,
                                kDelayWobble, kDelayAbyss, kDelayMix;
    std::unique_ptr<CosmicKnob> kFeedback, kMix, kOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DyingStarEditor)
};

} // namespace dying
