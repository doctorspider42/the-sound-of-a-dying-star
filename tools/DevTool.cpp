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

/*  Offline developer tool.

        devtool check            - DSP assertions, measurements, CPU cost
        devtool shot <file.png>  - render the editor to a PNG
        devtool icon <file.png>  - render the application mark to a PNG

    None of the modes needs a host, an audio device or a display, so all of them run in
    CI and all of them run in about two seconds on a laptop. Every claim made about this
    reverb in the README is checked by something in here.                             */

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/gui/Icon.h"

#include <iostream>

using Processor = dying::DyingStarProcessor;

namespace
{

constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

void setParam (Processor& proc, const char* id, float value)
{
    if (auto* p = proc.getState().getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

struct Stats
{
    float rms = 0.0f;
    float peak = 0.0f;
    bool  finite = true;
};

Stats analyse (const juce::AudioBuffer<float>& buffer, int start = 0, int length = -1)
{
    Stats s;
    const auto n = length < 0 ? buffer.getNumSamples() - start : length;
    double sum = 0.0;
    int count = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* d = buffer.getReadPointer (ch);

        for (int i = start; i < start + n; ++i)
        {
            if (! std::isfinite (d[i]))
                s.finite = false;

            sum += (double) d[i] * d[i];
            s.peak = juce::jmax (s.peak, std::abs (d[i]));
            ++count;
        }
    }

    s.rms = count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    return s;
}

void fillSine (juce::AudioBuffer<float>& buffer, double freq, float amplitude,
               int start = 0, int length = -1)
{
    const auto n = length < 0 ? buffer.getNumSamples() - start : length;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);

        for (int i = 0; i < n; ++i)
            d[start + i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                             * freq * (double) i / kSampleRate);
    }
}

void fillNoise (juce::AudioBuffer<float>& buffer, float amplitude, int start, int length)
{
    juce::Random rng (0x5eed);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);

        for (int i = 0; i < length; ++i)
            d[start + i] = amplitude * (rng.nextFloat() * 2.0f - 1.0f);
    }
}

/** Amplitude of one frequency component by direct correlation. Exact when the window
    holds a whole number of periods, so no windowing is needed. */
float magnitudeAt (const juce::AudioBuffer<float>& buffer, double freq,
                   int startSample, int numSamples)
{
    double re = 0.0, im = 0.0;
    const auto* d = buffer.getReadPointer (0);

    for (int n = 0; n < numSamples; ++n)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * freq * (double) n / kSampleRate;
        re += (double) d[startSample + n] * std::cos (phase);
        im += (double) d[startSample + n] * std::sin (phase);
    }

    return (float) (2.0 * std::sqrt (re * re + im * im) / (double) numSamples);
}

/** When each repeat arrives, in milliseconds.

    A repeat is the loudest millisecond within half a gap either side of itself. Picking
    plain local maxima instead finds the texture inside a single burst and reports a
    repeat every 20 ms whatever the delay is actually doing - which it did, convincingly,
    until this was fed a click and the numbers came back identical for three settings
    that sound nothing alike. */
juce::Array<float> repeatTimesMs (const juce::AudioBuffer<float>& buffer, float minGapMs)
{
    constexpr float windowMs = 1.0f;
    const auto window = (int) (kSampleRate * windowMs * 0.001);

    std::vector<float> envelope;

    for (int start = 0; start + window <= buffer.getNumSamples(); start += window)
    {
        auto peak = 0.0f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = start; i < start + window; ++i)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        envelope.push_back (peak);
    }

    auto loudest = 0.0f;
    for (auto v : envelope)
        loudest = juce::jmax (loudest, v);

    juce::Array<float> times;
    const auto count = (int) envelope.size();
    const auto reach = juce::jmax (1, (int) (minGapMs * 0.5f / windowMs));

    for (int i = 0; i < count; ++i)
    {
        if (envelope[(size_t) i] <= loudest * 0.01f)
            continue;

        auto isPeak = true;

        // Strictly greater going back, greater or equal going forward, so a plateau is
        // reported once rather than at both ends of itself.
        for (int j = juce::jmax (0, i - reach); j < i && isPeak; ++j)
            isPeak = envelope[(size_t) j] < envelope[(size_t) i];

        for (int j = i + 1; j <= juce::jmin (count - 1, i + reach) && isPeak; ++j)
            isPeak = envelope[(size_t) j] <= envelope[(size_t) i];

        if (isPeak)
            times.add ((float) i * windowMs);
    }

    return times;
}

/** Where a stretch of audio sits between the speakers: +1 hard left, -1 hard right. */
float balanceOver (const juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    double sumL = 0.0, sumR = 0.0;

    for (int i = start; i < start + numSamples; ++i)
    {
        sumL += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        sumR += (double) buffer.getSample (1, i) * buffer.getSample (1, i);
    }

    const auto l = std::sqrt (sumL), r = std::sqrt (sumR);
    return (float) ((l - r) / (l + r + 1.0e-12));
}

void runThrough (Processor& proc, juce::AudioBuffer<float>& buffer, int blockSize = kBlockSize)
{
    juce::MidiBuffer midi;

    for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
    {
        const auto len = juce::jmin (blockSize, buffer.getNumSamples() - start);
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(), start, len);
        proc.processBlock (slice, midi);
    }
}

/** Puts the plug-in in a known clean state: fully wet, no colouring stages engaged. */
void neutral (Processor& proc)
{
    using namespace dying::pid;

    setParam (proc, mix, 100.0f);
    setParam (proc, preDelay, 0.0f);
    setParam (proc, size, 60.0f);
    setParam (proc, space, 0.0f);          // no early field unless a check asks for one
    setParam (proc, reverbLevel, 100.0f);
    setParam (proc, decay, 50.0f);
    setParam (proc, feedback, 0.0f);
    setParam (proc, damping, 30.0f);
    setParam (proc, lowCut, 20.0f);
    setParam (proc, highCut, 20000.0f);
    setParam (proc, diffusion, 70.0f);
    setParam (proc, shimmer, 0.0f);
    setParam (proc, shimPitch, 12.0f);
    setParam (proc, detune, 0.0f);
    setParam (proc, modRate, 0.3f);
    setParam (proc, modDepth, 0.0f);
    setParam (proc, collapse, 0.0f);
    setParam (proc, mass, 0.0f);
    setParam (proc, width, 100.0f);
    setParam (proc, output, 0.0f);
    setParam (proc, freeze, 0.0f);
    setParam (proc, bypass, 0.0f);

    // The delay is switched off, and its mix is left wide open: every check below that
    // wants to hear it only has to engage it.
    setParam (proc, delayOn, 0.0f);
    setParam (proc, delayTime, 420.0f);
    setParam (proc, delayFeed, 0.0f);
    setParam (proc, delaySpread, 0.0f);
    setParam (proc, delayShimmer, 0.0f);
    setParam (proc, delayPitch, 12.0f);
    setParam (proc, delayTone, 0.0f);
    setParam (proc, delayWobble, 0.0f);
    setParam (proc, delayAbyss, 0.0f);
    setParam (proc, delayMorph, 0.0f);
    setParam (proc, delayBounce, 0.0f);
    setParam (proc, delayWidth, 100.0f);
    setParam (proc, mono, 0.0f);
    setParam (proc, delayMix, 100.0f);
}

int runCheck()
{
    using namespace dying::pid;

    Processor proc;
    proc.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    // Several checks below compare floats with juce::exactlyEqual rather than a
    // tolerance, and mean it: "bit-identical", "both channels are the same channel" and
    // "the meter reads zero" are claims that a tolerance would not be making.
    int failures = 0;
    auto expect = [&failures] (bool condition, const juce::String& what)
    {
        std::cout << (condition ? "  ok    " : "  FAIL  ") << what << std::endl;
        if (! condition) ++failures;
    };

    std::cout << "offline check @ " << kSampleRate << " Hz, block " << kBlockSize << std::endl;
    std::cout << "reported latency: " << proc.getLatencySamples() << " samples" << std::endl;
    std::cout << "tail length: " << proc.getTailLengthSeconds() << " s\n" << std::endl;

    // ---- 1. transparent at 0 % mix -----------------------------------------
    // A reverb has no null setting for its own sound, but the dry path must survive
    // untouched - anything else shows up as a level change when users A/B the mix.
    {
        neutral (proc);
        setParam (proc, mix, 0.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, (int) kSampleRate);
        fillSine (buffer, 1000.0, 0.25f);
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);

        runThrough (proc, buffer);

        auto worst = 0.0f;
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            worst = juce::jmax (worst, std::abs (buffer.getSample (0, n)
                                                     - reference.getSample (0, n)));

        std::cout << "  dry-path error at 0 % mix: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, worst))
                  << " dBFS" << std::endl;
        expect (worst < 1.0e-4f, "dry path is untouched at 0 % mix");
    }

    // ---- 1b. the wet path is level-matched to the dry -----------------------
    // If it is not, the mix control doubles as a volume control and nobody can set it
    // by ear. Measured over the sustained part of a tone, once the tail has built up.
    {
        neutral (proc);
        setParam (proc, mix, 100.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.0));
        buffer.clear();
        fillSine (buffer, 1000.0, 0.25f, 0, (int) (kSampleRate * 2.0));

        const auto dry = analyse (buffer, (int) kSampleRate, (int) kSampleRate).rms;
        runThrough (proc, buffer);
        const auto wet = analyse (buffer, (int) kSampleRate, (int) kSampleRate).rms;

        const auto delta = juce::Decibels::gainToDecibels (wet / juce::jmax (1.0e-9f, dry));
        std::cout << "  wet vs dry at 100 % mix: " << juce::String (delta, 2) << " dB" << std::endl;
        expect (std::abs (delta) < 6.0f, "fully wet sits within 6 dB of the dry signal");
    }

    // ---- 2. parameters actually reach the DSP -------------------------------
    // The single highest-value assertion here: a smoothed parameter needs both halves,
    // the target read and the smoother advanced. Miss the first and every knob is
    // silently inert while the plug-in still looks like it works.
    {
        auto tailEnergyFor = [&] (float decayPercent)
        {
            neutral (proc);
            setParam (proc, decay, decayPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 4.0));
            buffer.clear();
            fillNoise (buffer, 0.3f, 0, (int) (kSampleRate * 0.25));
            runThrough (proc, buffer);

            // Last half second, well after the source has stopped.
            return analyse (buffer, (int) (kSampleRate * 3.5), (int) (kSampleRate * 0.5)).rms;
        };

        const auto shortTail = tailEnergyFor (25.0f);
        const auto longTail  = tailEnergyFor (85.0f);

        std::cout << "  tail after 3.5 s - decay 25 %: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, shortTail))
                  << " dBFS, decay 85 %: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, longTail))
                  << " dBFS" << std::endl;

        expect (longTail > shortTail * 8.0f, "decay control changes the decay time");
    }

    // ---- 3. bypass ----------------------------------------------------------
    {
        neutral (proc);
        setParam (proc, bypass, 1.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, (int) kSampleRate);
        fillSine (buffer, 700.0, 0.4f);
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);
        runThrough (proc, buffer);

        const auto latency = proc.getLatencySamples();
        auto worst = 0.0f;

        for (int n = 0; n < buffer.getNumSamples() - latency - 8; ++n)
            worst = juce::jmax (worst, std::abs (buffer.getSample (0, n + latency)
                                                     - reference.getSample (0, n)));

        expect (latency == 0, "reported latency is zero");
        expect (worst < 1.0e-5f, "bypass passes audio through, delayed by the reported latency");
        setParam (proc, bypass, 0.0f);
    }

    // ---- 4. silence in, silence out -----------------------------------------
    {
        neutral (proc);
        setParam (proc, decay, 90.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.0));
        buffer.clear();
        runThrough (proc, buffer);

        const auto after = analyse (buffer);
        expect (after.finite && after.peak < 1.0e-6f, "silence stays silent");
    }

    // ---- 5. everything at once stays bounded --------------------------------
    // Decay at maximum, the loop driven into the clipper, an octave-down feedback path
    // and a shimmer path both injecting energy. This is the configuration that would
    // blow up if the soft clipper were anywhere but inside the loop.
    {
        neutral (proc);
        setParam (proc, decay, 100.0f);
        setParam (proc, size, 100.0f);
        setParam (proc, shimmer, 100.0f);
        setParam (proc, mass, 100.0f);
        setParam (proc, collapse, 100.0f);
        setParam (proc, modDepth, 100.0f);
        setParam (proc, detune, 100.0f);
        setParam (proc, delayOn, 1.0f);
        setParam (proc, delayTime, 300.0f);
        setParam (proc, delayFeed, 100.0f);
        setParam (proc, delaySpread, 100.0f);
        setParam (proc, delayShimmer, 100.0f);
        setParam (proc, delayTone, 100.0f);
        setParam (proc, delayWobble, 100.0f);
        setParam (proc, delayAbyss, 100.0f);
        setParam (proc, delayMorph, 100.0f);
        setParam (proc, delayBounce, 100.0f);
        setParam (proc, space, 100.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 8.0));
        buffer.clear();
        fillNoise (buffer, 0.9f, 0, (int) (kSampleRate * 4.0));
        runThrough (proc, buffer);

        const auto after = analyse (buffer);
        std::cout << "  worst case peak: " << after.peak << std::endl;
        expect (after.finite, "output stays finite with every control at maximum");
        expect (after.peak < 2.0f, "output does not run away with every control at maximum");
    }

    // ---- 6. freeze holds ----------------------------------------------------
    {
        neutral (proc);
        setParam (proc, decay, 60.0f);
        proc.reset();

        juce::AudioBuffer<float> excite (2, (int) kSampleRate);
        fillNoise (excite, 0.4f, 0, excite.getNumSamples());
        runThrough (proc, excite);

        setParam (proc, freeze, 1.0f);

        juce::AudioBuffer<float> held (2, (int) (kSampleRate * 20.0));
        held.clear();
        runThrough (proc, held);

        const auto early = analyse (held, (int) kSampleRate, (int) (kSampleRate * 0.5)).rms;
        const auto late  = analyse (held, (int) (kSampleRate * 19.0), (int) (kSampleRate * 0.5)).rms;

        std::cout << "  frozen tail after 1 s: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, early))
                  << " dBFS, after 19 s: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, late))
                  << " dBFS" << std::endl;

        expect (late > early * 0.5f, "freeze holds the tail for at least 20 seconds");
        setParam (proc, freeze, 0.0f);
    }

    // ---- 7. shimmer really transposes ---------------------------------------
    // Feed one tone and look for the octave. Without this, a broken pitch shifter just
    // sounds like a slightly different reverb and nobody notices for months.
    {
        auto octaveEnergyFor = [&] (float shimmerPercent)
        {
            neutral (proc);
            setParam (proc, decay, 75.0f);
            setParam (proc, shimmer, shimmerPercent);
            setParam (proc, shimPitch, 12.0f);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 3.0));
            buffer.clear();
            fillSine (buffer, 500.0, 0.5f, 0, (int) kSampleRate);
            runThrough (proc, buffer);

            const auto start = (int) (kSampleRate * 1.5);
            const auto n = (int) kSampleRate;
            return magnitudeAt (buffer, 1000.0, start, n) / (magnitudeAt (buffer, 500.0, start, n)
                                                                 + 1.0e-9f);
        };

        const auto without = octaveEnergyFor (0.0f);
        const auto with    = octaveEnergyFor (90.0f);

        std::cout << "  octave-to-fundamental ratio - shimmer off: " << without
                  << ", shimmer 90 %: " << with << std::endl;

        expect (with > without * 4.0f, "shimmer puts energy an octave above the source");
    }

    // ---- 8. sample rate and block size changes ------------------------------
    {
        neutral (proc);
        setParam (proc, decay, 80.0f);
        setParam (proc, shimmer, 60.0f);

        // The delay allocates its lines from the sample rate, so the longest setting at
        // the highest rate is the one that would find a mis-sized buffer.
        setParam (proc, delayOn, 1.0f);
        setParam (proc, delayTime, 2000.0f);
        setParam (proc, delayFeed, 80.0f);
        setParam (proc, delayShimmer, 50.0f);
        setParam (proc, delayWobble, 100.0f);
        setParam (proc, delayAbyss, 60.0f);

        bool allFinite = true;

        for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (const auto block : { 16, 64, 480, 2048 })
            {
                proc.setPlayConfigDetails (2, 2, rate, block);
                proc.prepareToPlay (rate, block);

                juce::AudioBuffer<float> buffer (2, (int) (rate * 0.5));
                fillNoise (buffer, 0.5f, 0, buffer.getNumSamples());
                runThrough (proc, buffer, block);

                const auto s = analyse (buffer);
                allFinite = allFinite && s.finite && s.peak < 4.0f;
            }

        expect (allFinite, "stays finite across sample rates 44.1-192 kHz and blocks 16-2048");

        proc.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
        proc.prepareToPlay (kSampleRate, kBlockSize);
    }

    // ---- 9. mono ------------------------------------------------------------
    {
        neutral (proc);
        setParam (proc, delayOn, 1.0f);
        setParam (proc, delayFeed, 60.0f);
        setParam (proc, delaySpread, 100.0f);   // nothing to bounce between, in mono
        setParam (proc, delayShimmer, 40.0f);
        proc.setPlayConfigDetails (1, 1, kSampleRate, kBlockSize);
        proc.prepareToPlay (kSampleRate, kBlockSize);
        proc.reset();

        juce::AudioBuffer<float> buffer (1, (int) kSampleRate);
        fillNoise (buffer, 0.4f, 0, buffer.getNumSamples());
        runThrough (proc, buffer);

        const auto s = analyse (buffer);
        expect (s.finite && s.rms > 1.0e-4f, "mono instantiation produces finite audio");

        proc.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
        proc.prepareToPlay (kSampleRate, kBlockSize);
    }

    // ---- 10. every factory preset -------------------------------------------
    {
        bool allGood = true;

        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 3.0));
            buffer.clear();
            fillNoise (buffer, 0.5f, 0, (int) kSampleRate);
            runThrough (proc, buffer);

            const auto s = analyse (buffer);

            if (! s.finite || s.peak > 2.0f)
            {
                std::cout << "    preset '" << proc.getProgramName (i)
                          << "' peak " << s.peak << std::endl;
                allGood = false;
            }
        }

        expect (allGood, juce::String (proc.getNumPrograms())
                             + " factory presets all stay finite and bounded");
    }

    // ---- 11. state round-trips ----------------------------------------------
    // What a host does on every session save and reload. Getting this wrong loses a
    // user's settings, which is the one bug nobody forgives - and it is entirely
    // testable without a host.
    {
        neutral (proc);
        setParam (proc, decay, 77.0f);
        setParam (proc, shimmer, 41.0f);
        setParam (proc, shimPitch, -7.0f);
        setParam (proc, lowCut, 210.0f);
        setParam (proc, collapse, 63.0f);
        setParam (proc, freeze, 1.0f);

        juce::Array<float> expected;
        for (auto* p : proc.getParameters())
            expected.add (p->getValue());

        juce::MemoryBlock blob;
        proc.getStateInformation (blob);

        // Scramble everything, so a restore that silently does nothing cannot pass.
        for (auto* p : proc.getParameters())
            p->setValueNotifyingHost (1.0f - p->getValue());

        proc.setStateInformation (blob.getData(), (int) blob.getSize());

        auto worst = 0.0f;
        int index = 0;
        for (auto* p : proc.getParameters())
            worst = juce::jmax (worst, std::abs (p->getValue() - expected[index++]));

        std::cout << "  worst parameter drift across a save/reload: " << worst << std::endl;
        expect (blob.getSize() > 0, "state serialises to a non-empty block");
        expect (worst < 1.0e-6f, "every parameter survives a state round-trip");
    }

    // ---- 12. the editor can be opened and closed repeatedly ------------------
    // Hosts open and close editors constantly. This is where a dangling LookAndFeel or
    // an attachment outliving its parameter shows up - as a crash in someone's session.
    {
        neutral (proc);
        bool allPainted = true;

        for (int i = 0; i < 3; ++i)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

            if (editor == nullptr)
            {
                allPainted = false;
                break;
            }

            editor->setSize (dying::DyingStarEditor::kLogicalWidth,
                             dying::DyingStarEditor::kLogicalHeight);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(),
                                                                true, 1.0f);
            allPainted = allPainted && image.isValid();
        }

        expect (allPainted, "the editor constructs, paints and destroys three times over");
    }

    // ---- 13. automation while audio is running -------------------------------
    // Parameters moving every block is the normal case in a host, not the exception.
    // It is also what catches a smoother whose coefficient depends on a value it is
    // being handed mid-ramp.
    {
        neutral (proc);
        proc.reset();

        const char* swept[] = { size, space, reverbLevel, decay, shimmer, shimPitch,
                                collapse, mass, modRate, modDepth, detune, preDelay,
                                mix, width,
                                delayTime, delayFeed, delaySpread, delayShimmer,
                                delayTone, delayWobble, delayMorph, delayBounce,
                                delayWidth, delayAbyss, delayMix };

        setParam (proc, delayOn, 1.0f);

        juce::AudioBuffer<float> block (2, kBlockSize);
        juce::MidiBuffer midi;
        auto worstPeak = 0.0f;
        bool finite = true;

        for (int n = 0; n < 400; ++n)
        {
            const auto t = (float) n / 400.0f;

            // Each control sweeps on its own phase, so they are never all at the same
            // point of the same ramp at the same time.
            for (int k = 0; k < (int) (sizeof (swept) / sizeof (swept[0])); ++k)
                if (auto* p = proc.getState().getParameter (swept[k]))
                    p->setValueNotifyingHost (0.5f + 0.5f * std::sin (t * 37.0f + (float) k));

            fillNoise (block, 0.5f, 0, kBlockSize);
            proc.processBlock (block, midi);

            const auto s = analyse (block);
            finite = finite && s.finite;
            worstPeak = juce::jmax (worstPeak, s.peak);
        }

        std::cout << "  peak while sweeping "
                  << (int) (sizeof (swept) / sizeof (swept[0]))
                  << " parameters every block: " << worstPeak << std::endl;
        expect (finite && worstPeak < 2.0f,
                "stays finite and bounded with parameters sweeping every block");
    }

    // ---- 14. feedback sustains without running away --------------------------
    // The point of the control: excite it once, then leave it alone for five minutes
    // and come back to something still going, at roughly the level it was, neither
    // faded out nor piled into the clipper. Both failure modes are one-liners in the
    // engine and neither is audible in the first thirty seconds.
    {
        neutral (proc);
        setParam (proc, decay, 90.0f);
        setParam (proc, feedback, 95.0f);
        proc.reset();

        // Excited hard on purpose. A governor that overreacts to a loud passage is
        // exactly what makes a "sustain forever" control die in the first few seconds,
        // and it is invisible if the first measurement is taken a minute in - which is
        // how the earlier version of this test passed while the thing collapsed audibly.
        juce::AudioBuffer<float> excite (2, (int) (kSampleRate * 3.0));
        fillNoise (excite, 0.7f, 0, excite.getNumSamples());
        runThrough (proc, excite);

        juce::Array<float> perSecond;
        juce::AudioBuffer<float> second (2, (int) kSampleRate);
        auto finite = true;
        auto worstPeak = 0.0f;

        for (int t = 0; t < 300; ++t)
        {
            second.clear();
            runThrough (proc, second);
            const auto s = analyse (second);
            perSecond.add (s.rms);
            finite = finite && s.finite;
            worstPeak = juce::jmax (worstPeak, s.peak);
        }

        auto at = [&perSecond] (int sec) { return perSecond[juce::jlimit (0, 299, sec)]; };
        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        std::cout << "  regenerating level at 5/15/30/60/180/300 s: ";
        for (auto sec : { 5, 15, 30, 60, 180, 300 })
            std::cout << juce::String (db (at (sec - 1)), 1) << " ";
        std::cout << "dBFS (peak " << worstPeak << ")" << std::endl;

        auto loudest = 0.0f;
        for (int t = 4; t < 300; ++t)
            loudest = juce::jmax (loudest, perSecond[t]);

        expect (finite && worstPeak < 2.0f, "regenerating output stays finite and bounded");

        // The one that catches an over-eager governor: half a minute in, it must still
        // be near where it was five seconds in, not a fraction of it.
        expect (db (at (29)) - db (at (4)) > -10.0f,
                "does not collapse in the first half minute after a loud passage");

        expect (db (at (299)) > -30.0f, "still clearly audible after five minutes of silence");
        expect (db (loudest) > -20.0f, "sustains at a level that can carry a track");
    }

    // ---- 15. feedback at zero changes nothing --------------------------------
    // The control defaults to zero and every existing preset sets it there, so this is
    // what guarantees a saved session sounds the same after the upgrade that added it.
    {
        auto tailWith = [&] (float feedbackPercent)
        {
            neutral (proc);
            setParam (proc, decay, 60.0f);
            setParam (proc, feedback, feedbackPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 6.0));
            buffer.clear();
            fillNoise (buffer, 0.35f, 0, (int) (kSampleRate * 0.25));
            runThrough (proc, buffer);
            return analyse (buffer, (int) (kSampleRate * 5.5), (int) (kSampleRate * 0.5)).rms;
        };

        const auto off = tailWith (0.0f);
        const auto on  = tailWith (95.0f);

        std::cout << "  tail at 5.5 s - feedback 0 %: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, off))
                  << " dBFS, feedback 95 %: "
                  << juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, on)) << " dBFS"
                  << std::endl;

        expect (on > off * 4.0f, "feedback demonstrably outlasts the decay curve alone");
    }

    // ---- 16. one note, still going five minutes later ------------------------
    // The scenario the feature exists for: pluck a string once and walk away. Note the
    // shimmer setting - at 30 % the shimmer compensation used to multiply the
    // regenerated loop gain down to 0.945 and the whole thing died in seconds, and the
    // tests above never saw it because they all run with shimmer at zero.
    {
        neutral (proc);
        setParam (proc, size, 92.0f);
        setParam (proc, decay, 100.0f);
        setParam (proc, feedback, 100.0f);
        setParam (proc, damping, 44.0f);
        setParam (proc, highCut, 8500.0f);
        setParam (proc, lowCut, 55.0f);
        setParam (proc, shimmer, 30.0f);
        setParam (proc, diffusion, 30.0f);
        proc.reset();

        // 40 ms of plucked 220 Hz and nothing else. A tiny amount of energy compared to
        // the three seconds of noise the other test uses - the entire question here is
        // whether that is enough to get the network going.
        juce::AudioBuffer<float> ping (2, (int) (kSampleRate * 0.5));
        ping.clear();

        for (int n = 0; n < (int) (kSampleRate * 0.04); ++n)
        {
            const auto env = std::exp (-(float) n / (float) (kSampleRate * 0.012));
            const auto v = 0.5f * env * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                              * 220.0 * (double) n / kSampleRate);
            ping.setSample (0, n, v);
            ping.setSample (1, n, v);
        }

        runThrough (proc, ping);

        juce::Array<float> perSecond;
        juce::AudioBuffer<float> second (2, (int) kSampleRate);
        auto finite = true;

        for (int t = 0; t < 300; ++t)
        {
            second.clear();
            runThrough (proc, second);
            const auto s = analyse (second);
            perSecond.add (s.rms);
            finite = finite && s.finite;
        }

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        std::cout << "  single ping at 1/10/30/60/180/300 s: ";
        for (auto sec : { 1, 10, 30, 60, 180, 300 })
            std::cout << juce::String (db (perSecond[sec - 1]), 1) << " ";
        std::cout << "dBFS" << std::endl;

        expect (finite, "one ping stays finite for five minutes");
        expect (db (perSecond[299]) > -35.0f,
                "one ping is still clearly audible five minutes later");
        expect (db (perSecond[299]) - db (perSecond[9]) > -12.0f,
                "one ping does not quietly fade away over five minutes");
    }

    // ---- 17. the delay is inert while it is switched off ---------------------
    // Same contract the feedback control has: the section is off in every preset that
    // predates it and off by default, so a session saved before it existed has to
    // reload sounding identical - not close, identical.
    {
        auto render = [&] (bool wildSettings)
        {
            neutral (proc);
            setParam (proc, decay, 60.0f);
            setParam (proc, shimmer, 40.0f);

            if (wildSettings)
            {
                setParam (proc, delayFeed, 95.0f);
                setParam (proc, delaySpread, 100.0f);
                setParam (proc, delayShimmer, 100.0f);
                setParam (proc, delayWobble, 100.0f);
                setParam (proc, delayAbyss, 100.0f);
                setParam (proc, delayTime, 90.0f);
            }

            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.0));
            buffer.clear();
            fillNoise (buffer, 0.4f, 0, (int) (kSampleRate * 0.5));
            runThrough (proc, buffer);
            return buffer;
        };

        const auto quiet = render (false);
        const auto wild  = render (true);

        auto worst = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < quiet.getNumSamples(); ++n)
                worst = juce::jmax (worst, std::abs (quiet.getSample (ch, n)
                                                         - wild.getSample (ch, n)));

        expect (juce::exactlyEqual (worst, 0.0f),
                "the delay section is bit-identical to absent while off");
    }

    // ---- 18. the repeat lands where the time control says --------------------
    // A delay whose time is out by a factor of anything is useless for the one job it
    // has, and by ear a 300 ms repeat and a 340 ms repeat are indistinguishable until
    // you try to play in time with one.
    {
        auto repeatAtMs = [&] (float timeMs)
        {
            neutral (proc);
            setParam (proc, decay, 0.0f);          // keep the reverb tail out of the way
            setParam (proc, diffusion, 0.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, timeMs);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.5));
            buffer.clear();
            fillNoise (buffer, 0.5f, 0, (int) (kSampleRate * 0.02));
            runThrough (proc, buffer);

            const auto window = (int) (kSampleRate * 0.005);
            auto bestRms = 0.0f;
            auto bestStart = 0;

            for (int start = (int) (kSampleRate * 0.15);
                 start + window < buffer.getNumSamples(); start += window)
            {
                const auto r = analyse (buffer, start, window).rms;

                if (r > bestRms)
                {
                    bestRms = r;
                    bestStart = start;
                }
            }

            return 1000.0f * (float) bestStart / (float) kSampleRate;
        };

        const auto shortRepeat = repeatAtMs (300.0f);
        const auto longRepeat  = repeatAtMs (900.0f);

        std::cout << "  repeat measured at " << juce::String (shortRepeat, 1)
                  << " ms and " << juce::String (longRepeat, 1)
                  << " ms for 300 ms and 900 ms" << std::endl;

        expect (std::abs (shortRepeat - 300.0f) < 25.0f
                    && std::abs (longRepeat - 900.0f) < 40.0f,
                "the delay puts its repeat where the time control says");
    }

    // ---- 19. the delay's own shimmer transposes ------------------------------
    // The reverb's shimmer has its own check above; this is a separate pitch path in a
    // separate feedback loop and it can break on its own.
    {
        auto octaveRatioFor = [&] (float shimmerPercent)
        {
            neutral (proc);
            setParam (proc, decay, 0.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 250.0f);
            setParam (proc, delayFeed, 85.0f);
            setParam (proc, delayShimmer, shimmerPercent);
            setParam (proc, delayPitch, 12.0f);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 3.0));
            buffer.clear();
            fillSine (buffer, 500.0, 0.5f, 0, (int) kSampleRate);
            runThrough (proc, buffer);

            const auto start = (int) (kSampleRate * 1.5);
            const auto n = (int) kSampleRate;
            return magnitudeAt (buffer, 1000.0, start, n)
                       / (magnitudeAt (buffer, 500.0, start, n) + 1.0e-9f);
        };

        const auto without = octaveRatioFor (0.0f);
        const auto with    = octaveRatioFor (90.0f);

        std::cout << "  delay octave-to-fundamental ratio - shimmer off: " << without
                  << ", shimmer 90 %: " << with << std::endl;

        expect (with > without * 4.0f, "the delay's shimmer transposes its repeats");
    }

    // ---- 20. the delay sustains without running away -------------------------
    // Feedback at maximum is over unity here too, so this is the same question the
    // reverb's feedback answers: still going a minute later, and still bounded.
    {
        neutral (proc);
        setParam (proc, decay, 25.0f);
        setParam (proc, delayOn, 1.0f);
        setParam (proc, delayTime, 800.0f);
        setParam (proc, delayFeed, 100.0f);
        setParam (proc, delaySpread, 70.0f);
        setParam (proc, delayShimmer, 30.0f);
        setParam (proc, delayTone, 35.0f);
        setParam (proc, delayWobble, 30.0f);
        proc.reset();

        juce::AudioBuffer<float> excite (2, (int) (kSampleRate * 2.0));
        fillNoise (excite, 0.6f, 0, excite.getNumSamples());
        runThrough (proc, excite);

        juce::Array<float> perSecond;
        juce::AudioBuffer<float> second (2, (int) kSampleRate);
        auto finite = true;
        auto worstPeak = 0.0f;

        for (int t = 0; t < 120; ++t)
        {
            second.clear();
            runThrough (proc, second);
            const auto s = analyse (second);
            perSecond.add (s.rms);
            finite = finite && s.finite;
            worstPeak = juce::jmax (worstPeak, s.peak);
        }

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        std::cout << "  regenerating delay at 5/30/60/120 s: ";
        for (auto sec : { 5, 30, 60, 120 })
            std::cout << juce::String (db (perSecond[sec - 1]), 1) << " ";
        std::cout << "dBFS (peak " << worstPeak << ")" << std::endl;

        expect (finite && worstPeak < 2.0f, "the regenerating delay stays finite and bounded");
        expect (db (perSecond[119]) > -30.0f, "the delay is still audible two minutes later");
        expect (db (perSecond[119]) - db (perSecond[4]) > -12.0f,
                "the delay does not quietly fade away over two minutes");
    }

    // ---- 21. the abyss really drags the repeats downward ----------------------
    // The control that takes the section from delicate to a black hole. What it has to
    // do audibly is move every pass down in pitch; if it only distorted, half of what
    // the knob is for would be missing and nothing else here would notice.
    {
        auto belowRatioFor = [&] (float abyssPercent)
        {
            neutral (proc);
            setParam (proc, decay, 0.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 200.0f);
            setParam (proc, delayFeed, 88.0f);
            setParam (proc, delayAbyss, abyssPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 4.0));
            buffer.clear();
            fillSine (buffer, 800.0, 0.5f, 0, (int) (kSampleRate * 0.5));
            runThrough (proc, buffer);

            // Two seconds in, ten passes have happened. At full abyss the source should
            // have fallen most of two octaves; with it off nothing should have moved.
            const auto start = (int) (kSampleRate * 2.0);
            const auto n = (int) kSampleRate;
            return magnitudeAt (buffer, 300.0, start, n)
                       / (magnitudeAt (buffer, 800.0, start, n) + 1.0e-9f);
        };

        const auto without = belowRatioFor (0.0f);
        const auto with    = belowRatioFor (100.0f);

        std::cout << "  energy below the source after ten passes - abyss off: " << without
                  << ", abyss 100 %: " << with << std::endl;

        expect (with > without * 4.0f, "abyss drags the repeats below the source");
    }

    // ---- 22. shimmer does not quietly change the loop gain -------------------
    // The exact bug the reverb's shimmer compensation had, in the exact place it would
    // reappear: a pitch-shifted copy is decorrelated from its source, so blending it in
    // by amplitude loses power, and the delay dies at feedback settings that measure
    // perfectly with shimmer at zero. Below unity on purpose, so the governor is not
    // holding both cases at the same ceiling and hiding the difference.
    {
        auto tailAfter20s = [&] (float shimmerPercent)
        {
            neutral (proc);
            setParam (proc, decay, 0.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 500.0f);
            setParam (proc, delayFeed, 88.0f);
            setParam (proc, delayShimmer, shimmerPercent);
            proc.reset();

            juce::AudioBuffer<float> excite (2, (int) kSampleRate);
            fillNoise (excite, 0.4f, 0, excite.getNumSamples());
            runThrough (proc, excite);

            juce::AudioBuffer<float> silence (2, (int) (kSampleRate * 20.0));
            silence.clear();
            runThrough (proc, silence);

            return analyse (silence, (int) (kSampleRate * 19.0), (int) kSampleRate).rms;
        };

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        const auto flat  = tailAfter20s (0.0f);
        const auto risen = tailAfter20s (90.0f);

        std::cout << "  tail after 20 s at 88 % feedback - shimmer 0 %: "
                  << juce::String (db (flat), 1) << " dBFS, shimmer 90 %: "
                  << juce::String (db (risen), 1) << " dBFS" << std::endl;

        expect (std::abs (db (risen) - db (flat)) < 8.0f,
                "shimmer leaves the delay's loop gain where the feedback control put it");
    }

    // ---- 23. the early field is a room, not a level --------------------------
    // What Space has to do is put reflections in the first tenth of a second, where the
    // ear reads the size of a room - and not simply make the reverb louder, which is
    // what every "depth" control that is secretly a gain does.
    {
        struct Field { float early, late; };

        auto fieldFor = [&] (float spacePercent)
        {
            neutral (proc);
            setParam (proc, decay, 70.0f);
            setParam (proc, size, 70.0f);
            setParam (proc, space, spacePercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 3.0));
            buffer.clear();
            fillNoise (buffer, 0.5f, 0, (int) (kSampleRate * 0.005));
            runThrough (proc, buffer);

            Field f;
            f.early = analyse (buffer, (int) (kSampleRate * 0.010),
                               (int) (kSampleRate * 0.130)).rms;
            f.late  = analyse (buffer, (int) (kSampleRate * 1.5),
                               (int) (kSampleRate * 0.5)).rms;
            return f;
        };

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        const auto dry = fieldFor (0.0f);
        const auto room = fieldFor (80.0f);

        std::cout << "  first 140 ms: " << juce::String (db (dry.early), 1) << " -> "
                  << juce::String (db (room.early), 1) << " dBFS, tail at 1.5 s: "
                  << juce::String (db (dry.late), 1) << " -> "
                  << juce::String (db (room.late), 1) << " dBFS" << std::endl;

        expect (room.early > dry.early * 1.8f, "Space puts reflections in front of the tail");
        expect (std::abs (db (room.late) - db (dry.late)) < 6.0f,
                "Space adds a room rather than a level");
    }

    // ---- 24. the reverb's own dry/wet ----------------------------------------
    {
        auto wetFor = [&] (float reverbPercent)
        {
            neutral (proc);
            setParam (proc, decay, 60.0f);
            setParam (proc, space, 50.0f);
            setParam (proc, reverbLevel, reverbPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.0));
            buffer.clear();
            fillNoise (buffer, 0.4f, 0, (int) (kSampleRate * 0.5));
            runThrough (proc, buffer);
            return analyse (buffer, (int) kSampleRate, (int) kSampleRate).rms;
        };

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        const auto full = wetFor (100.0f);
        const auto half = wetFor (50.0f);
        const auto none = wetFor (0.0f);

        std::cout << "  reverb at 100/50/0 %: " << juce::String (db (full), 1) << " "
                  << juce::String (db (half), 1) << " " << juce::String (db (none), 1)
                  << " dBFS" << std::endl;

        expect (std::abs (db (half) - db (full) + 6.0f) < 1.5f,
                "the reverb control is a straight level over the network and its room");
        expect (none < 1.0e-6f, "at 0 % the reverb is gone and only the delay is left");
    }

    // ---- 25. a session saved before Space existed reloads without it ----------
    // The one control here whose default is not its inert position, so it is also the
    // one that needs a migration - and a migration nobody tests is a migration that
    // silently stops working the next time the state format is touched.
    {
        neutral (proc);
        setParam (proc, space, 70.0f);
        setParam (proc, decay, 66.0f);

        juce::MemoryBlock current;
        proc.getStateInformation (current);

        auto xml = juce::AudioProcessor::getXmlFromBinary (current.getData(),
                                                           (int) current.getSize());
        auto removed = false;

        if (xml != nullptr)
        {
            for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
                if (child->getStringAttribute ("id") == space)
                {
                    xml->removeChildElement (child, true);
                    removed = true;
                    break;
                }
        }

        expect (removed, "the state names its parameters one by one");

        if (removed)
        {
            juce::MemoryBlock older;
            juce::AudioProcessor::copyXmlToBinary (*xml, older);

            setParam (proc, space, 95.0f);   // so a restore that does nothing cannot pass
            proc.setStateInformation (older.getData(), (int) older.getSize());

            const auto restored = proc.getState().getRawParameterValue (space)->load();
            const auto decayKept = proc.getState().getRawParameterValue (decay)->load();

            std::cout << "  space after loading a state that predates it: " << restored
                      << " %" << std::endl;

            expect (juce::exactlyEqual (restored, 0.0f),
                    "a state without Space reloads with the early field off");
            expect (std::abs (decayKept - 66.0f) < 0.01f,
                    "and the rest of that state still arrives intact");
        }
    }

    // ---- 26. a very short delay still rings ----------------------------------
    // Twenty-five milliseconds is forty passes a second, so anything that costs a
    // fraction of a dB per pass costs tens of dB a second down there. Everything
    // cumulative in the loop is scaled by the spacing for exactly this reason, and this
    // is the check that says so: the same feedback setting has to hold a short delay up
    // as well as it holds a long one.
    {
        auto stillThereAfter = [&] (float timeMs, float seconds)
        {
            neutral (proc);
            setParam (proc, decay, 25.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, timeMs);
            setParam (proc, delayFeed, 95.0f);
            setParam (proc, delayTone, 50.0f);
            setParam (proc, delayShimmer, 30.0f);
            proc.reset();

            juce::AudioBuffer<float> excite (2, (int) (kSampleRate * 0.5));
            fillNoise (excite, 0.5f, 0, excite.getNumSamples());
            runThrough (proc, excite);

            juce::AudioBuffer<float> silence (2, (int) (kSampleRate * seconds));
            silence.clear();
            runThrough (proc, silence);

            return analyse (silence, (int) (kSampleRate * (seconds - 1.0)), (int) kSampleRate).rms;
        };

        auto db = [] (float v) { return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9f, v)); };

        const auto shortTime = stillThereAfter (25.0f, 10.0);
        const auto longTime  = stillThereAfter (600.0f, 10.0);

        std::cout << "  ringing ten seconds on - 25 ms: " << juce::String (db (shortTime), 1)
                  << " dBFS, 600 ms: " << juce::String (db (longTime), 1) << " dBFS" << std::endl;

        expect (db (shortTime) > -40.0f, "a 25 ms delay still rings ten seconds later");
        expect (db (shortTime) - db (longTime) > -20.0f,
                "the short setting holds up comparably to a long one");
    }

    // ---- 27. morph moves the repeats off the note they came in on -------------
    {
        auto offPitchFor = [&] (float morphPercent)
        {
            neutral (proc);
            setParam (proc, decay, 0.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 220.0f);
            setParam (proc, delayFeed, 88.0f);
            setParam (proc, delayShimmer, 70.0f);
            setParam (proc, delayPitch, 0.0f);      // the only thing moving is Morph
            setParam (proc, delayMorph, morphPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 6.0));
            buffer.clear();
            fillSine (buffer, 600.0, 0.5f, 0, (int) (kSampleRate * 0.5));
            runThrough (proc, buffer);

            // How much of what is left sits anywhere but on the note that went in.
            const auto start = (int) (kSampleRate * 2.0);
            const auto n = (int) (kSampleRate * 2.0);
            const auto onNote = magnitudeAt (buffer, 600.0, start, n);
            const auto elsewhere = magnitudeAt (buffer, 480.0, start, n)
                                     + magnitudeAt (buffer, 760.0, start, n)
                                     + magnitudeAt (buffer, 900.0, start, n);

            return elsewhere / (onNote + 1.0e-9f);
        };

        const auto still  = offPitchFor (0.0f);
        const auto moving = offPitchFor (85.0f);

        std::cout << "  energy away from the source note - morph off: " << still
                  << ", morph 85 %: " << moving << std::endl;

        expect (moving > still * 3.0f, "morph slides the repeats onto other pitches");
    }

    // ---- 28. the ball bounces -------------------------------------------------
    // One strike in, and the gaps between the repeats have to shrink - geometrically,
    // and without the pitch running away with them, which is the whole reason the
    // spacing is stepped and crossfaded rather than swept.
    {
        auto gapsFor = [&] (float bouncePercent)
        {
            neutral (proc);
            setParam (proc, reverbLevel, 0.0f);   // nothing in the wet path but the delay
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 400.0f);
            setParam (proc, delayBounce, bouncePercent);
            setParam (proc, delayFeed, 94.0f);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 6.0));
            buffer.clear();
            fillNoise (buffer, 0.7f, 0, (int) (kSampleRate * 0.001));   // one click
            runThrough (proc, buffer);

            const auto times = repeatTimesMs (buffer, 24.0f);
            juce::Array<float> gaps;

            for (int i = 1; i < times.size(); ++i)
                gaps.add (times[i] - times[i - 1]);

            return gaps;
        };

        const auto still = gapsFor (0.0f);
        const auto falling = gapsFor (70.0f);
        const auto rising = gapsFor (-70.0f);

        auto describe = [] (const juce::Array<float>& gaps)
        {
            juce::String s;
            for (int i = 0; i < juce::jmin (6, gaps.size()); ++i)
                s += juce::String (juce::roundToInt (gaps[i])) + " ";
            return s;
        };

        std::cout << "  gaps with bounce off:  " << describe (still) << "ms" << std::endl;
        std::cout << "  gaps bouncing down:    " << describe (falling) << "ms  ("
                  << falling.size() << " repeats in six seconds)" << std::endl;
        std::cout << "  gaps bouncing up:      " << describe (rising) << "ms" << std::endl;

        expect (still.size() >= 8 && std::abs (still[7] - still[0]) < 12.0f,
                "with bounce off the repeats are evenly spaced");

        expect (falling.size() > still.size(),
                "bouncing down fits more repeats into the same six seconds");

        expect (falling.size() >= 8 && falling[7] < falling[0] * 0.75f,
                "and every repeat lands sooner than the one before it");

        expect (rising.size() >= 4 && rising[3] > rising[0] * 1.15f,
                "negative bounce spreads the repeats apart instead");
    }

    // ---- 29. bouncing does not run the pitch away -----------------------------
    // A swept delay line is a pitch shift, and inside a feedback loop it compounds once
    // per pass - which is a siren, not a ball. The steps are crossfaded between two
    // fixed read heads precisely so this stays put.
    {
        neutral (proc);
        setParam (proc, reverbLevel, 0.0f);
        setParam (proc, delayOn, 1.0f);
        setParam (proc, delayTime, 400.0f);
        // Gently, so the measurement lands while the ball is still bouncing: wound
        // right up it reaches the floor in a couple of seconds and what is left is a
        // twenty-millisecond comb, whose spectrum is its own business and has nothing
        // to say about whether the steps moved the pitch.
        setParam (proc, delayBounce, 45.0f);
        setParam (proc, delayFeed, 94.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 6.0));
        buffer.clear();
        fillSine (buffer, 400.0, 0.6f, 0, (int) (kSampleRate * 0.25));
        runThrough (proc, buffer);

        // Four seconds and a dozen bounces later, the note has to still be the note. A
        // tenth of a semitone per pass would be four octaves by here.
        const auto start = (int) (kSampleRate * 4.0);
        const auto n = (int) kSampleRate;
        const auto onNote = magnitudeAt (buffer, 400.0, start, n);
        const auto aboveIt = magnitudeAt (buffer, 500.0, start, n)
                               + magnitudeAt (buffer, 630.0, start, n)
                               + magnitudeAt (buffer, 800.0, start, n);

        std::cout << "  after four seconds of bouncing - on the note: " << onNote
                  << ", above it: " << aboveIt << std::endl;

        expect (onNote > aboveIt, "the bouncing repeats stay on the note they came in on");
    }

    // ---- 30. the repeats really do go left, right, left ----------------------
    // Spread is the ping-pong, and it used to be one only at the very top of its
    // travel: measured with a mono click, the second repeat sat dead centre at 60 %.
    // The rotation is tapered now, so the control does something across its range.
    {
        auto balances = [&] (float spreadPercent)
        {
            neutral (proc);
            setParam (proc, reverbLevel, 0.0f);   // the repeats, and nothing else
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 300.0f);
            setParam (proc, delayFeed, 85.0f);
            setParam (proc, delaySpread, spreadPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.0));
            buffer.clear();
            fillNoise (buffer, 0.7f, 0, (int) (kSampleRate * 0.005));

            for (int n = 0; n < (int) (kSampleRate * 0.005); ++n)
                buffer.setSample (1, n, buffer.getSample (0, n));   // exactly mono in

            runThrough (proc, buffer);

            juce::Array<float> out;
            for (int r = 1; r <= 4; ++r)
                out.add (balanceOver (buffer, (int) (kSampleRate * 0.3 * r)
                                                  - (int) (kSampleRate * 0.01),
                                      (int) (kSampleRate * 0.08)));
            return out;
        };

        auto describe = [] (const juce::Array<float>& b)
        {
            juce::String s;
            for (auto v : b)
                s += juce::String (v, 2) + " ";
            return s;
        };

        const auto centred = balances (0.0f);
        const auto middling = balances (60.0f);
        const auto hard = balances (100.0f);

        std::cout << "  repeat balance, spread 0 %:   " << describe (centred) << std::endl;
        std::cout << "  repeat balance, spread 60 %:  " << describe (middling) << std::endl;
        std::cout << "  repeat balance, spread 100 %: " << describe (hard) << std::endl;

        expect (std::abs (centred[0]) < 0.05f && std::abs (centred[3]) < 0.05f,
                "with spread at zero a mono source stays in the middle");

        expect (hard[0] > 0.9f && hard[1] < -0.9f && hard[2] > 0.9f && hard[3] < -0.9f,
                "at 100 % the repeats alternate hard left and hard right");

        expect (middling[0] > 0.3f && middling[1] < -0.3f,
                "and at 60 % they are already clearly bouncing");
    }

    // ---- 31. the delay's own width -------------------------------------------
    {
        auto sideEnergyFor = [&] (float widthPercent)
        {
            neutral (proc);
            setParam (proc, reverbLevel, 0.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayTime, 250.0f);
            setParam (proc, delayFeed, 80.0f);
            setParam (proc, delaySpread, 100.0f);
            setParam (proc, delayWidth, widthPercent);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 2.0));
            buffer.clear();
            fillNoise (buffer, 0.6f, 0, (int) (kSampleRate * 0.01));
            runThrough (proc, buffer);

            double mid = 0.0, side = 0.0;
            for (int i = (int) (kSampleRate * 0.2); i < buffer.getNumSamples(); ++i)
            {
                const auto m = 0.5 * (buffer.getSample (0, i) + buffer.getSample (1, i));
                const auto s = 0.5 * (buffer.getSample (0, i) - buffer.getSample (1, i));
                mid += m * m;
                side += s * s;
            }

            return (float) (side / (mid + 1.0e-12));
        };

        const auto narrow = sideEnergyFor (0.0f);
        const auto normal = sideEnergyFor (100.0f);
        const auto wide   = sideEnergyFor (200.0f);

        std::cout << "  side-to-mid at delay width 0/100/200 %: " << narrow << " "
                  << normal << " " << wide << std::endl;

        expect (narrow < 1.0e-6f, "at 0 % the delay is mono, wherever Spread has put it");
        expect (wide > normal * 2.0f, "at 200 % it is wider than it was");
    }

    // ---- 32. mono sums exactly what left the plug-in --------------------------
    // Not a pan law, not a correlation trick: the two channels have to become the same
    // channel, and that channel has to be what the stereo output would have summed to.
    {
        auto render = [&] (bool summed)
        {
            neutral (proc);
            setParam (proc, decay, 60.0f);
            setParam (proc, space, 50.0f);
            setParam (proc, width, 160.0f);
            setParam (proc, delayOn, 1.0f);
            setParam (proc, delayFeed, 60.0f);
            setParam (proc, delaySpread, 100.0f);
            setParam (proc, mono, summed ? 1.0f : 0.0f);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 1.5));
            buffer.clear();
            fillNoise (buffer, 0.4f, 0, (int) (kSampleRate * 0.5));
            runThrough (proc, buffer);
            return buffer;
        };

        const auto stereo = render (false);
        const auto summed = render (true);

        auto worstSplit = 0.0f, worstSum = 0.0f;

        for (int n = 0; n < stereo.getNumSamples(); ++n)
        {
            worstSplit = juce::jmax (worstSplit, std::abs (summed.getSample (0, n)
                                                               - summed.getSample (1, n)));
            worstSum = juce::jmax (worstSum,
                                   std::abs (summed.getSample (0, n)
                                                 - 0.5f * (stereo.getSample (0, n)
                                                               + stereo.getSample (1, n))));
        }

        std::cout << "  mono: worst channel difference " << worstSplit
                  << ", worst error against the stereo sum " << worstSum << std::endl;

        expect (juce::exactlyEqual (worstSplit, 0.0f),
                "with Mono engaged both channels are identical");
        expect (worstSum < 1.0e-6f, "and they are the sum of what stereo would have given");
    }

    // ---- 33. bypass tells the meter -------------------------------------------
    // The panel keeps reading the last thing it saw otherwise, which is a tail meter
    // sitting half full while nothing at all is being processed.
    {
        neutral (proc);
        setParam (proc, decay, 80.0f);
        proc.reset();

        juce::AudioBuffer<float> excite (2, (int) (kSampleRate * 0.5));
        fillNoise (excite, 0.5f, 0, excite.getNumSamples());
        runThrough (proc, excite);

        const auto whileRunning = proc.getWetLevel();

        setParam (proc, bypass, 1.0f);
        juce::AudioBuffer<float> quiet (2, kBlockSize);
        quiet.clear();
        runThrough (proc, quiet);

        const auto whileBypassed = proc.getWetLevel();
        setParam (proc, bypass, 0.0f);

        std::cout << "  tail meter: " << whileRunning << " running, " << whileBypassed
                  << " bypassed" << std::endl;

        expect (whileRunning > 0.01f && juce::exactlyEqual (whileBypassed, 0.0f),
                "the meter reads zero while the plug-in is bypassed");
    }

    // ---- CPU cost ------------------------------------------------------------
    {
        constexpr double seconds = 60.0;

        auto costOf = [&] (bool withDelay)
        {
            neutral (proc);
            setParam (proc, shimmer, 50.0f);
            setParam (proc, mass, 30.0f);
            setParam (proc, space, 40.0f);   // what a fresh instance runs
            setParam (proc, delayOn, withDelay ? 1.0f : 0.0f);
            setParam (proc, delayFeed, 60.0f);
            setParam (proc, delayShimmer, 40.0f);
            setParam (proc, delayAbyss, 30.0f);
            proc.reset();

            juce::AudioBuffer<float> buffer (2, kBlockSize);
            fillSine (buffer, 500.0, 0.3f);

            juce::MidiBuffer midi;
            const auto start = juce::Time::getHighResolutionTicks();

            for (int done = 0; done < (int) (seconds * kSampleRate); done += kBlockSize)
                proc.processBlock (buffer, midi);

            return juce::Time::highResolutionTicksToSeconds (
                       juce::Time::getHighResolutionTicks() - start);
        };

        const auto bare = costOf (false);
        const auto full = costOf (true);

        std::cout << "\n" << seconds << " s of stereo processed in "
                  << juce::String (bare, 3) << " s  ("
                  << juce::String (bare / seconds * 100.0, 2)
                  << " % of one core at " << kSampleRate << " Hz), and in "
                  << juce::String (full, 3) << " s  ("
                  << juce::String (full / seconds * 100.0, 2)
                  << " %) with the delay engaged" << std::endl;
    }

    std::cout << (failures == 0 ? "\nall checks passed"
                                : "\n" + juce::String (failures) + " check(s) failed")
              << std::endl;

    return failures == 0 ? 0 : 1;
}

int writePng (const juce::Image& image, const juce::File& destination)
{
    destination.deleteFile();

    if (auto stream = destination.createOutputStream())
    {
        juce::PNGImageFormat png;

        if (png.writeImageToStream (image, *stream))
        {
            std::cout << "wrote " << destination.getFullPathName()
                      << " (" << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
            return 0;
        }
    }

    std::cerr << "could not write " << destination.getFullPathName() << std::endl;
    return 1;
}

/** Renders the editor with no window and no display server, so a panel that fails to
    construct or paint fails the build instead of shipping. */
int renderShot (const juce::File& destination, int presetIndex)
{
    Processor proc;
    proc.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    if (presetIndex >= 0)
        proc.setCurrentProgram (presetIndex);

    // Push audio through first, so the meter and the star have something to show:
    // component timers never fire without a message loop.
    {
        juce::AudioBuffer<float> buffer (2, (int) (kSampleRate * 1.5));
        buffer.clear();
        fillNoise (buffer, 0.35f, 0, (int) kSampleRate);
        runThrough (proc, buffer);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "no editor" << std::endl;
        return 1;
    }

    editor->setSize (dying::DyingStarEditor::kLogicalWidth,
                     dying::DyingStarEditor::kLogicalHeight);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    if (image.isNull())
    {
        std::cerr << "snapshot failed" << std::endl;
        return 1;
    }

    return writePng (image, destination);
}

/** Renders the application mark. The icon is drawn at the size it is asked for rather
    than scaled down from one master, so this is also the check that it still holds
    together at 32 pixels. */
int renderIcon (const juce::File& destination, int size)
{
    const auto image = dying::icon::render (size);

    if (image.isNull())
    {
        std::cerr << "icon render failed" << std::endl;
        return 1;
    }

    return writePng (image, destination);
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String command = argc > 1 ? juce::String (argv[1]) : "check";
    const auto cwd = juce::File::getCurrentWorkingDirectory();

    if (command == "check")
        return runCheck();

    if (command == "shot")
        return renderShot (cwd.getChildFile (argc > 2 ? juce::String (argv[2]) : "panel.png"),
                           argc > 3 ? juce::String (argv[3]).getIntValue() : -1);

    if (command == "icon")
        return renderIcon (cwd.getChildFile (argc > 2 ? juce::String (argv[2]) : "icon.png"),
                           argc > 3 ? juce::String (argv[3]).getIntValue() : 1024);

    std::cerr << "usage: devtool [check | shot <file.png> [presetIndex] "
              << "| icon <file.png> [size]]" << std::endl;
    return 2;
}
