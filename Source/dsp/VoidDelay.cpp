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

#include "VoidDelay.h"

namespace dying::dsp
{

namespace
{
    /** Deep modulation on a 40 ms slap is vibrato; on a two-second repeat it is tape.
        The depth is scaled with the time further down so the character holds across
        the whole range. */
    constexpr float kMaxWobbleMs = 16.0f;

    /** How flat the abyss voice runs at the top of the control, on a repeat spaced far
        enough apart to count. Two and a half semitones per pass is enough that a
        handful of repeats have visibly fallen, and little enough that the first one
        still sounds like the source. */
    constexpr float kMaxSagSemis = -2.6f;

    /** How far the morph modulator swings the pitch voices at the top of the control.
        A fifth either way: far enough that consecutive repeats are recognisably
        different notes rather than the same one detuned. */
    constexpr float kMaxMorphSemis = 7.0f;

    /** The delay time everything per-pass is calibrated against. A repeat every 20 ms
        goes round fifteen times as often as one every 300 ms, so the same per-pass
        darkening, drag or loss is fifteen times as much of it per second - which is
        why a short delay with any tone at all used to be swallowed after four repeats.
        Everything cumulative below is scaled by the ratio of the two, so the controls
        mean the same thing per second wherever the Time control is. */
    constexpr float kNominalPassMs = 300.0f;

    /** Loop gain at maximum feedback. Just past unity, for the same reason as the
        reverb's: everything in the path - interpolation, filtering, the pitch voices -
        is a small loss per pass, and a delay meant to run forever has to outrun them. */
    constexpr float kMaxLoopGain = 1.06f;

    /** How long the voice-return followers look back. Long enough that they measure the
        shifter rather than the music going through it, short enough to follow a change
        of window or interval within a repeat or two. */
    constexpr float kVoiceEnvSec = 0.25f;

    /** Bounds on that correction, so a pathological measurement cannot become a loop
        gain of its own. The honest value lives around 1.25. */
    constexpr float kMinVoiceComp = 0.5f;
    constexpr float kMaxVoiceComp = 2.0f;

    // The clipper inside the loop is what makes any of this safe; the governor only
    // stops a regenerating line from living permanently inside it. Below the ceiling it
    // does nothing at all.
    constexpr float kLoopCeiling   = 0.20f;
    constexpr float kGovernorRatio = 0.35f;
    constexpr float kGovernorFloor = 0.88f;

    constexpr float kEnergyTimeSec = 0.35f;
    constexpr float kRegTimeSec    = 2.5f;

    /** Calibrated so that a repeat at 100 % delay mix comes back at about the level of
        the source, the same convention the reverb's output trim follows. */
    constexpr float kDelayTrim = 0.9f;

    // ---- the bouncing ball ---------------------------------------------------
    /** How much of the spacing one bounce takes away at the end of the control.

        A run-down takes roughly one spacing divided by this, so the number has to cover
        a two-second gap as well as a fifty-millisecond one: at a tenth, two seconds
        could not be run down in less than twenty. At a quarter it can be done in eight,
        and the square law below still leaves the gentle settings - the ones that take
        fifteen or thirty seconds - across most of the travel. */
    constexpr float kMaxContraction = 0.24f;

    /** How short the spacing is allowed to get, as a fraction of where it started. Past
        this the repeats are a tone rather than a rattle and there is nothing left to
        hear happening. */
    constexpr float kBounceFloor = 0.045f;

    /** And how long, going the other way. */
    constexpr float kBounceCeiling = 4.0f;

    /** The crossfade that hides each step. Long enough to be inaudible, short enough
        that a fast rattle is not one continuous crossfade. */
    constexpr float kBounceXfadeMs = 6.0f;

    /** A strike has to be this much above the running level to be a new one, and
        nothing retriggers within this long of the last. */
    constexpr float kOnsetRatio    = 2.5f;
    constexpr float kOnsetFloor    = 0.0015f;
    constexpr float kOnsetLockSec  = 0.15f;
}

void VoidDelay::prepare (double sampleRate)
{
    sr = sampleRate;

    // The right head reads a little further out than the left, and both modulators sit
    // on top of that.
    const auto longestMs = kMaxDelayMs * 1.03f + kMaxWobbleMs * 3.0f;
    lineLength = (int) (longestMs * 0.001f * (float) sr) + 64;

    for (int ch = 0; ch < 2; ++ch)
    {
        lines[ch].prepare (lineLength);
        rumbleHP[ch].setCutoff (28.0f, sr);

        // Different windows per channel and per voice: identical shifters flutter in
        // lockstep, and two of those in a feedback path is a beating tone.
        riser[ch].prepare (sr, ch == 0 ? 88.0f : 96.0f);
        sag[ch].prepare   (sr, ch == 0 ? 140.0f : 152.0f);

        wobbleLfo[ch].setRate (ch == 0 ? 0.63f : 0.71f, sr);
        driftLfo[ch] .setRate (ch == 0 ? 0.043f : 0.037f, sr);
    }

    // The morph modulators are advanced once per control block rather than per sample -
    // they are far too slow to need more - so their rates are scaled by the interval
    // they are stepped at. Four incommensurate rates, so the four voices never come
    // back into line and the wander never repeats.
    const float morphHz[4] = { 0.047f, 0.031f, 0.019f, 0.013f };

    for (int v = 0; v < 4; ++v)
        morphLfo[v].setRate (morphHz[v] * (float) kControlInterval, sr);

    constexpr float fast = 30.0f, slow = 80.0f;
    smTime    .prepare (sr, 220.0f, params.timeMs * 0.001f * (float) sr);
    smFeedback.prepare (sr, 60.0f,  0.0f);
    smSpread  .prepare (sr, 60.0f,  params.spread);
    smShimmer .prepare (sr, slow,   params.shimmer);
    smAbyss   .prepare (sr, slow,   params.abyss);
    smWobble  .prepare (sr, slow,   0.0f);
    smMix     .prepare (sr, fast,   params.mix);
    smDrive   .prepare (sr, slow,   1.0f);
    smEngage  .prepare (sr, 25.0f,  0.0f);

    energyCoeff = 1.0f - std::exp (-1.0f / (kEnergyTimeSec * (float) sr));
    regCoeff    = 1.0f - std::exp (-(float) kControlInterval / (kRegTimeSec * (float) sr));
    voiceCoeff  = 1.0f - std::exp (-1.0f / (kVoiceEnvSec * (float) sr));

    onsetAttack      = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    onsetFastRelease = 1.0f - std::exp (-1.0f / (0.120f * (float) sr));
    onsetSlowRelease = 1.0f - std::exp (-1.0f / (1.500f * (float) sr));

    reset();
}

void VoidDelay::reset() noexcept
{
    Xorshift rng (0x2b71c0deu);

    for (int ch = 0; ch < 2; ++ch)
    {
        lines[ch].reset();
        toneLP[ch].reset();
        rumbleHP[ch].reset();
        riser[ch].reset();
        sag[ch].reset();
        dc[ch].reset();
        wobbleLfo[ch].reset (rng.nextFloat());
        driftLfo[ch].reset (rng.nextFloat());
    }

    for (int v = 0; v < 4; ++v)
        morphLfo[v].reset (rng.nextFloat());

    loopEnergy = 0.0f;
    regGain = 1.0f;
    silentSamples = 0;

    for (int ch = 0; ch < 2; ++ch)
    {
        riseEnvIn[ch] = riseEnvOut[ch] = sagEnvIn[ch] = sagEnvOut[ch] = 0.0f;
        riseComp[ch] = sagComp[ch] = 1.0f;
    }

    bounceScale = bouncePrevScale = 1.0f;
    bounceXfade = 0;
    sinceBounce = 0;
    stepCountdown = 0;
    onsetFast = onsetSlow = 0.0f;
    onsetLockout = 0;

    updateControlRate();

    for (auto* s : { &smTime, &smFeedback, &smSpread, &smShimmer, &smAbyss, &smWobble,
                     &smMix, &smDrive, &smEngage })
        s->snap();

    // Nothing is circulating, so an engine that starts up switched off starts up doing
    // no work at all rather than a line length of it.
    dormant = ! params.enabled;
}

void VoidDelay::updateControlRate() noexcept
{
    if (params.enabled)
    {
        dormant = false;
        silentSamples = 0;
    }

    const auto timeMs = clamp (params.timeMs, kMinDelayMs, kMaxDelayMs);
    const auto abyss  = clamp (params.abyss, 0.0f, 1.0f);
    const auto morph  = clamp (params.morph, 0.0f, 1.0f);

    // ---- the bouncing ball --------------------------------------------------
    // Each repeat lands a little sooner than the last, the way a dropped thing does.
    // Negative and it goes the other way: the repeats spread apart instead.
    const auto bounce = clamp (params.bounce, -1.0f, 1.0f);
    const auto wasActive = bounceActive;

    bounceActive = std::abs (bounce) > 0.0f;
    bounceRatio  = 1.0f - kMaxContraction * bounce * std::abs (bounce);

    // Never past the ends of the Time control's own range, whichever way it is going.
    bounceFloor   = std::max (kBounceFloor, kMinDelayMs / timeMs);
    bounceCeiling = std::min (kBounceCeiling, kMaxDelayMs / timeMs);

    if (wasActive && ! bounceActive)
    {
        // Switched off, the spacing goes back to what the Time control says - through
        // the same crossfade, so turning the knob down is not a click.
        if (bounceScale < 0.999f || bounceScale > 1.001f)
            stepBounce (1.0f, timeMs * 0.001f * (float) sr);

        sinceBounce = 0;
    }

    // Everything that accumulates pass by pass is scaled by how often a pass actually
    // happens - which, while the ball is bouncing, is not what the Time control says.
    perPass = std::min (1.0f, timeMs * bounceScale / kNominalPassMs);

    smEngage  .setTarget (params.enabled ? 1.0f : 0.0f);
    smTime    .setTarget (timeMs * 0.001f * (float) sr);
    smSpread  .setTarget (clamp (params.spread, 0.0f, 1.0f));
    smShimmer .setTarget (clamp (params.shimmer, 0.0f, 1.0f));
    smAbyss   .setTarget (abyss);
    smMix     .setTarget (clamp (params.mix, 0.0f, 1.0f));
    smDrive   .setTarget (1.0f + 13.0f * abyss);
    smWobble  .setTarget (clamp (params.wobble, 0.0f, 1.0f) * kMaxWobbleMs
                              * perPass * 0.001f * (float) sr);

    // Tone decides how fast the repeats go dark; the abyss drags them further down
    // still, because something falling into a well loses its top end first.
    const auto toneHz = clamp (18000.0f * std::pow (0.035f, clamp (params.tone, 0.0f, 1.0f) * perPass)
                                        * std::pow (0.30f, abyss * perPass),
                               180.0f, 18000.0f);

    // The pitch voices wander. Four modulators at rates that share no factor, so the
    // riser is somewhere different every time a repeat passes through it and the
    // repeats morph between intervals instead of stacking up on one.
    const auto morphSemis = kMaxMorphSemis * morph;
    const float wander[4] = { morphLfo[0].next() * morphSemis, morphLfo[1].next() * morphSemis,
                              morphLfo[2].next() * morphSemis, morphLfo[3].next() * morphSemis };

    // The shifter smears by about half its window. At 90 ms that is inaudible inside a
    // half-second repeat and exactly what a shimmer wants; inside a 25 ms one it would
    // blur four repeats into a wash, so the window comes in with the delay time.
    const auto windowSamples = timeMs * 0.8f * 0.001f * (float) sr;

    for (int ch = 0; ch < 2; ++ch)
    {
        toneLP[ch].setCutoff (toneHz, sr);

        riser[ch].setWindow (windowSamples);
        sag[ch].setWindow (windowSamples);

        riser[ch].setPitch (params.pitchSemis + wander[ch], ch == 0 ? 4.0f : -5.5f);

        // Squared, so the first half of the control is a barely-there sag and the top
        // of it is the fall.
        sag[ch].setPitch (kMaxSagSemis * abyss * abyss * perPass + wander[2 + ch] * 0.5f,
                          ch == 0 ? 0.0f : -3.0f);
    }

    // ---- what the pitch voices give back ------------------------------------
    for (int ch = 0; ch < 2; ++ch)
    {
        riseComp[ch] = clamp (std::sqrt ((riseEnvIn[ch] + 1.0e-12f)
                                             / (riseEnvOut[ch] + 1.0e-12f)),
                              kMinVoiceComp, kMaxVoiceComp);
        sagComp[ch]  = clamp (std::sqrt ((sagEnvIn[ch] + 1.0e-12f)
                                             / (sagEnvOut[ch] + 1.0e-12f)),
                              kMinVoiceComp, kMaxVoiceComp);
    }

    // ---- the governor -------------------------------------------------------
    const auto over = loopEnergy / kLoopCeiling;
    const auto desired = over > 1.0f ? std::pow (over, -kGovernorRatio) : 1.0f;

    regGain += (desired - regGain) * regCoeff;
    regGain = clamp (regGain, kGovernorFloor, 1.0f);

    // The trim is per pass, and at 25 ms a pass happens forty times a second - so a
    // governor holding a long delay a fraction of a dB down would hold a short one
    // thirty dB down and call it the same setting. Raising it to the spacing makes the
    // trim mean the same thing per second wherever the Time control is.
    const auto governor = std::pow (regGain, perPass);

    // Nothing is governed until the loop is near sustaining. A delay set to fade has to
    // fade exactly as asked however loud the passage going into it - the governor is
    // there for the settings that never end, and for nothing else.
    const auto raw = kMaxLoopGain * clamp (params.feedback, 0.0f, 1.0f);
    const auto governed = clamp ((raw - 0.90f) / 0.14f, 0.0f, 1.0f);

    smFeedback.setTarget (raw * (1.0f - governed + governed * governor));
}

void VoidDelay::stepBounce (float newScale, float delaySamples) noexcept
{
    bouncePrevScale = bounceScale;
    bounceScale = newScale;

    // Both heads read at a fixed offset for the length of the fade, so both play at
    // exactly normal speed and the step costs no pitch at all - which is the whole
    // reason the spacing is stepped rather than swept.
    const auto nominal = (int) (kBounceXfadeMs * 0.001f * (float) sr);
    const auto shortest = (int) (delaySamples * std::min (bounceScale, bouncePrevScale) * 0.4f);
    bounceXfadeLen = std::max (32, std::min (nominal, std::max (32, shortest)));
    bounceXfade = bounceXfadeLen;
}

void VoidDelay::process (float inL, float inR, float& outL, float& outR) noexcept
{
    outL = 0.0f;
    outR = 0.0f;

    if (dormant)
        return;

    const auto engage   = smEngage.next();
    const auto timeL    = smTime.next();
    const auto wobble   = smWobble.next();
    const auto spread   = smSpread.next();
    const auto shimmer  = smShimmer.next();
    const auto abyss    = smAbyss.next();
    const auto drive    = smDrive.next();
    const auto driveInv = 1.0f / drive;
    const auto fbGain   = smFeedback.next() * engage;
    const auto mix      = smMix.next() * engage;

    // The right head sits a couple of per cent further out, so the two lines never
    // phase-lock into a single mono repeat however wide the cross-feed is opened.
    const auto timeR = timeL * (1.0f + 0.021f * spread);

    // ---- the bouncing ball --------------------------------------------------
    if (bounceActive)
    {
        // A strike well above the running level is a new one, and drops the ball from
        // the top again. Without this the ball runs down once and the next thing played
        // arrives into a rattle instead of starting its own.
        const auto strike = 0.5f * (std::abs (inL) + std::abs (inR)) * engage;
        onsetFast += (strike > onsetFast ? onsetAttack : onsetFastRelease)
                         * (strike - onsetFast);
        onsetSlow += (strike > onsetSlow ? onsetAttack * 0.25f : onsetSlowRelease)
                         * (strike - onsetSlow);

        if (onsetLockout > 0)
            --onsetLockout;

        const auto restart = onsetLockout == 0
                               && onsetFast > onsetSlow * kOnsetRatio + kOnsetFloor;

        if (restart && std::abs (bounceScale - 1.0f) > 0.001f && bounceXfade <= 0)
        {
            stepBounce (1.0f, timeL);
            sinceBounce = 0;
            onsetLockout = (int) (kOnsetLockSec * (float) sr);
        }
        else if (restart)
        {
            onsetLockout = (int) (kOnsetLockSec * (float) sr);
        }

        // One repeat has gone by. The step itself waits for the middle of the gap after
        // it: at that instant the read head is over the quiet between two repeats, so a
        // jump either way lands on quiet too. Stepping on the repeat instead is fine
        // going down - the head lands past the repeat it has just played - but going up
        // it lands just behind one and plays it a second time, which measures as a pair
        // of hits 20 ms apart and sounds like a flam.
        const auto period = (int) (timeL * bounceScale);

        if (++sinceBounce >= period)
        {
            sinceBounce = 0;
            stepCountdown = std::max (1, period / 2);
        }

        if (stepCountdown > 0 && --stepCountdown == 0 && bounceXfade <= 0)
        {
            const auto next = clamp (bounceScale * bounceRatio, bounceFloor, bounceCeiling);

            if (std::abs (next - bounceScale) > 1.0e-6f)
                stepBounce (next, timeL);
        }
    }

    // Both blends in the loop are normalised in power rather than in amplitude. A
    // pitch-shifted copy is decorrelated from what it was made from, so the two do not
    // sum the way a crossfade assumes and an amplitude blend quietly loses a tenth of
    // the loop gain at every setting in the middle of the control - which is a delay
    // that dies in half a minute with the feedback at maximum, and a bug that hides
    // completely from any test run with Shimmer at zero. The voices themselves are
    // corrected to unit return first, by measurement, so this holds at any window.
    const auto riseDry = 1.0f - 0.55f * shimmer;
    const auto riseWet = 1.05f * shimmer;
    const auto riseNorm = 1.0f / std::sqrt (riseDry * riseDry + riseWet * riseWet);

    const auto sagDry = 1.0f - 0.75f * abyss;
    const auto sagWet = 0.95f * abyss;
    const auto sagNorm = 1.0f / std::sqrt (sagDry * sagDry + sagWet * sagWet);

    float shaped[2];

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto base = ch == 0 ? timeL : timeR;
        const auto move = wobbleLfo[ch].next() * wobble
                            + driftLfo[ch].next() * wobble * 1.7f;

        auto x = lines[ch].read (base * bounceScale + move);

        if (bounceXfade > 0)
        {
            // Raised cosine, so neither end of the fade has a corner in it.
            const auto t = (float) bounceXfade / (float) bounceXfadeLen;
            const auto old = 0.5f - 0.5f * std::cos (kPi * t);
            x = x * (1.0f - old) + lines[ch].read (base * bouncePrevScale + move) * old;
        }
        x = toneLP[ch].process (x);
        x = rumbleHP[ch].process (x);

        // Up an interval, blended rather than added, so the repeats climb away instead
        // of piling on top of themselves.
        const auto riseIn = x;
        const auto up = riser[ch].process (riseIn);
        riseEnvIn[ch]  += voiceCoeff * (riseIn * riseIn - riseEnvIn[ch]);
        riseEnvOut[ch] += voiceCoeff * (up * up - riseEnvOut[ch]);
        x = (x * riseDry + up * riseWet * riseComp[ch]) * riseNorm;

        // ... and down, a little flatter each pass. This is the one that turns a delay
        // into something falling.
        const auto sagIn = x;
        const auto down = sag[ch].process (sagIn);
        sagEnvIn[ch]  += voiceCoeff * (sagIn * sagIn - sagEnvIn[ch]);
        sagEnvOut[ch] += voiceCoeff * (down * down - sagEnvOut[ch]);
        x = (x * sagDry + down * sagWet * sagComp[ch]) * sagNorm;

        // Linear below its threshold, so at low Abyss this is purely the guarantee that
        // an over-unity loop cannot run away; wound up, it is the aggression.
        x = softClip (x * drive) * driveInv;
        shaped[ch] = dc[ch].process (x);
    }

    loopEnergy += energyCoeff * (0.5f * (std::abs (shaped[0]) + std::abs (shaped[1]))
                                     - loopEnergy);

    // An orthogonal rotation rather than a blend: at full spread the two channels swap
    // outright and the repeats bounce, and everywhere in between the pair keeps the
    // energy - and the stereo - it started with. A plain crossfade would collapse the
    // loop to mono half way along the control.
    const auto theta = spread * kPi * 0.5f;
    const auto c = std::cos (theta), s = std::sin (theta);
    const auto fbL = c * shaped[0] + s * shaped[1];
    const auto fbR = c * shaped[1] - s * shaped[0];

    // The input follows the same idea from the other end: as the cross-feed opens, the
    // source is folded into the left line alone, which is what makes a mono part bounce
    // rather than simply arrive in both channels at once.
    const auto norm = 1.0f / (1.0f + spread);
    const auto writeL = killDenormal ((inL + spread * inR) * norm * engage + fbGain * fbL);
    const auto writeR = killDenormal ((1.0f - spread) * inR * norm * engage + fbGain * fbR);

    lines[0].write (writeL);
    lines[1].write (writeR);
    lines[0].advance();
    lines[1].advance();

    if (bounceXfade > 0)
        --bounceXfade;

    // Switched off, the loop is fed silence and fades over the engage ramp. Once a
    // whole line length of silence has gone in there is provably nothing left to read,
    // and the whole thing can stop costing anything.
    if (std::abs (writeL) < 1.0e-9f && std::abs (writeR) < 1.0e-9f)
    {
        if (++silentSamples > lineLength)
            dormant = true;
    }
    else
    {
        silentSamples = 0;
    }

    outL = shaped[0] * mix * kDelayTrim;
    outR = shaped[1] * mix * kDelayTrim;
}

} // namespace dying::dsp
