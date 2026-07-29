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

/*  Every parameter ID in one place, plus a struct of raw atomic pointers resolved once
    at construction. Reading a parameter on the audio thread is then one relaxed load
    rather than a string lookup through the APVTS.                                  */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace dying
{

namespace pid
{
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* preDelay   = "predelay";
    inline constexpr const char* size       = "size";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* feedback   = "feedback";
    inline constexpr const char* damping    = "damping";
    inline constexpr const char* lowCut     = "lowcut";
    inline constexpr const char* highCut    = "highcut";
    inline constexpr const char* diffusion  = "diffusion";
    inline constexpr const char* shimmer    = "shimmer";
    inline constexpr const char* shimPitch  = "shimpitch";
    inline constexpr const char* detune     = "detune";
    inline constexpr const char* modRate    = "modrate";
    inline constexpr const char* modDepth   = "moddepth";
    inline constexpr const char* collapse   = "collapse";
    inline constexpr const char* mass       = "mass";
    inline constexpr const char* width      = "width";
    inline constexpr const char* output     = "output";
    inline constexpr const char* freeze     = "freeze";
    inline constexpr const char* bypass     = "bypass";

    // The delay section. Added after everything else, so no parameter that existed
    // before it moved index - hosts that remember automation by position keep working.
    inline constexpr const char* delayOn      = "delayon";
    inline constexpr const char* delayTime    = "delaytime";
    inline constexpr const char* delayFeed    = "delayfb";
    inline constexpr const char* delaySpread  = "delayspread";
    inline constexpr const char* delayShimmer = "delayshimmer";
    inline constexpr const char* delayPitch   = "delaypitch";
    inline constexpr const char* delayTone    = "delaytone";
    inline constexpr const char* delayWobble  = "delaywobble";
    inline constexpr const char* delayAbyss   = "delayabyss";
    inline constexpr const char* delayMix     = "delaymix";
    inline constexpr const char* delayMorph   = "delaymorph";
    inline constexpr const char* delayBounce  = "delaybounce";

    // The early field, and the network's own level in the wet path.
    inline constexpr const char* space        = "space";
    inline constexpr const char* reverbLevel  = "reverblevel";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Raw pointers into the APVTS, resolved once. Never null after construction: the
    constructor asserts on every one of them. */
struct ParamPointers
{
    void attach (juce::AudioProcessorValueTreeState& state);

    std::atomic<float>* mix       = nullptr;
    std::atomic<float>* preDelay  = nullptr;
    std::atomic<float>* size      = nullptr;
    std::atomic<float>* decay     = nullptr;
    std::atomic<float>* feedback  = nullptr;
    std::atomic<float>* damping   = nullptr;
    std::atomic<float>* lowCut    = nullptr;
    std::atomic<float>* highCut   = nullptr;
    std::atomic<float>* diffusion = nullptr;
    std::atomic<float>* shimmer   = nullptr;
    std::atomic<float>* shimPitch = nullptr;
    std::atomic<float>* detune    = nullptr;
    std::atomic<float>* modRate   = nullptr;
    std::atomic<float>* modDepth  = nullptr;
    std::atomic<float>* collapse  = nullptr;
    std::atomic<float>* mass      = nullptr;
    std::atomic<float>* width     = nullptr;
    std::atomic<float>* output    = nullptr;
    std::atomic<float>* freeze    = nullptr;
    std::atomic<float>* bypass    = nullptr;

    std::atomic<float>* delayOn      = nullptr;
    std::atomic<float>* delayTime    = nullptr;
    std::atomic<float>* delayFeed    = nullptr;
    std::atomic<float>* delaySpread  = nullptr;
    std::atomic<float>* delayShimmer = nullptr;
    std::atomic<float>* delayPitch   = nullptr;
    std::atomic<float>* delayTone    = nullptr;
    std::atomic<float>* delayWobble  = nullptr;
    std::atomic<float>* delayAbyss   = nullptr;
    std::atomic<float>* delayMix     = nullptr;
    std::atomic<float>* delayMorph   = nullptr;
    std::atomic<float>* delayBounce  = nullptr;

    std::atomic<float>* space        = nullptr;
    std::atomic<float>* reverbLevel  = nullptr;
};

} // namespace dying
