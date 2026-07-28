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

#include "Skin.h"

#include "../dsp/Utils.h"

namespace dying::skin
{

juce::Font labelFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain))
               .withHorizontalScale (0.94f);
}

juce::Font displayFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height,
                                          juce::Font::plain))
               .withHorizontalScale (0.90f);
}

void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                  juce::Rectangle<float> area, juce::Justification justification,
                  juce::Colour c, float tracking)
{
    if (text.isEmpty())
        return;

    juce::Array<float> widths;
    auto total = 0.0f;

    for (int i = 0; i < text.length(); ++i)
    {
        const auto w = juce::GlyphArrangement::getStringWidth (font, text.substring (i, i + 1));
        widths.add (w);
        total += w + tracking;
    }

    total -= tracking;

    auto x = area.getX();

    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - total * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = area.getRight() - total;

    const auto baseline = area.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.35f;

    g.setFont (font);
    g.setColour (c);

    for (int i = 0; i < text.length(); ++i)
    {
        g.drawSingleLineText (text.substring (i, i + 1), juce::roundToInt (x),
                              juce::roundToInt (baseline));
        x += widths[i] + tracking;
    }
}

void glowEllipse (juce::Graphics& g, juce::Point<float> centre, float radius,
                  juce::Colour c, float intensity, int layers)
{
    for (int i = layers; i >= 1; --i)
    {
        const auto t = (float) i / (float) layers;
        const auto r = radius * (1.0f + t * 1.9f);
        const auto a = intensity * (1.0f - t) * (1.0f - t) * 0.55f;

        if (a <= 0.002f)
            continue;

        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }
}

void glowPath (juce::Graphics& g, const juce::Path& path, juce::Colour c,
               float baseWidth, float intensity, int layers)
{
    for (int i = layers; i >= 1; --i)
    {
        const auto t = (float) i / (float) layers;
        const auto w = baseWidth * (1.0f + t * 3.2f);
        const auto a = intensity * (1.0f - t) * 0.5f;

        if (a <= 0.002f)
            continue;

        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        g.strokePath (path, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, intensity)));
    g.strokePath (path, juce::PathStrokeType (baseWidth, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void drawSectionFrame (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour accent)
{
    constexpr float corner = 10.0f;

    juce::ColourGradient fill (juce::Colour (0x2a141d3a), area.getX(), area.getY(),
                               juce::Colour (0x140a0f22), area.getX(), area.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (area, corner);

    // Grain, so the fill is not a flat digital wash.
    g.setTiledImageFill (noiseTile(), 0, 0, 0.030f);
    g.fillRoundedRectangle (area, corner);

    // The accent only touches the top edge: light falls from above everywhere on this
    // panel, and the frames have to agree with the star.
    juce::ColourGradient edge (accent.withAlpha (0.42f), area.getCentreX(), area.getY(),
                               accent.withAlpha (0.06f), area.getCentreX(), area.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (area, corner, 1.2f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (area.reduced (1.4f), corner - 1.4f, 1.0f);
}

void drawSectionTitle (juce::Graphics& g, const juce::String& title,
                       juce::Rectangle<float> area, juce::Colour accent)
{
    const auto font = labelFont (10.5f, true);

    // Shadow one pixel down, then the ink: reads as cut into the surface.
    auto shadowArea = area.translated (0.0f, 1.0f);
    drawTracked (g, title, font, shadowArea, juce::Justification::centredLeft,
                 juce::Colours::black.withAlpha (0.55f), 2.6f);
    drawTracked (g, title, font, area, juce::Justification::centredLeft,
                 accent.withAlpha (0.92f), 2.6f);

    const auto textWidth = juce::GlyphArrangement::getStringWidth (font, title)
                             + 2.6f * (float) title.length();
    auto rule = area.withTrimmedLeft (textWidth + 10.0f).withHeight (1.0f)
                    .withY (area.getCentreY());

    if (rule.getWidth() > 4.0f)
    {
        juce::ColourGradient grad (accent.withAlpha (0.30f), rule.getX(), 0.0f,
                                   accent.withAlpha (0.0f), rule.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        g.fillRect (rule);
    }
}

const juce::Image& noiseTile()
{
    static const juce::Image tile = []
    {
        juce::Image image (juce::Image::ARGB, 128, 128, true);
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
        dsp::Xorshift rng (0x1a2b3c4du);

        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
            {
                const auto v = (juce::uint8) (110 + (int) (rng.nextFloat() * 90.0f));
                data.setPixelColour (x, y, juce::Colour (v, v, (juce::uint8) (v + 6)));
            }

        return image;
    }();

    return tile;
}

} // namespace dying::skin
