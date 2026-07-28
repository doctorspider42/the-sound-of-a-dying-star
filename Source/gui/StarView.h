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

/*  The visualiser. It is not decoration: the colour, the size of the core, the speed
    of the accretion disk and whether an event horizon has opened are all read straight
    off the controls, so the picture tells you what the reverb is doing before you have
    played a note through it.                                                        */

#pragma once

#include "Skin.h"
#include "../dsp/Utils.h"

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dying
{

/** Everything the star needs, pulled once per frame. Keeping it a plain struct means
    the view has no idea a plug-in exists and can be exercised on its own. */
struct StarState
{
    float level      = 0.0f;   // wet RMS, 0..1
    float brightness = 0.0f;   // high-frequency content of the tail, 0..1
    float collapse   = 0.0f;   // 0..1
    float mass       = 0.0f;   // 0..1
    float decay      = 0.0f;   // 0..1
    float shimmer    = 0.0f;   // 0..1
    float size       = 0.0f;   // 0..1
    float modRate    = 0.0f;   // Hz
    bool  freeze     = false;
};

class StarView final : public juce::Component,
                       private juce::Timer
{
public:
    StarView();

    void setStateProvider (std::function<StarState()> fn);

    /** Pulls the state and snaps every smoothed quantity to it, with no animation.
        Timers never fire during a headless render, and the first frame the user sees
        should not be a star sweeping up from cold either. */
    void refreshNow();

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    juce::Colour coreColour (float heat) const;

    void paintDisk (juce::Graphics&, juce::Point<float> centre, float radius,
                    juce::Colour tint, float opacity, bool nearHalf) const;
    void paintCorona (juce::Graphics&, juce::Point<float> centre, float radius,
                      juce::Colour tint, float turbulence) const;

    static constexpr int kNumRays = 72;
    static constexpr int kNumMotes = 150;

    struct Mote
    {
        float angle = 0.0f, radius = 1.0f, speed = 0.0f, size = 1.0f, tilt = 0.0f;
    };

    std::function<StarState()> provider;
    StarState state;

    std::array<float, kNumRays> rayLength {};
    std::array<float, kNumRays> rayTarget {};
    std::array<Mote,  kNumMotes> motes {};

    dsp::Xorshift rng { 0x51a2c0deu };

    float spin = 0.0f;
    float pulse = 0.0f;
    float heat = 0.0f;
    float frost = 0.0f;
    float breathe = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StarView)
};

} // namespace dying
