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

    Neither mode needs a host, an audio device or a display, so both run in CI and both
    run in about two seconds on a laptop. Every claim made about this reverb in the
    README is checked by something in here.                                          */

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

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
    setParam (proc, decay, 50.0f);
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
}

int runCheck()
{
    using namespace dying::pid;

    Processor proc;
    proc.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
    proc.prepareToPlay (kSampleRate, kBlockSize);

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

    // ---- CPU cost ------------------------------------------------------------
    {
        constexpr double seconds = 60.0;
        neutral (proc);
        setParam (proc, shimmer, 50.0f);
        setParam (proc, mass, 30.0f);
        proc.reset();

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        fillSine (buffer, 500.0, 0.3f);

        juce::MidiBuffer midi;
        const auto start = juce::Time::getHighResolutionTicks();

        for (int done = 0; done < (int) (seconds * kSampleRate); done += kBlockSize)
            proc.processBlock (buffer, midi);

        const auto elapsed = juce::Time::highResolutionTicksToSeconds (
                                 juce::Time::getHighResolutionTicks() - start);

        std::cout << "\n" << seconds << " s of stereo processed in "
                  << juce::String (elapsed, 3) << " s  ("
                  << juce::String (elapsed / seconds * 100.0, 2)
                  << " % of one core at " << kSampleRate << " Hz)" << std::endl;
    }

    std::cout << (failures == 0 ? "\nall checks passed"
                                : "\n" + juce::String (failures) + " check(s) failed")
              << std::endl;

    return failures == 0 ? 0 : 1;
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

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String command = argc > 1 ? juce::String (argv[1]) : "check";

    if (command == "check")
        return runCheck();

    if (command == "shot")
        return renderShot (juce::File::getCurrentWorkingDirectory()
                               .getChildFile (argc > 2 ? juce::String (argv[2]) : "panel.png"),
                           argc > 3 ? juce::String (argv[3]).getIntValue() : -1);

    std::cerr << "usage: devtool [check | shot <file.png> [presetIndex]]" << std::endl;
    return 2;
}
