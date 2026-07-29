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

/*  The application mark: a collapsing star inside its accretion disk.

    Drawn rather than authored as a bitmap, and every dimension in it is a fraction of
    the square it is given, so the same code produces the 1024 px icon a platform wants
    for a dock and the 32 px one it wants for a list - each rendered at its own size
    instead of scaled down from one master, which is what keeps the disk a hairline
    rather than a smear. The palette is the panel's, so the icon and the plug-in behind
    it are visibly the same object.                                                   */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dying::icon
{

/** Draws the mark into the largest square that fits `area`, centred. */
void paint (juce::Graphics& g, juce::Rectangle<float> area);

/** The mark as a square ARGB image with transparent corners - what the build writes
    out as the PNG that CMake hands to JUCE. */
juce::Image render (int sizePixels);

} // namespace dying::icon
