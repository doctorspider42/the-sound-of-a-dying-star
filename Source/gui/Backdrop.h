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

/*  Everything that never moves: the void, the nebulae, the star field, the section
    frames and their lettering. All of it is expensive to draw and none of it changes
    between frames, so it is rendered once into an image at the display's real pixel
    density and blitted after that.

    This matters more than it sounds: the visualiser repaints 30 times a second and is
    not opaque, so without the cache every one of those frames would re-run several
    hundred gradient and path operations underneath it.                             */

#pragma once

#include "Skin.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace dying
{

struct Section
{
    juce::Rectangle<float> bounds;
    juce::String title;
    juce::Colour accent;
};

class Backdrop final : public juce::Component
{
public:
    Backdrop();

    void setSections (std::vector<Section> newSections);
    void paint (juce::Graphics&) override;

private:
    void paintScene (juce::Graphics&) const;

    std::vector<Section> sections;
    juce::Image cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Backdrop)
};

} // namespace dying
