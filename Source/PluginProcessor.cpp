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

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace dying
{

DyingStarProcessor::DyingStarProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DYINGSTAR", createParameterLayout())
{
    params.attach (apvts);
    bypassParam = apvts.getParameter (pid::bypass);
}

bool DyingStarProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void DyingStarProcessor::reset()
{
    // Push first: the engine snaps its smoothers on reset, and it can only snap to the
    // right values if it has been told what they are.
    pushParametersToEngine();
    engine.reset();
}

void DyingStarProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    pushParametersToEngine();
    engine.prepare (sampleRate, maximumExpectedSamplesPerBlock);
    monoScratch.setSize (2, juce::jmax (1, maximumExpectedSamplesPerBlock), false, false, true);
    monoScratch.clear();

    // No lookahead and no oversampling, so bypass is a straight pass-through and the
    // host has nothing to compensate for.
    setLatencySamples (0);
}

double DyingStarProcessor::getTailLengthSeconds() const
{
    // The decay curve tops out at 60 s, and freeze holds indefinitely - report the
    // long end so hosts do not truncate an offline render mid-tail.
    return 60.0;
}

void DyingStarProcessor::pushParametersToEngine() noexcept
{
    dsp::ReverbParams p;

    p.mix          = params.mix->load()       * 0.01f;
    p.preDelayMs   = params.preDelay->load();
    p.size         = params.size->load()      * 0.01f;
    p.decay        = params.decay->load()     * 0.01f;
    p.feedback     = params.feedback->load()  * 0.01f;
    p.damping      = params.damping->load()   * 0.01f;
    p.lowCutHz     = params.lowCut->load();
    p.highCutHz    = params.highCut->load();
    p.diffusion    = params.diffusion->load() * 0.01f;
    p.shimmer      = params.shimmer->load()   * 0.01f;
    p.shimmerSemis = params.shimPitch->load();
    p.detuneCents  = params.detune->load();
    p.modRateHz    = params.modRate->load();
    p.modDepth     = params.modDepth->load()  * 0.01f;
    p.collapse     = params.collapse->load()  * 0.01f;
    p.mass         = params.mass->load()      * 0.01f;
    p.width        = params.width->load()     * 0.01f;
    p.outputGain   = juce::Decibels::decibelsToGain (params.output->load());
    p.freeze       = params.freeze->load() > 0.5f;

    engine.setParams (p);
}

void DyingStarProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    if (params.bypass->load() > 0.5f)
        return;   // latency is zero, so the dry buffer is already the correct output

    pushParametersToEngine();

    if (buffer.getNumChannels() >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        // Mono in, mono out: still run the stereo network - its two output taps are
        // decorrelated, and summing them back gives a denser tail than halving the
        // network would.
        auto* src = buffer.getWritePointer (0);
        auto* l = monoScratch.getWritePointer (0);
        auto* r = monoScratch.getWritePointer (1);

        juce::FloatVectorOperations::copy (l, src, numSamples);
        juce::FloatVectorOperations::copy (r, src, numSamples);

        engine.process (l, r, numSamples);

        for (int n = 0; n < numSamples; ++n)
            src[n] = 0.5f * (l[n] + r[n]);
    }

    wetLevelOut.store (engine.getWetLevel(), std::memory_order_relaxed);
    brightnessOut.store (engine.getBrightness(), std::memory_order_relaxed);
}

int DyingStarProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

void DyingStarProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    applyPreset (apvts, index);
}

const juce::String DyingStarProcessor::getProgramName (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return {};

    return getFactoryPresets()[(size_t) index].name;
}

void DyingStarProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DyingStarProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DyingStarProcessor::createEditor()
{
    return new DyingStarEditor (*this);
}

} // namespace dying

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dying::DyingStarProcessor();
}
