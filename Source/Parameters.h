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
    inline constexpr const char* delayWidth   = "delaywidth";

    // Collapses everything leaving the plug-in to one channel.
    inline constexpr const char* mono         = "mono";

    // The early field, and the network's own level in the wet path.
    inline constexpr const char* space        = "space";
    inline constexpr const char* reverbLevel  = "reverblevel";

    // The delay's spacing taken from the host's tempo instead of its own knob, and
    // which note value it is taken as.
    inline constexpr const char* delaySync    = "delaysync";
    inline constexpr const char* delayDiv     = "delaydiv";

    // Lets the shimmer interval off the semitone grid.
    inline constexpr const char* shimFree     = "shimfree";
}

/** The note values the delay can be locked to.

    Ordered by how long they are rather than by name, because the control is a knob: one
    end has to be the shortest repeat and the other the longest, with everything in
    between passing through in order. */
namespace tempo
{
    struct Division
    {
        const char* label;
        float beats;      // in quarter notes, which is what a BPM counts
    };

    inline constexpr Division kDivisions[] =
    {
        { "1/32T", 1.0f / 12.0f },
        { "1/32",  0.125f       },
        { "1/16T", 1.0f / 6.0f  },
        { "1/32D", 0.1875f      },
        { "1/16",  0.25f        },
        { "1/8T",  1.0f / 3.0f  },
        { "1/16D", 0.375f       },
        { "1/8",   0.5f         },
        { "1/4T",  2.0f / 3.0f  },
        { "1/8D",  0.75f        },
        { "1/4",   1.0f         },
        { "1/2T",  4.0f / 3.0f  },
        { "1/4D",  1.5f         },
        { "1/2",   2.0f         },
        { "1/1T",  8.0f / 3.0f  },
        { "1/2D",  3.0f         },
        { "1/1",   4.0f         },
        { "1/1D",  6.0f         },
    };

    inline constexpr int kNumDivisions = (int) (sizeof (kDivisions) / sizeof (kDivisions[0]));

    /** A quarter note: the one everybody reaches for first. */
    inline constexpr int kDefaultDivision = 10;

    /** What the host reports when it has no tempo to report - and what the standalone
        runs at, since it has no transport of its own. */
    inline constexpr double kFallbackBpm = 120.0;
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
    std::atomic<float>* delayWidth   = nullptr;
    std::atomic<float>* mono         = nullptr;

    std::atomic<float>* space        = nullptr;
    std::atomic<float>* reverbLevel  = nullptr;

    std::atomic<float>* delaySync    = nullptr;
    std::atomic<float>* delayDiv     = nullptr;
    std::atomic<float>* shimFree     = nullptr;
};

} // namespace dying
