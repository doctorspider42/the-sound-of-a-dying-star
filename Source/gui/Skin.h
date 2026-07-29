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

/*  Palette, type and the handful of drawing primitives every widget shares. Keeping
    them here means the whole panel can be re-tuned by editing a dozen numbers rather
    than hunting through paint methods.                                             */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dying::skin
{

namespace colour
{
    inline const juce::Colour voidTop    { 0xff05060e };
    inline const juce::Colour voidMid    { 0xff0b0819 };
    inline const juce::Colour voidDeep   { 0xff03040a };

    inline const juce::Colour panelFill  { 0x33121a34 };
    inline const juce::Colour panelEdge  { 0x2a8fa8ff };
    inline const juce::Colour panelInner { 0x14ffffff };

    inline const juce::Colour text       { 0xffd8e2ff };
    inline const juce::Colour textDim    { 0xff7b86ad };
    inline const juce::Colour textFaint  { 0xff4d5578 };

    inline const juce::Colour starWhite  { 0xfffff4e4 };
    inline const juce::Colour ice        { 0xff9fe4ff };

    // Group accents. Cool at the top of the signal path, hot at the bottom - the
    // colour tells you roughly what a control will do before you read the label.
    inline const juce::Colour gravity    { 0xff5fc8ff };
    inline const juce::Colour spectrum   { 0xffa98cff };
    inline const juce::Colour drift      { 0xff6ff0c0 };
    inline const juce::Colour collapse   { 0xffff7a4f };
    inline const juce::Colour echo       { 0xffff79c8 };
    inline const juce::Colour master     { 0xffffd9a0 };
}

/** juce::String(const char*) decodes as Latin-1, which turns any UTF-8 glyph in a
    source literal into mojibake. Everything non-ASCII goes through here. */
inline juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::Font labelFont (float height, bool bold = false);
juce::Font displayFont (float height);

/** Panel lettering is always tracked out - JUCE has no tracking parameter, so the
    glyphs are measured and placed one at a time. */
void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                  juce::Rectangle<float> area, juce::Justification justification,
                  juce::Colour c, float tracking);

/** Additive-looking bloom: several passes of the same shape, widening and fading.
    Cheaper and softer than a real blur, and it survives any scaling. */
void glowEllipse (juce::Graphics& g, juce::Point<float> centre, float radius,
                  juce::Colour c, float intensity, int layers = 5);

void glowPath (juce::Graphics& g, const juce::Path& path, juce::Colour c,
               float baseWidth, float intensity, int layers = 4);

/** Section frame: faint fill, one-pixel edge that catches the accent colour along the
    top, and a soft inner shadow so it reads as recessed into the panel. */
void drawSectionFrame (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour accent);

void drawSectionTitle (juce::Graphics& g, const juce::String& title,
                       juce::Rectangle<float> area, juce::Colour accent);

/** A 128x128 monochrome noise tile, generated once. Nothing physical is perfectly
    even, and a few per cent of grain over flat fills is most of what sells it. */
const juce::Image& noiseTile();

} // namespace dying::skin
