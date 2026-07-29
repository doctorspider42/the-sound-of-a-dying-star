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

/*  Two-tap crossfading delay-line pitch shifter.

    Not a phase vocoder: this is the classic "rotating read head" shifter, and it is
    the right tool here precisely because it is imperfect. Its slight smear and the
    doubling around the crossfade are what a shimmer reverb wants, and it costs a few
    per cent of an FFT-based shifter while adding no latency at all.

    The read pointer drifts at (1 - ratio) samples per sample. A second tap sits half a
    window away and the two are crossfaded with a Hann pair, which sums to exactly one,
    so a steady tone comes out steady.                                              */

#pragma once

#include "DelayLine.h"
#include "Utils.h"

namespace dying::dsp
{

class PitchShifter
{
public:
    void prepare (double sr, float windowMs = 90.0f)
    {
        sampleRate = sr;
        maxWindow = (float) (windowMs * 0.001 * sr);
        line.prepare ((int) (maxWindow * 2.0f) + 64);
        smoothedRatio.prepare (sr, 60.0f, 1.0f);
        smoothedWindow.prepare (sr, 120.0f, maxWindow);
        phase = 0.0f;
    }

    void reset() noexcept
    {
        line.reset();
        phase = 0.0f;
        smoothedRatio.reset (smoothedRatio.getTarget());
        smoothedWindow.reset (smoothedWindow.getTarget());
    }

    /** Semitones plus a cents offset, so shimmer intervals and detune stay separate
        controls all the way down to here. */
    void setPitch (float semitones, float cents) noexcept
    {
        smoothedRatio.setTarget (semisToRatio (semitones) * centsToRatio (cents));
    }

    /** Shortens the window below what was allocated. The window is how far the read
        head travels before it has to jump, so it is also how much this smears: leave it
        at the prepared length for a reverb tail, and pull it in when the shifter sits
        inside a delay short enough that a 90 ms smear would blur four repeats into one.
        Smoothed, because the phase lives inside the window and a step change in it is a
        step change in where the read head is. */
    void setWindow (float windowSamples) noexcept
    {
        smoothedWindow.setTarget (clamp (windowSamples, 48.0f, maxWindow));
    }

    float getMaxWindow() const noexcept { return maxWindow; }

    float process (float x) noexcept
    {
        line.write (killDenormal (x));
        line.advance();

        const auto window = smoothedWindow.next();
        const auto ratio = smoothedRatio.next();

        phase += (1.0f - ratio);

        // Wrap without a divide: the drift per sample is at most a few samples, so a
        // couple of conditional adds always suffice.
        while (phase >= window) phase -= window;
        while (phase < 0.0f)    phase += window;

        const auto half = window * 0.5f;
        auto phase2 = phase + half;
        if (phase2 >= window) phase2 -= window;

        constexpr float margin = 4.0f;
        const auto a = line.read (phase  + margin);
        const auto b = line.read (phase2 + margin);

        // Each tap is faded out across its own wrap point, where its read position
        // jumps by a whole window. The Hann pair at p and p+0.5 sums to exactly 1.
        const auto ga = 0.5f - 0.5f * std::cos (kTwoPi * (phase  / window));
        const auto gb = 0.5f - 0.5f * std::cos (kTwoPi * (phase2 / window));

        return a * ga + b * gb;
    }

private:
    DelayLine line;
    Smoothed  smoothedRatio, smoothedWindow;
    double sampleRate = 48000.0;
    float  maxWindow = 4096.0f;
    float  phase = 0.0f;
};

} // namespace dying::dsp
