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

/*  Fractional delay line with 4-point Hermite interpolation.

    Linear interpolation would be cheaper, but in a feedback network the interpolator
    runs once per repeat: its high-frequency loss compounds, and a tail that is meant
    to hang for a minute goes dull and then vanishes. Hermite costs a handful of
    multiplies and makes the "infinite" settings actually infinite.                */

#pragma once

#include "Utils.h"

#include <vector>

namespace dying::dsp
{

class DelayLine
{
public:
    /** Allocates the next power of two at or above maxDelaySamples + interpolation
        margin. Call from prepare only - this is the one allocating function here. */
    void prepare (int maxDelaySamples)
    {
        int size = 8;
        while (size < maxDelaySamples + 4)
            size <<= 1;

        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void write (float x) noexcept
    {
        buffer[(size_t) writeIndex] = x;
    }

    void advance() noexcept
    {
        writeIndex = (writeIndex + 1) & mask;
    }

    /** delaySamples is clamped into [1, size - 3] so a mis-scaled parameter degrades
        into a short delay rather than reading uninitialised memory. */
    float read (float delaySamples) const noexcept
    {
        const auto maxDelay = (float) (mask - 2);
        const auto d = clamp (delaySamples, 1.0f, maxDelay);

        const auto intPart  = (int) d;
        const auto frac     = d - (float) intPart;
        const auto base     = writeIndex - intPart;

        const auto y0 = buffer[(size_t) ((base + 1) & mask)];
        const auto y1 = buffer[(size_t) ( base      & mask)];
        const auto y2 = buffer[(size_t) ((base - 1) & mask)];
        const auto y3 = buffer[(size_t) ((base - 2) & mask)];

        // Catmull-Rom / Hermite, 4-point 3rd order
        const auto c0 = y1;
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    int getSize() const noexcept { return mask + 1; }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writeIndex = 0;
};

/** Schroeder all-pass with a modulatable fractional delay. Four of these in series
    turn a bare impulse into a smear dense enough to feed the network. */
class Allpass
{
public:
    void prepare (int maxDelaySamples)
    {
        line.prepare (maxDelaySamples);
    }

    void reset() noexcept { line.reset(); }

    void setDelay (float samples) noexcept { delaySamples = samples; }
    void setGain  (float g)       noexcept { gain = clamp (g, -0.85f, 0.85f); }

    float process (float x) noexcept
    {
        const auto delayed = line.read (delaySamples);
        const auto v = x - gain * delayed;
        line.write (killDenormal (v));
        line.advance();
        return gain * v + delayed;
    }

private:
    DelayLine line;
    float delaySamples = 1.0f;
    float gain = 0.5f;
};

} // namespace dying::dsp
