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

/*  The early field: the first dozen reflections, before the tail.

    This is what was missing from the reverb and what people mean by depth. A feedback
    delay network builds a beautiful diffuse wash, but it arrives as a wash - it has no
    beginning. A real hall hands you a handful of discrete reflections off the near
    walls first, spread across the stereo field and each one a little duller than the
    last, and the ear reads the pattern of those as the size and the distance of the
    room. Take them away and every space sounds like the inside of a cloud.

    Nine taps per channel, no two the same, both sets mutually prime-ish - then two
    all-passes to take the edge off, so the taps read as reflections rather than as
    nine discrete copies of the source.                                             */

#pragma once

#include "DelayLine.h"
#include "Utils.h"

namespace dying::dsp
{

class EarlyField
{
public:
    static constexpr int kNumTaps = 9;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        const auto longestMs = kTapMsR[kNumTaps - 1] * kMaxScale;
        lineL.prepare ((int) (longestMs * 0.001f * (float) sr) + 64);
        lineR.prepare ((int) (longestMs * 0.001f * (float) sr) + 64);

        for (int k = 0; k < 2; ++k)
        {
            smoothL[k].prepare ((int) (18.0f * 0.001f * (float) sr) + 64);
            smoothR[k].prepare ((int) (18.0f * 0.001f * (float) sr) + 64);
            smoothL[k].setGain (0.55f);
            smoothR[k].setGain (0.55f);
            smoothL[k].setDelay (kSmoothMsL[k] * 0.001f * (float) sr);
            smoothR[k].setDelay (kSmoothMsR[k] * 0.001f * (float) sr);
        }

        // Distance eats the top end long before it eats the level. Without this the
        // early field is brighter than the source and sits in front of it instead of
        // behind it - which is the opposite of depth.
        airL.setCutoff (8500.0f, sr);
        airR.setCutoff (8500.0f, sr);

        smScale.prepare (sr, 120.0f, 1.0f);
        reset();
    }

    void reset() noexcept
    {
        lineL.reset();
        lineR.reset();

        for (int k = 0; k < 2; ++k)
        {
            smoothL[k].reset();
            smoothR[k].reset();
        }

        airL.reset();
        airR.reset();
        smScale.snap();
    }

    /** Bigger room, later reflections. Clamped well below the network's own size range:
        past a certain point a reflection is not early any more, it is an echo. */
    void setSize (float sizeScale) noexcept
    {
        smScale.setTarget (clamp (sizeScale, kMinScale, kMaxScale));
    }

    /** Keeps the line moving without reading any of it. Eighteen interpolated reads a
        sample is not a price worth paying while Space is at zero, but the history has
        to stay warm or turning it up starts with a hole in it. */
    void skip (float inL, float inR) noexcept
    {
        lineL.write (killDenormal (inL));
        lineR.write (killDenormal (inR));
        lineL.advance();
        lineR.advance();
        smScale.next();
    }

    /** Wet only. Roughly unit gain: the tap gains are normalised in power, so turning
        Space up adds a room rather than a level. */
    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        lineL.write (killDenormal (inL));
        lineR.write (killDenormal (inR));

        const auto scale = smScale.next();

        auto sumL = 0.0f, sumR = 0.0f;

        for (int k = 0; k < kNumTaps; ++k)
        {
            sumL += kGain[k] * kSignL[k] * lineL.read (kTapMsL[k] * 0.001f * (float) sr * scale);
            sumR += kGain[k] * kSignR[k] * lineR.read (kTapMsR[k] * 0.001f * (float) sr * scale);
        }

        lineL.advance();
        lineR.advance();

        sumL = airL.process (sumL);
        sumR = airR.process (sumR);

        for (int k = 0; k < 2; ++k)
        {
            sumL = smoothL[k].process (sumL);
            sumR = smoothR[k].process (sumR);
        }

        outL = sumL;
        outR = sumR;
    }

private:
    // Two sets of nine, sharing no ratios, so the two channels never agree on where a
    // wall is - which is where the width of the early field comes from.
    static constexpr float kTapMsL[kNumTaps] =
        { 11.3f, 19.7f, 28.1f, 39.7f, 51.3f, 66.1f, 81.7f, 99.3f, 121.7f };
    static constexpr float kTapMsR[kNumTaps] =
        { 14.9f, 23.3f, 33.7f, 44.9f, 57.1f, 71.9f, 88.3f, 106.1f, 129.3f };

    // exp(-0.30 k), normalised so the squares sum to one.
    static constexpr float kGain[kNumTaps] =
        { 0.673f, 0.499f, 0.370f, 0.274f, 0.203f, 0.150f, 0.111f, 0.083f, 0.061f };

    // Irregular polarity. Real reflections arrive either way up, and a set that all
    // agree sums into a single thump at the front of the response.
    static constexpr float kSignL[kNumTaps] = { 1, -1, 1, 1, -1, 1, -1, -1, 1 };
    static constexpr float kSignR[kNumTaps] = { 1, 1, -1, 1, -1, -1, 1, -1, -1 };

    static constexpr float kSmoothMsL[2] = {  6.7f, 11.3f };
    static constexpr float kSmoothMsR[2] = {  8.1f, 13.9f };

    static constexpr float kMinScale = 0.45f;
    static constexpr float kMaxScale = 2.4f;

    double sr = 48000.0;

    DelayLine lineL, lineR;
    Allpass   smoothL[2], smoothR[2];
    OnePoleLP airL, airR;
    Smoothed  smScale;
};

} // namespace dying::dsp
