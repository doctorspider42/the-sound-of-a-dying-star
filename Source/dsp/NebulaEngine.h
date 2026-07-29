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

/*  The reverb engine: an 8-line feedback delay network with a Hadamard mixing matrix,
    a shimmer/sub pitch-shifted feedback path, and a soft clipper in the loop that
    doubles as the "collapse" distortion and as the guarantee that nothing can ever run
    away no matter what the user does with the decay control.

    No JUCE here, on purpose. The whole engine can be exercised from a plain console
    program, which is how every DSP claim in this project is checked.               */

#pragma once

#include "DelayLine.h"
#include "EarlyField.h"
#include "PitchShifter.h"
#include "Utils.h"
#include "VoidDelay.h"

#include <array>

namespace dying::dsp
{

inline constexpr int kNumLines = 8;

/** Everything the engine needs, in real units. The processor converts from parameter
    values once per block; the engine owns all the smoothing below this line. */
struct ReverbParams
{
    float mix          = 0.45f;   // 0..1
    float preDelayMs   = 40.0f;   // 0..500
    float size         = 0.6f;    // 0..1
    float space        = 0.0f;    // 0..1  early reflections: the depth of the room
    float reverbLevel  = 1.0f;    // 0..1  the network's own presence in the wet path
    float decay        = 0.65f;   // 0..1  (>= kInfiniteDecay means never)
    float feedback     = 0.0f;    // 0..1  regeneration on top of decay
    float damping      = 0.4f;    // 0..1
    float lowCutHz     = 60.0f;
    float highCutHz    = 12000.0f;
    float diffusion    = 0.7f;    // 0..1
    float shimmer      = 0.3f;    // 0..1
    float shimmerSemis = 12.0f;   // -24..24
    float detuneCents  = 12.0f;   // 0..100
    float modRateHz    = 0.35f;
    float modDepth     = 0.3f;    // 0..1
    float collapse     = 0.2f;    // 0..1
    float mass         = 0.0f;    // 0..1
    float width        = 1.2f;    // 0..2
    float outputGain   = 1.0f;    // linear
    bool  freeze       = false;
    bool  mono         = false;   // collapses everything leaving the plug-in

    /** The delay in front of the network. It lives inside the wet path, so the dry
        signal stays untouched and Mix still means what it says. */
    DelayParams delay;
};

/** Above this the decay control means literally infinite rather than "very long". */
inline constexpr float kInfiniteDecay = 0.995f;

class NebulaEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Called once per block from the audio thread. Cheap: no allocation, no locks. */
    void setParams (const ReverbParams& p) noexcept { params = p; }

    /** In-place stereo processing. */
    void process (float* left, float* right, int numSamples) noexcept;

    /** Smoothed wet RMS, for the visualiser. Read from the message thread. */
    float getWetLevel() const noexcept { return wetLevel; }

    /** Rough high-frequency content of the tail, 0..1 - drives the star's colour. */
    float getBrightness() const noexcept { return brightness; }

private:
    void updateTargets() noexcept;
    void updateControlRate() noexcept;

    double sr = 48000.0;

    ReverbParams params;

    // Plain C arrays rather than std::array: every index in the process loop is an int,
    // and std::array's size_type would make each one a signed-conversion warning.
    DelayLine lines[kNumLines];
    OnePoleLP damp[kNumLines];
    OnePoleHP cut[kNumLines];
    LFO       modLfo[kNumLines];
    LFO       driftLfo[kNumLines];
    float     feedback[kNumLines] {};
    float     baseDelay[kNumLines] {};   // samples, before size scaling

    Allpass diffuseL[4], diffuseR[4];
    DelayLine preDelayL, preDelayR;
    VoidDelay delay;
    EarlyField early;
    bool earlyActive = false;

    PitchShifter shimmerA, shimmerB, subShifter;
    OnePoleHP    shimmerHP;
    OnePoleLP    subLP;

    DCBlocker dcL, dcR;

    Smoothed smPreDelay, smSizeScale, smDecay, smMix, smWidth, smOutput, smSpace, smReverb,
             smMono;
    Smoothed smShimmer, smMass, smDrive, smModDepth, smDriftDepth, smInput, smDiffusion;
    Smoothed smFeedback;

    // Regeneration needs a governor. Loop gain above unity is what lets the network
    // sustain and keep accepting new material for hours instead of fading, but left
    // alone it also means energy only ever accumulates - into the clipper, and from
    // there into mush. These track how much is circulating and trim the regenerated
    // part of the feedback to hold it at a target.
    float loopEnergy = 0.0f;
    float regGain = 1.0f;
    float energyCoeff = 0.0f;
    float regCoeff = 0.0f;

    int   controlCounter = 0;
    float wetLevel = 0.0f;
    float brightness = 0.0f;
    float levelFollower = 0.0f;
    float brightFollower = 0.0f;
    OnePoleLP brightSplit;
};

} // namespace dying::dsp
