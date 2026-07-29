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

#include "NebulaEngine.h"

namespace dying::dsp
{

namespace
{
    /** Mutually prime-ish line lengths in milliseconds. Ratios matter more than the
        absolute values: shared factors put repeats on top of each other and the tail
        turns metallic. */
    constexpr float kBaseMs[kNumLines] =
        { 26.03f, 33.71f, 41.19f, 49.53f, 58.87f, 67.31f, 76.61f, 89.89f };

    constexpr float kDiffuseMsL[4] = {  4.77f,  6.31f,  9.13f, 13.71f };
    constexpr float kDiffuseMsR[4] = {  5.31f,  7.19f, 10.43f, 15.11f };

    /** In-place fast Walsh-Hadamard transform, normalised. Orthogonal, so the network
        is lossless before the feedback gains are applied - the decay time is then
        entirely determined by those gains and nothing else. */
    inline void hadamard8 (float* v) noexcept
    {
        for (int i = 0; i < 8; i += 2)
        {
            const auto a = v[i], b = v[i + 1];
            v[i] = a + b; v[i + 1] = a - b;
        }

        for (int i = 0; i < 8; i += 4)
            for (int j = 0; j < 2; ++j)
            {
                const auto a = v[i + j], b = v[i + j + 2];
                v[i + j] = a + b; v[i + j + 2] = a - b;
            }

        for (int j = 0; j < 4; ++j)
        {
            const auto a = v[j], b = v[j + 4];
            v[j] = a + b; v[j + 4] = a - b;
        }

        constexpr float norm = 0.35355339059f;   // 1 / sqrt(8)
        for (int i = 0; i < 8; ++i)
            v[i] *= norm;
    }

    constexpr float kInputTrim       = 0.32f;
    // Calibrated so that at 100 % mix and a mid decay the wet output sits at roughly
    // the level of the source. Anything else turns the mix control into a volume
    // control and nobody can set it by ear. `devtool check` measures this.
    constexpr float kOutputTrim      = 2.85f;
    constexpr float kMaxPreDelayMs   = 500.0f;
    constexpr float kMaxModMs        = 8.0f;
    constexpr float kMaxDriftMs      = 13.0f;

    // How much of the delay's output is also sent into the network. Less than what goes
    // straight to the output: the repeats should bloom into the space, not be swallowed
    // by it. Tied to the delay's own mix, so one control still means one thing.
    constexpr float kDelayToNetwork  = 0.75f;

    // The early field goes to the output at close to full level, and into the network
    // at half of it. Both matter: the first is what you hear as the room, the second is
    // what gives the tail something shaped to grow out of, so the wash arrives from
    // somewhere instead of simply fading up.
    constexpr float kEarlyLevel      = 0.95f;
    constexpr float kEarlyToNetwork  = 0.5f;

    // Loop gain at full regeneration. Comfortably over unity: it has to outrun the
    // interpolator and modulation losses that drag a nominally infinite tail downward
    // over minutes, and still have something left over to build with.
    constexpr float kRegenGain       = 1.052f;

    // The governor is NOT what keeps this safe - the soft clipper inside the loop is,
    // and it bounds every line unconditionally. The governor exists only so a
    // regenerating network does not end up parked inside that clipper forever, sounding
    // permanently saturated. So it is a ceiling, not a target: below it nothing happens
    // at all, and feedback means feedback.
    constexpr float kLoopCeiling     = 0.075f;

    // Gentle ratio and a high floor. A governor that can pull the loop gain a long way
    // down is a governor that can cut a tail short, which is the opposite of the point.
    // At the floor the loop still rings for many seconds.
    constexpr float kGovernorRatio   = 0.35f;
    constexpr float kGovernorFloor   = 0.90f;

    constexpr float kEnergyTimeSec   = 0.35f;   // how fast the governor sees a change
    constexpr float kRegTimeSec      = 3.0f;    // how fast it acts on one

    /** Size, mapped logarithmically from a tight room to lines long enough that the
        network stops being a reverb and becomes a multi-tap delay: 90 ms x 6 is well
        over half a second per line, so a single note comes back as audible, separable
        repeats rather than a wash. Wound up with Feedback and Diffusion low, that is
        the "one ping and it is still going when everyone has gone home" setting, and it
        needs no separate delay section - these are already delay lines. */
    inline float sizeToScale (float size) noexcept
    {
        return 0.15f * std::pow (40.0f, clamp (size, 0.0f, 1.0f));
    }

    constexpr float kMaxSizeScale = 6.0f;
}

void NebulaEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;

    // Longest line at maximum size, plus room for both modulators on top.
    const auto longestMs = kBaseMs[kNumLines - 1] * kMaxSizeScale + kMaxModMs + kMaxDriftMs;
    const auto maxLineSamples = (int) (longestMs * 0.001 * sr) + 64;

    Xorshift rng (0x5eed57a4u);

    for (int i = 0; i < kNumLines; ++i)
    {
        lines[i].prepare (maxLineSamples);
        baseDelay[i] = kBaseMs[i] * 0.001f * (float) sr;

        // Independent phases, or every line swings together and the modulation reads
        // as vibrato instead of as movement inside the space.
        modLfo[i].reset (rng.nextFloat());
        driftLfo[i].reset (rng.nextFloat());
        driftLfo[i].setRate (0.031f + 0.047f * (float) i, sr);
    }

    for (int k = 0; k < 4; ++k)
    {
        diffuseL[k].prepare ((int) (kDiffuseMsL[k] * 0.001f * sr * 2.0f) + 64);
        diffuseR[k].prepare ((int) (kDiffuseMsR[k] * 0.001f * sr * 2.0f) + 64);
    }

    preDelayL.prepare ((int) (kMaxPreDelayMs * 0.001f * sr) + 64);
    preDelayR.prepare ((int) (kMaxPreDelayMs * 0.001f * sr) + 64);

    delay.setParams (params.delay);
    delay.prepare (sr);
    early.prepare (sr);

    shimmerA.prepare (sr, 92.0f);
    shimmerB.prepare (sr, 78.0f);      // different windows, so the two voices do not
    subShifter.prepare (sr, 110.0f);   // flutter in lockstep

    shimmerHP.setCutoff (260.0f, sr);
    subLP.setCutoff (420.0f, sr);
    brightSplit.setCutoff (2200.0f, sr);

    constexpr float fast = 12.0f, slow = 60.0f;
    smPreDelay  .prepare (sr, 120.0f, params.preDelayMs * 0.001f * (float) sr);
    smSizeScale .prepare (sr, slow,   sizeToScale (params.size));
    smDecay     .prepare (sr, slow,   params.decay);
    smMix       .prepare (sr, fast,   params.mix);
    smWidth     .prepare (sr, fast,   params.width);
    smOutput    .prepare (sr, fast,   params.outputGain);
    smShimmer   .prepare (sr, slow,   params.shimmer);
    smMass      .prepare (sr, slow,   params.mass);
    smDrive     .prepare (sr, slow,   1.0f);
    smModDepth  .prepare (sr, slow,   0.0f);
    smDriftDepth.prepare (sr, slow,   0.0f);
    smInput     .prepare (sr, 40.0f,  kInputTrim);
    smDiffusion .prepare (sr, slow,   params.diffusion);
    smFeedback  .prepare (sr, slow,   params.feedback);
    smSpace     .prepare (sr, fast,   params.space);
    smReverb    .prepare (sr, fast,   params.reverbLevel);
    smMono      .prepare (sr, fast,   params.mono ? 1.0f : 0.0f);

    energyCoeff = 1.0f - std::exp (-1.0f / (kEnergyTimeSec * (float) sr));
    regCoeff    = 1.0f - std::exp (-(float) kControlInterval / (kRegTimeSec * (float) sr));

    reset();
}

void NebulaEngine::reset() noexcept
{
    for (int i = 0; i < kNumLines; ++i)
    {
        lines[i].reset();
        damp[i].reset();
        cut[i].reset();
    }

    for (int k = 0; k < 4; ++k)
    {
        diffuseL[k].reset();
        diffuseR[k].reset();
    }

    preDelayL.reset();
    preDelayR.reset();
    shimmerA.reset();
    shimmerB.reset();
    subShifter.reset();
    shimmerHP.reset();
    subLP.reset();
    brightSplit.reset();
    dcL.reset();
    dcR.reset();

    // Nothing is left to ramp away from, so every smoother starts where the parameters
    // already are. Otherwise the first few milliseconds after a reset run with whatever
    // the previous session's values happened to be.
    updateTargets();

    for (auto* s : { &smPreDelay, &smSizeScale, &smDecay, &smMix, &smWidth, &smOutput,
                     &smShimmer, &smMass, &smDrive, &smModDepth, &smDriftDepth, &smInput,
                     &smDiffusion, &smFeedback, &smSpace, &smReverb, &smMono })
        s->snap();

    // After updateTargets, which is what hands the delay its parameters - it snaps its
    // own smoothers to them.
    delay.reset();
    early.reset();
    earlyActive = params.space > 0.0f;

    controlCounter = 0;
    wetLevel = brightness = levelFollower = brightFollower = 0.0f;
    loopEnergy = 0.0f;
    regGain = 1.0f;
}

void NebulaEngine::updateTargets() noexcept
{
    const auto frozen = params.freeze;

    smPreDelay  .setTarget (clamp (params.preDelayMs, 0.0f, kMaxPreDelayMs) * 0.001f * (float) sr);
    smSizeScale .setTarget (sizeToScale (params.size));
    smDecay     .setTarget (params.decay);
    smMix       .setTarget (params.mix);
    smWidth     .setTarget (params.width);
    smOutput    .setTarget (params.outputGain);
    smShimmer   .setTarget (params.shimmer);
    smMass      .setTarget (params.mass);
    smDiffusion .setTarget (params.diffusion);
    smFeedback  .setTarget (params.feedback);
    smSpace     .setTarget (params.space);
    smReverb    .setTarget (params.reverbLevel);
    smMono      .setTarget (params.mono ? 1.0f : 0.0f);
    smInput     .setTarget (frozen ? 0.0f : kInputTrim);
    smDrive     .setTarget (1.0f + 11.0f * params.collapse);
    smModDepth  .setTarget (params.modDepth * kMaxModMs * 0.001f * (float) sr
                                * (frozen ? 0.5f : 1.0f));
    smDriftDepth.setTarget (params.detuneCents * 0.01f * kMaxDriftMs * 0.001f * (float) sr
                                * (frozen ? 0.5f : 1.0f));

    delay.setParams (params.delay);
}

void NebulaEngine::updateControlRate() noexcept
{
    const auto frozen    = params.freeze;
    const auto sizeScale = smSizeScale.getCurrent();
    const auto shimmer   = smShimmer.getCurrent();

    // These two only ever get read here, so this is where they are advanced. Setting a
    // target without ever stepping the smoother is the classic way to end up with a
    // control that looks connected and does nothing.
    const auto decay     = smDecay.skip (kControlInterval);
    const auto diffusion = smDiffusion.skip (kControlInterval);
    const auto regen     = smFeedback.skip (kControlInterval);

    // ---- the governor -------------------------------------------------------
    // Nothing at all below the ceiling. Above it, a fractional exponent rather than a
    // straight ratio, so being twice too loud asks for a 26 % trim instead of a 50 %
    // one - the network eases back into range over several seconds instead of being
    // yanked, and a loud passage does not leave a hole behind it.
    const auto over = loopEnergy / kLoopCeiling;
    const auto desired = over > 1.0f ? std::pow (over, -kGovernorRatio) : 1.0f;

    regGain += (desired - regGain) * regCoeff;
    regGain = clamp (regGain, kGovernorFloor, 1.0f);

    // ---- feedback -----------------------------------------------------------
    // Per-line gain from a target RT60, so every line decays over the same time even
    // though they are all different lengths. Without this the short lines die first
    // and the tail collapses into a few slow echoes.
    if (frozen || decay >= kInfiniteDecay)
    {
        for (int i = 0; i < kNumLines; ++i)
            feedback[i] = 1.0f;
    }
    else
    {
        const auto rt60 = 0.25f * std::pow (240.0f, decay);   // 0.25 s .. 60 s

        for (int i = 0; i < kNumLines; ++i)
        {
            const auto d = baseDelay[i] * sizeScale;
            feedback[i] = std::pow (10.0f, -3.0f * d / (rt60 * (float) sr));
        }
    }

    // The shimmer path injects energy of its own. Trading a little of the direct
    // feedback for it keeps the overall decay roughly where the knob says it is.
    //
    // This applies to the DECAY gain only, and must stay on this side of the blend
    // below. Applied afterwards it also scaled the regenerated gain, so 30 % shimmer
    // turned a loop gain of 1.038 into 0.945 and the network died in seconds no matter
    // where Feedback was set - while a test with shimmer at zero saw nothing wrong.
    const auto shimmerComp = 1.0f - 0.30f * shimmer;

    for (int i = 0; i < kNumLines; ++i)
        feedback[i] = std::min (feedback[i] * shimmerComp, 1.0f);

    // ---- regeneration -------------------------------------------------------
    // Feedback pulls the loop gain away from whatever the decay curve asked for and up
    // to just above unity, where the network sustains indefinitely and still takes new
    // input - which is the difference between this and Freeze, and the reason it can be
    // left running for hours. The governor only ever trims the regenerated share, so at
    // 0 % feedback the decay behaviour above is untouched, bit for bit.
    const auto governed = (1.0f - regen) + regen * regGain;

    for (int i = 0; i < kNumLines; ++i)
        feedback[i] = (feedback[i] * (1.0f - regen) + kRegenGain * regen) * governed;

    // ---- loop filters -------------------------------------------------------
    // Frozen, these open up: a damping filter inside a unity-gain loop is still a
    // loss, and "infinite" has to mean it.
    const auto dampHz = frozen ? params.highCutHz
                               : std::min (params.highCutHz,
                                           18000.0f * std::pow (0.045f, params.damping));
    const auto cutHz  = frozen ? 20.0f : params.lowCutHz;

    for (int i = 0; i < kNumLines; ++i)
    {
        damp[i].setCutoff (dampHz, sr);
        cut[i].setCutoff (cutHz, sr);
        modLfo[i].setRate (params.modRateHz * (0.72f + 0.09f * (float) i), sr);
    }

    // ---- diffusion ----------------------------------------------------------
    const auto apGain = 0.78f * diffusion;
    // Capped: the diffusers are allocated at twice their nominal length, and past
    // about this much scaling they stop diffusing and start echoing anyway.
    const auto apScale = 0.45f + 0.55f * std::min (sizeScale, 2.5f);

    for (int k = 0; k < 4; ++k)
    {
        diffuseL[k].setGain (apGain);
        diffuseR[k].setGain (apGain);
        diffuseL[k].setDelay (kDiffuseMsL[k] * 0.001f * (float) sr * apScale);
        diffuseR[k].setDelay (kDiffuseMsR[k] * 0.001f * (float) sr * apScale);
    }

    // ---- pitch voices -------------------------------------------------------
    // The pair is detuned symmetrically, so "detune" widens the shimmer into a chorus
    // of itself instead of transposing it.
    shimmerA.setPitch (params.shimmerSemis,  params.detuneCents);
    shimmerB.setPitch (params.shimmerSemis, -params.detuneCents * 1.13f);
    subShifter.setPitch (-12.0f, params.detuneCents * 0.25f);

    delay.updateControlRate();

    // ---- early field --------------------------------------------------------
    early.setSize (sizeScale);

    // Eighteen interpolated reads a sample is not a price worth paying while Space is
    // at zero. The line keeps being written either way, so turning it up does not start
    // with a hole in it.
    earlyActive = params.space > 0.0f || smSpace.getCurrent() > 1.0e-6f;
}

void NebulaEngine::process (float* left, float* right, int numSamples) noexcept
{
    const auto frozen = params.freeze;

    updateTargets();

    for (int n = 0; n < numSamples; ++n)
    {
        if (controlCounter <= 0)
        {
            updateControlRate();
            controlCounter = kControlInterval;
        }
        --controlCounter;

        const auto dryL = left[n];
        const auto dryR = right[n];

        // ---- pre-delay ------------------------------------------------------
        const auto preSamples = smPreDelay.next();
        preDelayL.write (dryL);
        preDelayR.write (dryR);
        auto inL = preDelayL.read (preSamples);
        auto inR = preDelayR.read (preSamples);
        preDelayL.advance();
        preDelayR.advance();

        // ---- the delay ------------------------------------------------------
        // Fed from the dry input, so its repeats are clean before the space gets hold
        // of them. Its output goes two ways: through the diffusers into the network,
        // where it blooms, and straight to the wet output further down, where it stays
        // a repeat you can count. Added before the input trim, so Freeze - which shuts
        // that trim - holds the space and leaves the delay echoing over the top of it.
        float delayL = 0.0f, delayR = 0.0f;
        delay.process (inL, inR, delayL, delayR);

        inL += delayL * kDelayToNetwork;
        inR += delayR * kDelayToNetwork;

        // ---- early reflections ----------------------------------------------
        // Fed from the input and the delay both, so a repeat arrives in the same room
        // the source did. Its own output is not fed back into it - the line it reads
        // was written a line above this.
        const auto space = smSpace.next();
        float earlyL = 0.0f, earlyR = 0.0f;

        if (earlyActive)
        {
            early.process (inL, inR, earlyL, earlyR);
            inL += earlyL * space * kEarlyToNetwork;
            inR += earlyR * space * kEarlyToNetwork;
        }
        else
        {
            early.skip (inL, inR);
        }

        // ---- input diffusion ------------------------------------------------
        for (int k = 0; k < 4; ++k)
        {
            inL = diffuseL[k].process (inL);
            inR = diffuseR[k].process (inR);
        }

        const auto inputGain = smInput.next();
        inL *= inputGain;
        inR *= inputGain;

        // ---- read the network -----------------------------------------------
        const auto sizeScale  = smSizeScale.next();
        const auto modDepth   = smModDepth.next() * std::min (1.0f, sizeScale);
        const auto driftDepth = smDriftDepth.next() * std::min (1.0f, sizeScale);

        float v[kNumLines];

        for (int i = 0; i < kNumLines; ++i)
        {
            const auto d = baseDelay[i] * sizeScale
                             + modLfo[i].next()   * modDepth
                             + driftLfo[i].next() * driftDepth;
            v[i] = lines[i].read (d);
        }

        // Alternating signs across interleaved lines: the two output taps then share
        // no line in the same polarity, which is where the stereo image comes from.
        auto wetL = 0.5f * (v[0] - v[2] + v[4] - v[6]);
        auto wetR = 0.5f * (v[1] - v[3] + v[5] - v[7]);

        // ---- pitched feedback paths ------------------------------------------
        const auto sendMono = 0.5f * (wetL + wetR);

        // What the governor watches: the signal actually circulating, read before the
        // output trim so the reading does not move when the user changes the output.
        loopEnergy += energyCoeff * (std::abs (sendMono) - loopEnergy);
        const auto shimGain = smShimmer.next() * 0.78f;
        const auto massGain = smMass.next() * 0.62f;

        const auto shimSend = shimmerHP.process (sendMono);
        const auto voiceA   = shimmerA.process (shimSend);
        const auto voiceB   = shimmerB.process (shimSend);
        const auto subVoice = subLP.process (subShifter.process (sendMono));

        // ---- feedback matrix and write ---------------------------------------
        hadamard8 (v);

        const auto drive    = smDrive.next();
        const auto driveInv = 1.0f / drive;

        for (int i = 0; i < kNumLines; ++i)
        {
            auto x = v[i] * feedback[i]
                       + ((i & 1) ? inR : inL)
                       + shimGain * ((i & 1) ? voiceB : voiceA)
                       + massGain * subVoice;

            if (! frozen)
            {
                x = damp[i].process (x);
                x = cut[i].process (x);
            }

            // Linear below threshold, so at low drive this is a pure safety net and
            // colours nothing; wound up, it is the collapse.
            x = softClip (x * drive) * driveInv;

            lines[i].write (killDenormal (x));
        }

        for (int i = 0; i < kNumLines; ++i)
            lines[i].advance();

        // ---- output stage -----------------------------------------------------
        wetL *= kOutputTrim;
        wetR *= kOutputTrim;

        // The room joins the tail, and the Reverb control sets what the two of them
        // together are worth. At 0 % the space disappears and only the delay is left,
        // which is the setting for using this as a delay with a reverb attached rather
        // than the other way round.
        const auto reverbLevel = smReverb.next();
        const auto earlyLevel  = space * kEarlyLevel;

        wetL = (wetL + earlyL * earlyLevel) * reverbLevel;
        wetR = (wetR + earlyR * earlyLevel) * reverbLevel;

        // The repeats join the wet path here, in the same units the network's output
        // has just been trimmed into - so everything below this line, width, DC and the
        // wet limiter included, treats the two the same.
        wetL += delayL;
        wetR += delayR;

        const auto width = smWidth.next();
        const auto mid   = 0.5f * (wetL + wetR);
        const auto side  = 0.5f * (wetL - wetR) * width;
        wetL = mid + side;
        wetR = mid - side;

        wetL = dcL.process (wetL);
        wetR = dcR.process (wetR);

        // Bound the wet path only. The dry path is never touched, so the plug-in stays
        // bit-transparent at 0 % mix however hard it is being driven.
        wetL = softClip (wetL * 0.7f) * (1.0f / 0.7f);
        wetR = softClip (wetR * 0.7f) * (1.0f / 0.7f);

        const auto mix = smMix.next();
        const auto wetGain = std::sin (mix * kPi * 0.5f);
        const auto dryGain = std::cos (mix * kPi * 0.5f);
        const auto out = smOutput.next();

        auto outL = (dryL * dryGain + wetL * wetGain) * out;
        auto outR = (dryR * dryGain + wetR * wetGain) * out;

        // Everything, dry included, because the question this control answers is what
        // the far end of the chain is going to hear. Ramped rather than switched, and
        // at zero it is arithmetically absent: adding zero times anything leaves both
        // channels exactly as they were.
        // Written as a crossfade rather than as "add the difference": both ends of a
        // crossfade are exact, and x + (m - x) is not x at one end nor m at the other -
        // it lands an ulp away from each, which is a channel difference of 1.2e-07 in a
        // control whose entire job is that there is no difference.
        const auto monoAmount = smMono.next();
        const auto summed = 0.5f * (outL + outR);
        outL = outL * (1.0f - monoAmount) + summed * monoAmount;
        outR = outR * (1.0f - monoAmount) + summed * monoAmount;

        left[n]  = outL;
        right[n] = outR;

        // ---- visualiser followers ---------------------------------------------
        const auto wetMono = 0.5f * (wetL + wetR);
        const auto high = wetMono - brightSplit.process (wetMono);
        levelFollower  += 0.0006f * (std::abs (wetMono) - levelFollower);
        brightFollower += 0.0006f * (std::abs (high) - brightFollower);
    }

    wetLevel   = clamp (levelFollower * 3.0f, 0.0f, 1.0f);
    brightness = clamp (brightFollower / (levelFollower + 1.0e-5f), 0.0f, 1.0f);
}

} // namespace dying::dsp
