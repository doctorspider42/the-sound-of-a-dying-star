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

#include "StarView.h"

namespace dying
{

namespace
{
    constexpr float kFrameRate = 30.0f;
    constexpr float kDt = 1.0f / kFrameRate;

    /** Black-body-ish ramp: a young blue-white star at one end, something that has run
        out of fuel at the other. */
    const std::array<std::pair<float, juce::Colour>, 5> kRamp
    {{
        { 0.00f, juce::Colour (0xffcfe9ff) },
        { 0.28f, juce::Colour (0xfffff2dc) },
        { 0.55f, juce::Colour (0xffffc463) },
        { 0.80f, juce::Colour (0xffff6f33) },
        { 1.00f, juce::Colour (0xffbe2313) },
    }};

    float smoothTo (float current, float target, float rate)
    {
        return current + (target - current) * juce::jlimit (0.0f, 1.0f, rate);
    }
}

StarView::StarView()
{
    setInterceptsMouseClicks (false, false);

    for (int i = 0; i < kNumRays; ++i)
        rayLength[(size_t) i] = rayTarget[(size_t) i] = 0.5f + 0.5f * rng.nextFloat();

    for (auto& m : motes)
    {
        m.angle  = rng.nextFloat() * dsp::kTwoPi;
        m.radius = 0.25f + 0.75f * rng.nextFloat();
        m.speed  = 0.35f + 0.9f * rng.nextFloat();
        m.size   = 0.7f + 1.9f * rng.nextFloat();
        m.tilt   = 0.35f + 0.65f * rng.nextFloat();
    }
}

void StarView::setStateProvider (std::function<StarState()> fn)
{
    provider = std::move (fn);
    refreshNow();
}

void StarView::refreshNow()
{
    if (! provider)
        return;

    state = provider();

    heat = juce::jlimit (0.0f, 1.0f,
                         0.55f * state.collapse + 0.62f * state.mass + 0.18f * state.decay
                             - 0.20f * state.brightness);
    frost = state.freeze ? 1.0f : 0.0f;
    pulse = juce::jlimit (0.0f, 1.0f, state.level);

    repaint();
}

void StarView::visibilityChanged()
{
    if (isVisible())
        startTimerHz ((int) kFrameRate);
    else
        stopTimer();
}

void StarView::timerCallback()
{
    if (provider)
        state = provider();

    // Heat is what turns a star into a black hole here: drive in the loop plus the
    // sub-octave mass, with a long decay pushing it a little further.
    const auto targetHeat = juce::jlimit (0.0f, 1.0f,
                                          0.55f * state.collapse + 0.62f * state.mass
                                              + 0.18f * state.decay
                                              - 0.20f * state.brightness);

    heat   = smoothTo (heat, targetHeat, 0.06f);
    frost  = smoothTo (frost, state.freeze ? 1.0f : 0.0f, 0.08f);
    pulse  = smoothTo (pulse, juce::jlimit (0.0f, 1.0f, state.level), state.level > pulse ? 0.35f : 0.06f);

    breathe += kDt * (0.09f + 0.25f * state.modRate);
    if (breathe > dsp::kTwoPi) breathe -= dsp::kTwoPi;

    // The disk slows to a stop when frozen - that is the whole point of the control.
    const auto spinRate = (0.10f + 0.55f * state.modRate + 0.25f * heat) * (1.0f - 0.97f * frost);
    spin += kDt * spinRate;
    if (spin > dsp::kTwoPi) spin -= dsp::kTwoPi;

    // Corona rays chase new random targets; higher collapse makes the chase quicker
    // and the targets wilder, which reads as turbulence.
    const auto churn = 0.05f + 0.30f * heat;

    for (int i = 0; i < kNumRays; ++i)
    {
        const auto idx = (size_t) i;

        if (rng.nextFloat() < churn)
            rayTarget[idx] = 0.35f + (0.45f + 0.75f * heat) * rng.nextFloat()
                                 + 0.35f * pulse;

        rayLength[idx] = smoothTo (rayLength[idx], rayTarget[idx], 0.10f + 0.20f * heat);
    }

    const auto moteSpeed = (0.12f + 0.5f * state.modRate + 0.35f * heat) * (1.0f - 0.98f * frost);

    for (auto& m : motes)
    {
        m.angle += kDt * moteSpeed * (0.6f + m.speed) / (0.25f + m.radius);
        m.radius -= kDt * moteSpeed * 0.055f * m.speed;

        if (m.radius < 0.12f)
        {
            m.radius = 1.0f + 0.25f * rng.nextFloat();
            m.angle  = rng.nextFloat() * dsp::kTwoPi;
            m.size   = 0.6f + 1.6f * rng.nextFloat();
        }
    }

    repaint();
}

juce::Colour StarView::coreColour (float h) const
{
    h = juce::jlimit (0.0f, 1.0f, h);

    for (size_t i = 1; i < kRamp.size(); ++i)
    {
        if (h <= kRamp[i].first)
        {
            const auto span = kRamp[i].first - kRamp[i - 1].first;
            const auto t = span > 0.0f ? (h - kRamp[i - 1].first) / span : 0.0f;
            const auto c = kRamp[i - 1].second.interpolatedWith (kRamp[i].second, t);
            return c.interpolatedWith (skin::colour::ice, frost * 0.8f);
        }
    }

    return kRamp.back().second;
}

void StarView::paintDisk (juce::Graphics& g, juce::Point<float> centre, float radius,
                          juce::Colour tint, float opacity, bool nearHalf) const
{
    constexpr int kSegments = 128;
    constexpr float kSquash = 0.26f;

    const auto rInner = radius * 1.35f;
    const auto rOuter = radius * 2.65f;

    for (int i = 0; i < kSegments; ++i)
    {
        const auto a = dsp::kTwoPi * (float) i / (float) kSegments;
        const auto onNearSide = std::sin (a) > 0.0f;

        if (onNearSide != nearHalf)
            continue;

        // Doppler-ish: the side turning toward us is brighter. Pure theatre, but it
        // is what stops the ring reading as a flat circle.
        const auto facing = 0.5f + 0.5f * std::cos (a - spin * 2.0f);
        const auto width = rOuter - rInner;
        const auto r = rInner + width * (0.5f + 0.42f * std::sin (a * 3.0f + spin * 1.7f));

        const auto x = centre.x + std::cos (a) * r;
        const auto y = centre.y + std::sin (a) * r * kSquash;

        const auto alpha = opacity * (0.18f + 0.82f * facing * facing);
        const auto dotSize = radius * (0.045f + 0.05f * facing);

        g.setColour (tint.withAlpha (juce::jlimit (0.0f, 1.0f, alpha * 0.55f)));
        g.fillEllipse (x - dotSize, y - dotSize * 0.9f, dotSize * 2.0f, dotSize * 1.8f);
    }

    // A continuous thin ring underneath the grains ties them together.
    juce::Path ring;
    const auto rMid = (rInner + rOuter) * 0.5f;
    ring.addCentredArc (centre.x, centre.y, rMid, rMid * kSquash, 0.0f,
                        nearHalf ? 0.0f : dsp::kPi, nearHalf ? dsp::kPi : dsp::kTwoPi, true);
    g.setColour (tint.withAlpha (opacity * 0.22f));
    g.strokePath (ring, juce::PathStrokeType (radius * 0.10f));
}

void StarView::paintCorona (juce::Graphics& g, juce::Point<float> centre, float radius,
                            juce::Colour tint, float turbulence) const
{
    // A closed contour through the ray tips. Spokes on their own read as a bicycle
    // wheel; filling the envelope they describe reads as plasma.
    auto envelope = [this, centre, radius, turbulence] (float extent)
    {
        juce::Path p;

        for (int i = 0; i <= kNumRays; ++i)
        {
            const auto idx = (size_t) (i % kNumRays);
            const auto a = dsp::kTwoPi * (float) i / (float) kNumRays;
            const auto len = radius * (1.0f + rayLength[idx] * extent
                                                  * (0.45f + 0.85f * turbulence));
            const auto pt = centre.translated (std::cos (a) * len, std::sin (a) * len);

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        p.closeSubPath();
        return p;
    };

    for (int layer = 0; layer < 2; ++layer)
    {
        const auto extent = layer == 0 ? 1.0f : 0.48f;
        const auto reach = radius * (1.0f + extent * (0.45f + 0.85f * turbulence));

        juce::ColourGradient grad (tint.withAlpha ((layer == 0 ? 0.09f : 0.17f)
                                                       + 0.07f * pulse),
                                   centre.x, centre.y,
                                   tint.withAlpha (0.0f), centre.x + reach, centre.y, true);
        grad.addColour (0.55, tint.withAlpha ((layer == 0 ? 0.05f : 0.11f) + 0.05f * pulse));
        g.setGradientFill (grad);
        g.fillPath (envelope (extent));
    }

    // Spokes, kept faint - just enough structure to catch the eye moving.
    juce::Path spokes;

    for (int i = 0; i < kNumRays; ++i)
    {
        const auto a = dsp::kTwoPi * (float) i / (float) kNumRays;
        const auto len = radius * (1.0f + rayLength[(size_t) i] * (0.55f + 0.9f * turbulence));
        const auto inner = radius * 0.94f;

        spokes.startNewSubPath (centre.x + std::cos (a) * inner,
                                centre.y + std::sin (a) * inner);
        spokes.lineTo (centre.x + std::cos (a) * len,
                       centre.y + std::sin (a) * len);
    }

    g.setColour (tint.withAlpha (0.05f + 0.06f * pulse));
    g.strokePath (spokes, juce::PathStrokeType (radius * 0.022f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void StarView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre().translated (0.0f, -bounds.getHeight() * 0.02f);

    const auto unit = juce::jmin (bounds.getWidth() * 0.5f, bounds.getHeight() * 0.5f);
    const auto breatheAmount = 1.0f + 0.035f * std::sin (breathe);
    const auto radius = unit * (0.30f + 0.14f * state.size + 0.06f * pulse)
                            * breatheAmount * (1.0f - 0.22f * heat);

    const auto tint = coreColour (heat);
    const auto haloTint = tint.interpolatedWith (juce::Colour (0xffb060ff), 0.35f * (1.0f - heat));

    // ---- orbital guides -----------------------------------------------------
    // Three barely-there ellipses. They cost nothing and they are what stops the panel
    // reading as a ball floating in an empty rectangle.
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto scale = 1.9f + 1.35f * (float) i;
            const auto rx = radius * scale;
            const auto ry = rx * (0.20f + 0.05f * (float) i);

            juce::Path orbit;
            orbit.addEllipse (juce::Rectangle<float> (rx * 2.0f, ry * 2.0f).withCentre (centre));
            orbit.applyTransform (juce::AffineTransform::rotation (-0.16f + 0.09f * (float) i,
                                                                   centre.x, centre.y));
            g.setColour (haloTint.withAlpha (0.09f - 0.02f * (float) i));
            g.strokePath (orbit, juce::PathStrokeType (1.0f));
        }
    }

    // ---- outer halo ---------------------------------------------------------
    {
        const auto haloRadius = radius * (4.2f + 1.6f * state.decay + 0.9f * pulse);
        juce::ColourGradient halo (haloTint.withAlpha (0.20f + 0.22f * pulse),
                                   centre.x, centre.y,
                                   haloTint.withAlpha (0.0f),
                                   centre.x + haloRadius, centre.y, true);
        halo.addColour (0.35, haloTint.withAlpha (0.055f + 0.06f * pulse));
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (haloRadius * 2.0f, haloRadius * 2.0f)
                           .withCentre (centre));
    }

    // ---- far half of the accretion disk -------------------------------------
    paintDisk (g, centre, radius, tint.interpolatedWith (juce::Colours::white, 0.15f),
               0.40f + 0.50f * state.mass + 0.22f * heat, false);

    // ---- corona -------------------------------------------------------------
    paintCorona (g, centre, radius, tint, heat);

    // ---- the star itself ----------------------------------------------------
    {
        const auto glowStrength = 0.55f + 0.45f * pulse;
        skin::glowEllipse (g, centre, radius * 0.95f, tint, glowStrength, 6);

        juce::ColourGradient core (juce::Colours::white.withAlpha (0.98f - 0.35f * heat),
                                   centre.x - radius * 0.18f, centre.y - radius * 0.22f,
                                   tint.withAlpha (0.0f),
                                   centre.x + radius, centre.y + radius, true);
        core.addColour (0.42, tint.withAlpha (0.95f));
        core.addColour (0.80, tint.withAlpha (0.45f));
        g.setGradientFill (core);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    }

    // ---- event horizon ------------------------------------------------------
    // Past a certain amount of collapse the middle stops emitting: a dark disk with a
    // bright rim where the light that did escape is piling up.
    if (heat > 0.55f)
    {
        const auto t = juce::jlimit (0.0f, 1.0f, (heat - 0.55f) / 0.45f);
        const auto holeRadius = radius * 0.85f * t;

        juce::ColourGradient hole (juce::Colour (0xff000000).withAlpha (0.97f),
                                   centre.x, centre.y,
                                   juce::Colour (0xff000000).withAlpha (0.0f),
                                   centre.x + holeRadius * 1.25f, centre.y, true);
        hole.addColour (0.72, juce::Colour (0xff000000).withAlpha (0.92f));
        g.setGradientFill (hole);
        g.fillEllipse (juce::Rectangle<float> (holeRadius * 2.5f, holeRadius * 2.5f)
                           .withCentre (centre));

        juce::Path rim;
        rim.addEllipse (juce::Rectangle<float> (holeRadius * 2.0f, holeRadius * 2.0f)
                            .withCentre (centre));
        skin::glowPath (g, rim, tint.brighter (0.6f), 1.6f, 0.75f * t, 3);
    }

    // ---- near half of the disk, in front of the star ------------------------
    paintDisk (g, centre, radius, tint.interpolatedWith (juce::Colours::white, 0.25f),
               0.50f + 0.50f * state.mass + 0.22f * heat, true);

    // ---- infalling dust -----------------------------------------------------
    // Separate x and y radii rather than one radius and a squash: the cloud then fills
    // the panel it is actually in instead of being culled at the top and bottom.
    {
        const auto rx = bounds.getWidth() * 0.47f;
        const auto ry = bounds.getHeight() * 0.46f;
        const auto moteTint = tint.interpolatedWith (skin::colour::starWhite, 0.5f);

        for (const auto& m : motes)
        {
            const auto x = centre.x + std::cos (m.angle) * m.radius * rx;
            const auto y = centre.y + std::sin (m.angle) * m.radius * ry * m.tilt;

            if (! bounds.contains (x, y))
                continue;

            const auto fade = juce::jlimit (0.0f, 1.0f, (1.15f - m.radius) * 0.9f);
            const auto s = m.size * (0.7f + 0.6f * fade);
            const auto alpha = 0.14f + 0.6f * fade * (0.45f + 0.55f * pulse);

            // A short trail along the orbit. One extra line per mote, and it turns a
            // field of dots into something that is visibly falling inward.
            const auto trail = 0.030f + 0.055f * (1.0f - m.radius);
            const auto px = centre.x + std::cos (m.angle - trail) * (m.radius + 0.012f) * rx;
            const auto py = centre.y + std::sin (m.angle - trail) * (m.radius + 0.012f) * ry * m.tilt;

            g.setColour (moteTint.withAlpha (alpha * 0.45f));
            g.drawLine (px, py, x, y, juce::jmax (0.6f, s * 0.45f));

            g.setColour (moteTint.withAlpha (alpha));
            g.fillEllipse (x - s * 0.5f, y - s * 0.5f, s, s);
        }
    }

    // ---- shimmer halo -------------------------------------------------------
    // Rising rings, because that is what the shimmer path is doing to the spectrum.
    if (state.shimmer > 0.02f)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto phase = std::fmod (breathe * 0.45f + (float) i * 0.3333f, 1.0f);
            const auto r = radius * (1.4f + phase * 3.4f);
            const auto a = state.shimmer * (1.0f - phase) * (1.0f - phase) * 0.35f;

            juce::Path ring;
            ring.addEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
            g.setColour (skin::colour::ice.interpolatedWith (tint, 0.4f)
                             .withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.strokePath (ring, juce::PathStrokeType (1.2f));
        }
    }

    // ---- frozen ------------------------------------------------------------
    if (frost > 0.01f)
    {
        const auto r = radius * 1.9f;
        juce::Path crystal;

        for (int i = 0; i < 6; ++i)
        {
            const auto a = dsp::kTwoPi * (float) i / 6.0f + spin;
            const auto p = centre.translated (std::cos (a) * r, std::sin (a) * r * 0.85f);

            if (i == 0) crystal.startNewSubPath (p);
            else        crystal.lineTo (p);
        }

        crystal.closeSubPath();
        skin::glowPath (g, crystal, skin::colour::ice, 1.4f, 0.55f * frost, 3);
    }
}

} // namespace dying
