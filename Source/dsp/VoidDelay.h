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

/*  The delay that feeds the reverb.

    A stereo pair of lines with an orthogonal cross-feed, so the repeats can rotate
    between the channels instead of sitting still, and a feedback path that is anything
    but a plain gain: each pass is darkened, pitch-shifted upward by the shimmer voice,
    dragged downward by the abyss voice, wobbled by two slow modulators and finally run
    through the same soft clipper the reverb uses.

    That combination is the point. A repeat never comes back the way it left, so the
    line can run at unity - or a little past it - for as long as you like without ever
    settling into a loop of the same sound. Wind Abyss up and the same path becomes a
    downward spiral: every repeat lands lower, darker and harder driven than the last.

    No JUCE here either: the whole thing is exercised from tools/DevTool.cpp.        */

#pragma once

#include "DelayLine.h"
#include "PitchShifter.h"
#include "Utils.h"

namespace dying::dsp
{

/** The longest repeat the lines can hold. Past this the reverb's own Size control is
    the better instrument - these exist for repeats you can still count. */
inline constexpr float kMaxDelayMs = 2000.0f;

/** And the shortest. Two milliseconds is well below anything you would call an echo:
    down there the line is a comb filter and the feedback control is its resonance. It
    is the same instrument - a repeat every 2 ms is still a repeat - and the whole
    section is built so that it keeps working when the repeats stop being countable. */
inline constexpr float kMinDelayMs = 2.0f;

/** In real units, like ReverbParams. The processor converts once per block. */
struct DelayParams
{
    bool  enabled    = false;
    float timeMs     = 420.0f;
    float feedback   = 0.45f;   // 0..1, past unity at the top
    float spread     = 0.6f;    // 0..1, stereo cross-feed
    float shimmer    = 0.25f;   // 0..1, how much of each repeat comes back transposed
    float pitchSemis = 12.0f;   // -24..24
    float tone       = 0.4f;    // 0..1, how fast the repeats go dark
    float wobble     = 0.25f;   // 0..1, tape-style movement on the read heads
    float morph      = 0.0f;    // 0..1, how far the pitch voices wander
    float bounce     = 0.0f;    // -1..1, how the spacing changes pass by pass
    float width      = 1.0f;    // 0..2, the repeats' own image
    float abyss      = 0.0f;    // 0..1, downward drag plus drive
    float mix        = 0.35f;   // 0..1, presence in the wet path
};

class VoidDelay
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Cheap: no allocation, no locks. Called once per block. */
    void setParams (const DelayParams& p) noexcept { params = p; }

    /** Filters, pitch voices and the feedback governor, all at the engine's control
        rate rather than per sample. */
    void updateControlRate() noexcept;

    /** Wet only, in the same units as the input - the caller decides where it goes. */
    void process (float inL, float inR, float& outL, float& outR) noexcept;

    /** Mean absolute level circulating in the lines, for the governor and the meter. */
    float getEnergy() const noexcept { return loopEnergy; }

private:
    double sr = 48000.0;

    DelayParams params;

    DelayLine    lines[2];
    OnePoleLP    toneLP[2];
    OnePoleHP    rumbleHP[2];
    PitchShifter riser[2];      // the shimmer voice
    PitchShifter sag[2];        // the abyss voice, always slightly flat
    LFO          wobbleLfo[2];
    LFO          driftLfo[2];
    LFO          morphLfo[4];   // one per pitch voice: two risers, two sags
    DCBlocker    dc[2];

    Smoothed smTime, smFeedback, smSpread, smShimmer, smAbyss, smWobble, smMix,
             smDrive, smEngage, smWidth;

    // Same shape of governor as the reverb's: a ceiling rather than a leveller, so
    // below it feedback means feedback and a decaying delay decays exactly as asked.
    float loopEnergy = 0.0f;
    float regGain = 1.0f;
    float energyCoeff = 0.0f;
    float regCoeff = 0.0f;

    // What a pitch voice actually hands back, measured while it runs rather than
    // assumed. It depends on the window, the interval and the material, and it is never
    // unity - so a fixed number for it is a loop gain that quietly changes with the
    // Shimmer control. Two slow followers per voice and the ratio between them is the
    // correction, which costs almost nothing and is right at every setting.
    float riseEnvIn[2] {}, riseEnvOut[2] {};
    float sagEnvIn[2] {},  sagEnvOut[2] {};
    float riseComp[2] { 1.0f, 1.0f };
    float sagComp[2]  { 1.0f, 1.0f };
    float voiceCoeff = 0.0f;

    // Set from the delay time: how much of a "per pass" loss, trim or drag is one
    // pass's worth at the current spacing.
    float perPass = 1.0f;

    /** Moves the read head one step closer to the write head (or further from it) and
        starts the crossfade that hides the jump. */
    void stepBounce (float newScale, float delaySamples) noexcept;

    // The bouncing ball. The spacing is stepped between repeats and crossfaded rather
    // than swept, because sweeping a recirculating delay is a pitch shift that compounds
    // once per pass - which is a siren, not a ball.
    float bounceScale = 1.0f;
    float bouncePrevScale = 1.0f;
    float bounceRatio = 1.0f;
    float bounceFloor = 0.05f;
    float bounceCeiling = 1.0f;
    int   bounceXfade = 0;
    int   bounceXfadeLen = 1;
    int   sinceBounce = 0;
    int   stepCountdown = 0;
    bool  bounceActive = false;

    // What restarts it: a new strike drops the ball again from the top.
    float onsetFast = 0.0f, onsetSlow = 0.0f;
    float onsetAttack = 0.0f, onsetFastRelease = 0.0f, onsetSlowRelease = 0.0f;
    int   onsetLockout = 0;

    // Switched off, the lines are flushed by writing silence into them rather than by
    // clearing a couple of megabytes on the audio thread. Once a whole line length of
    // silence has gone in there is nothing left to hear and processing stops.
    int lineLength = 1;
    int silentSamples = 0;
    bool dormant = true;
};

} // namespace dying::dsp
