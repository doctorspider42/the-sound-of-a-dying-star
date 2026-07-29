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
    p.space        = params.space->load()     * 0.01f;
    p.reverbLevel  = params.reverbLevel->load() * 0.01f;
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
    p.mono         = params.mono->load() > 0.5f;

    p.delay.enabled    = params.delayOn->load() > 0.5f;
    p.delay.timeMs     = params.delayTime->load();
    p.delay.feedback   = params.delayFeed->load()    * 0.01f;
    p.delay.spread     = params.delaySpread->load()  * 0.01f;
    p.delay.shimmer    = params.delayShimmer->load() * 0.01f;
    p.delay.pitchSemis = params.delayPitch->load();
    p.delay.tone       = params.delayTone->load()    * 0.01f;
    p.delay.wobble     = params.delayWobble->load()  * 0.01f;
    p.delay.abyss      = params.delayAbyss->load()   * 0.01f;
    p.delay.mix        = params.delayMix->load()     * 0.01f;
    p.delay.morph      = params.delayMorph->load()   * 0.01f;
    p.delay.bounce     = params.delayBounce->load()  * 0.01f;
    p.delay.width      = params.delayWidth->load()   * 0.01f;

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
    {
        // Latency is zero, so the dry buffer is already the correct output. The meter
        // has to be told, though: left alone it holds whatever it was reading when the
        // bypass went on, and a tail meter frozen at half full while nothing is being
        // processed is a lie the panel tells for as long as you leave it there.
        wetLevelOut.store (0.0f, std::memory_order_relaxed);
        brightnessOut.store (0.0f, std::memory_order_relaxed);
        return;
    }

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

namespace
{
    /** APVTS writes one <PARAM id=... value=.../> per parameter, and leaves anything it
        does not find in the tree at whatever value it already had - which for a
        parameter that did not exist when the session was saved is whatever the last
        patch happened to leave behind. */
    bool stateMentions (const juce::XmlElement& xml, const char* id)
    {
        for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
            if (child->getStringAttribute ("id") == id)
                return true;

        return false;
    }
}

void DyingStarProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // Space defaults to 40 %, because a new instance should sound like a room. A
    // session saved before the early field existed has to sound like what it was saved
    // as, though, so it is migrated to the setting that reproduces exactly that.
    const auto knewAboutSpace = stateMentions (*xml, pid::space);

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (! knewAboutSpace)
        if (auto* p = apvts.getParameter (pid::space))
            p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
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
