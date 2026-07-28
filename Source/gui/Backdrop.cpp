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

#include "Backdrop.h"

#include "../dsp/Utils.h"

namespace dying
{

Backdrop::Backdrop()
{
    // Opaque, so JUCE never bothers painting anything underneath - but that promise
    // means paint() has to cover every pixel of the bounds, corners included.
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
}

void Backdrop::setSections (std::vector<Section> newSections)
{
    sections = std::move (newSections);
    cache = {};
    repaint();
}

void Backdrop::paintScene (juce::Graphics& g) const
{
    const auto bounds = getLocalBounds().toFloat();

    // ---- the void -----------------------------------------------------------
    juce::ColourGradient space (skin::colour::voidTop, bounds.getCentreX(), 0.0f,
                                skin::colour::voidDeep, bounds.getCentreX(), bounds.getBottom(),
                                false);
    space.addColour (0.42, skin::colour::voidMid);
    g.setGradientFill (space);
    g.fillRect (bounds);

    // ---- nebulae ------------------------------------------------------------
    struct Cloud { float x, y, r; juce::Colour c; float a; };

    const Cloud clouds[] =
    {
        { 0.20f, 0.30f, 0.55f, juce::Colour (0xff7a3ac8), 0.16f },
        { 0.78f, 0.22f, 0.48f, juce::Colour (0xff2f6fd0), 0.14f },
        { 0.52f, 0.62f, 0.70f, juce::Colour (0xffc0357f), 0.09f },
        { 0.10f, 0.82f, 0.40f, juce::Colour (0xff1f8ea0), 0.10f },
        { 0.90f, 0.75f, 0.42f, juce::Colour (0xff6a34a8), 0.11f },
    };

    for (const auto& c : clouds)
    {
        const auto cx = bounds.getWidth() * c.x;
        const auto cy = bounds.getHeight() * c.y;
        const auto r  = bounds.getWidth() * c.r;

        juce::ColourGradient cloud (c.c.withAlpha (c.a), cx, cy,
                                    c.c.withAlpha (0.0f), cx + r, cy, true);
        cloud.addColour (0.45, c.c.withAlpha (c.a * 0.42f));
        g.setGradientFill (cloud);
        g.fillEllipse (cx - r, cy - r * 0.85f, r * 2.0f, r * 1.7f);
    }

    // ---- star field ---------------------------------------------------------
    // Deterministic, so the sky is the same every time the editor opens. A user who
    // notices the constellations moving between sessions will find it distracting.
    {
        dsp::Xorshift rng (0xc0ffee11u);

        for (int i = 0; i < 620; ++i)
        {
            const auto x = rng.nextFloat() * bounds.getWidth();
            const auto y = rng.nextFloat() * bounds.getHeight();
            const auto m = rng.nextFloat();
            const auto size = 0.5f + m * m * m * 2.6f;
            const auto alpha = 0.10f + m * m * 0.72f;

            // A few per cent of stars are not white. Real skies are not either.
            const auto tint = rng.nextFloat();
            const auto c = tint > 0.93f ? juce::Colour (0xffffd0a0)
                         : tint > 0.86f ? juce::Colour (0xffa8c8ff)
                                        : juce::Colour (0xfff2f6ff);

            g.setColour (c.withAlpha (alpha));
            g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);

            if (size > 2.3f)
                skin::glowEllipse (g, { x, y }, size * 0.55f, c, 0.30f, 3);
        }
    }

    // ---- vignette -----------------------------------------------------------
    {
        const auto r = juce::jmax (bounds.getWidth(), bounds.getHeight()) * 0.78f;
        juce::ColourGradient v (juce::Colours::transparentBlack,
                                bounds.getCentreX(), bounds.getCentreY(),
                                juce::Colours::black.withAlpha (0.72f),
                                bounds.getCentreX() + r, bounds.getCentreY(), true);
        v.addColour (0.55, juce::Colours::black.withAlpha (0.06f));
        g.setGradientFill (v);
        g.fillRect (bounds);
    }

    // ---- section frames -----------------------------------------------------
    for (const auto& s : sections)
    {
        skin::drawSectionFrame (g, s.bounds, s.accent);

        if (s.title.isNotEmpty())
        {
            auto title = s.bounds.withHeight (22.0f).translated (0.0f, 7.0f).reduced (14.0f, 0.0f);
            skin::drawSectionTitle (g, s.title, title, s.accent);
        }
    }
}

void Backdrop::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // setBufferedToImage would cache at logical size and go soft the moment the editor
    // is scaled up, so the cache is built at the real device resolution instead.
    const auto scale = juce::jlimit (1.0f, 3.0f,
                                     (float) g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale));
    const auto h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    if (cache.isNull() || cache.getWidth() != w || cache.getHeight() != h)
    {
        cache = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics ig (cache);
        ig.addTransform (juce::AffineTransform::scale (scale));
        paintScene (ig);
    }

    g.drawImage (cache, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

} // namespace dying
