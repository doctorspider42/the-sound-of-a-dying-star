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

/*  Small DSP primitives. Deliberately free of any JUCE include so the whole engine
    stays portable and testable outside a plug-in host.                            */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dying::dsp
{

inline constexpr float kPi    = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

template <typename T>
inline T clamp (T v, T lo, T hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

inline float dbToGain (float db)  noexcept { return std::pow (10.0f, db * 0.05f); }
inline float semisToRatio (float s) noexcept { return std::pow (2.0f, s * (1.0f / 12.0f)); }
inline float centsToRatio (float c) noexcept { return std::pow (2.0f, c * (1.0f / 1200.0f)); }

/** Anything smaller than this is denormal territory on the way down; killing it costs
    one compare and saves the FPU stalls that show up as random CPU spikes in long
    reverb tails. */
inline float killDenormal (float v) noexcept
{
    return (v > -1.0e-18f && v < 1.0e-18f) ? 0.0f : v;
}

/** Exponential (one-pole) parameter smoother. Never reaches the target exactly, which
    is fine for gains and delay times and much cheaper than a ramp counter. */
class Smoothed
{
public:
    void prepare (double sampleRate, float timeMs, float initial = 0.0f) noexcept
    {
        setTime (sampleRate, timeMs);
        reset (initial);
    }

    void setTime (double sampleRate, float timeMs) noexcept
    {
        const auto samples = std::max (1.0, (double) timeMs * 0.001 * sampleRate);
        coeff = (float) std::exp (-1.0 / samples);
    }

    void reset (float value) noexcept { current = target = value; }
    void setTarget (float value) noexcept { target = value; }

    /** Jump to the target without ramping - for a reset, where there is no previous
        signal for a ramp to protect. */
    void snap() noexcept { current = target; }

    float getTarget() const noexcept { return target; }
    float getCurrent() const noexcept { return current; }

    float next() noexcept
    {
        current = target + (current - target) * coeff;
        return current;
    }

    /** Advance by n samples in one step - for control-rate updates. */
    float skip (int n) noexcept
    {
        current = target + (current - target) * std::pow (coeff, (float) n);
        return current;
    }

private:
    float coeff = 0.0f, current = 0.0f, target = 0.0f;
};

/** One-pole low pass. Used for loop damping, where its gentle 6 dB/oct slope is
    exactly what you want - a steeper filter makes the tail sound like it is being
    switched off rather than absorbed. */
class OnePoleLP
{
public:
    void setCutoff (float hz, double sampleRate) noexcept
    {
        const auto w = kTwoPi * clamp (hz, 10.0f, (float) (sampleRate * 0.49)) / (float) sampleRate;
        a = std::exp (-w);
    }

    void reset() noexcept { z = 0.0f; }

    float process (float x) noexcept
    {
        z = killDenormal (x + a * (z - x));
        return z;
    }

private:
    float a = 0.0f, z = 0.0f;
};

/** One-pole high pass (complement of the above). */
class OnePoleHP
{
public:
    void setCutoff (float hz, double sampleRate) noexcept { lp.setCutoff (hz, sampleRate); }
    void reset() noexcept { lp.reset(); }
    float process (float x) noexcept { return x - lp.process (x); }

private:
    OnePoleLP lp;
};

/** Removes any DC the saturator's asymmetry or a long feedback loop accumulates. */
class DCBlocker
{
public:
    void reset() noexcept { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        const auto y = x - x1 + 0.9995f * y1;
        x1 = x;
        y1 = killDenormal (y);
        return y1;
    }

private:
    float x1 = 0.0f, y1 = 0.0f;
};

/** Soft clipper that is *exactly* linear below the threshold, so it colours nothing
    until the loop is actually being driven, then bounds hard at +/-1. That property is
    what lets the same function sit in the feedback path as both a safety net and the
    "collapse" distortion. */
inline float softClip (float x) noexcept
{
    constexpr float t = 0.7f;
    const auto ax = std::abs (x);

    if (ax <= t)
        return x;

    const auto over = (ax - t) / (1.0f - t);
    const auto y = t + (1.0f - t) * std::tanh (over);
    return x < 0.0f ? -y : y;
}

/** Cheap deterministic noise for LFO phase offsets and the star field. */
class Xorshift
{
public:
    explicit Xorshift (uint32_t seed = 0x9e3779b9u) noexcept : s (seed | 1u) {}

    uint32_t nextInt() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }

    /** Uniform in [0, 1). */
    float nextFloat() noexcept { return (float) (nextInt() >> 8) * (1.0f / 16777216.0f); }

    /** Uniform in [-1, 1). */
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

private:
    uint32_t s;
};

/** Sine LFO. Phase is kept as a normalised float so retuning the rate never clicks. */
class LFO
{
public:
    void reset (float startPhase = 0.0f) noexcept { phase = startPhase; }

    void setRate (float hz, double sampleRate) noexcept
    {
        inc = hz / (float) sampleRate;
    }

    float next() noexcept
    {
        phase += inc;
        while (phase >= 1.0f) phase -= 1.0f;
        return std::sin (kTwoPi * phase);
    }

private:
    float phase = 0.0f, inc = 0.0f;
};

} // namespace dying::dsp
