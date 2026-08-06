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

#include "Parameters.h"
#include "dsp/NebulaEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace dying
{

class DyingStarProcessor final : public juce::AudioProcessor
{
public:
    DyingStarProcessor();
    ~DyingStarProcessor() override = default;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

    /** Visualiser feeds. Written on the audio thread, read on the message thread. */
    float getWetLevel()   const noexcept { return wetLevelOut.load (std::memory_order_relaxed); }
    float getBrightness() const noexcept { return brightnessOut.load (std::memory_order_relaxed); }

private:
    void pushParametersToEngine() noexcept;

    /** Asks the host what tempo it is running at, if it is a host and if it knows. */
    void updateHostTempo() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    ParamPointers params;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    dsp::NebulaEngine engine;
    juce::AudioBuffer<float> monoScratch;

    // What the host said last time it was asked. Only ever touched from the audio
    // thread, and from prepareToPlay, which cannot run at the same time as it.
    double hostBpm = tempo::kFallbackBpm;

    std::atomic<float> wetLevelOut { 0.0f };
    std::atomic<float> brightnessOut { 0.0f };

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DyingStarProcessor)
};

} // namespace dying
