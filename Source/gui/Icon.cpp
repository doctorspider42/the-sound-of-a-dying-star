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

#include "Icon.h"

#include "Skin.h"
#include "../dsp/Utils.h"

namespace dying::icon
{

namespace
{
    // Every number here is a fraction of the square's side.
    constexpr float kCorner     = 0.222f;   // close to what the platform masks expect
    constexpr float kCore       = 0.132f;
    constexpr float kDisk       = 0.400f;
    constexpr float kSquash     = 0.270f;
    constexpr float kTilt       = -0.19f;   // radians, so the disk is not a flat line
    constexpr float kCentreLift = 0.020f;   // the star sits a shade above centre

    // The star's own ramp, taken from the panel's: white heat in the middle, an
    // exhausted red at the edge of what is left.
    const juce::Colour kWhite { 0xfffff8ea };
    const juce::Colour kAmber { 0xffffc463 };
    const juce::Colour kEmber { 0xffff6f33 };
    const juce::Colour kAsh   { 0xffbe2313 };

    /** The disk itself: a wash of matter in the orbital plane, under the ring. Without
        it the ring is a hoop the star happens to sit inside, which is a planet. */
    void paintDiskPlane (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour tint)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::translation (-c.x, -c.y)
                            .scaled (1.0f, kSquash)
                            .rotated (kTilt)
                            .translated (c.x, c.y));

        juce::ColourGradient plane (tint.withAlpha (0.0f), c.x, c.y,
                                    tint.withAlpha (0.0f), c.x + r, c.y, true);
        plane.addColour (0.40, tint.withAlpha (0.035f));
        plane.addColour (0.72, tint.withAlpha (0.13f));
        plane.addColour (0.93, tint.withAlpha (0.07f));
        g.setGradientFill (plane);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    }

    /** One half of the ring, fading out at both ends - where it passes the star's
        silhouette and is either behind it or coming round the front of it. The taper is
        a horizontal gradient over the stroke's outline rather than a run of segments
        with falling alpha: the ends of the arc are its leftmost and rightmost points, so
        one gradient does it, and every alternative that draws the arc piecewise leaves
        the seams visible as beads or a comb.

        The near half is drawn after the star and the far half before it, which is the
        whole reason the ring reads as going around something. */
    void paintDiskHalf (juce::Graphics& g, juce::Point<float> c, float rx, float ry,
                        bool nearHalf, juce::Colour tint, float weight, float width)
    {
        // Angles run clockwise from twelve o'clock, so the near half is the one that
        // passes through six.
        constexpr auto quarter = dsp::kPi * 0.5f;

        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rx, ry, kTilt,
                           nearHalf ? quarter : -quarter,
                           nearHalf ? quarter * 3.0f : quarter,
                           true);

        const auto reach = rx * std::cos (kTilt);

        for (int pass = 3; pass >= 0; --pass)
        {
            const auto strength = pass == 0 ? weight : weight * 0.11f / (float) pass;

            juce::Path outline;
            juce::PathStrokeType (width * (1.0f + 2.4f * (float) pass),
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded).createStrokedPath (outline, arc);

            juce::ColourGradient taper (tint.withAlpha (0.0f), c.x - reach, c.y,
                                        tint.withAlpha (0.0f), c.x + reach, c.y, false);
            taper.addColour (0.13, tint.withAlpha (strength * 0.32f));
            taper.addColour (0.50, tint.withAlpha (strength));
            taper.addColour (0.87, tint.withAlpha (strength * 0.32f));
            g.setGradientFill (taper);
            g.fillPath (outline);
        }
    }

    /** A four-pointed flare: one lozenge through the centre each way, the horizontal one
        longer. Cheap, and it is what makes a bright blob read as a star at the sizes
        where nothing else survives. */
    void paintFlare (juce::Graphics& g, juce::Point<float> c, float u, juce::Colour tint)
    {
        struct Spike { float length, halfWidth, angle, alpha; };

        const Spike spikes[] =
        {
            { 0.395f, 0.020f, 0.0f,            0.80f },
            { 0.290f, 0.016f, dsp::kPi * 0.5f, 0.62f },
        };

        for (const auto& s : spikes)
        {
            const auto len = u * s.length;
            const auto w   = u * s.halfWidth;

            juce::Path lozenge;
            lozenge.startNewSubPath (c.x - len, c.y);
            lozenge.lineTo (c.x, c.y - w);
            lozenge.lineTo (c.x + len, c.y);
            lozenge.lineTo (c.x, c.y + w);
            lozenge.closeSubPath();
            lozenge.applyTransform (juce::AffineTransform::rotation (s.angle, c.x, c.y));

            // Radial rather than flat, and steep: a spike that fades linearly reaches
            // the edge of the mark still faintly visible and reads as a scratch.
            juce::ColourGradient fade (tint.withAlpha (s.alpha), c.x, c.y,
                                       tint.withAlpha (0.0f), c.x + len, c.y, true);
            fade.addColour (0.24, tint.withAlpha (s.alpha * 0.40f));
            fade.addColour (0.52, tint.withAlpha (s.alpha * 0.10f));
            fade.addColour (0.76, tint.withAlpha (s.alpha * 0.02f));
            g.setGradientFill (fade);
            g.fillPath (lozenge);
        }
    }

    void paintPlate (juce::Graphics& g, juce::Rectangle<float> square, float u,
                     juce::Point<float> c)
    {
        // ---- the void ------------------------------------------------------
        juce::ColourGradient space (skin::colour::voidMid, c.x, c.y - u * 0.06f,
                                    skin::colour::voidDeep, c.x, c.y + u * 0.78f, true);
        space.addColour (0.45, juce::Colour (0xff070814));
        g.setGradientFill (space);
        g.fillRect (square);

        // ---- nebulae -------------------------------------------------------
        struct Cloud { float x, y, r; juce::Colour c; float a; };

        const Cloud clouds[] =
        {
            { 0.18f, 0.16f, 0.62f, juce::Colour (0xff7a3ac8), 0.30f },
            { 0.86f, 0.30f, 0.54f, juce::Colour (0xff2f6fd0), 0.22f },
            { 0.62f, 0.92f, 0.66f, juce::Colour (0xffc0357f), 0.18f },
            { 0.06f, 0.86f, 0.46f, juce::Colour (0xff1f8ea0), 0.16f },
        };

        for (const auto& cloud : clouds)
        {
            const auto cx = square.getX() + u * cloud.x;
            const auto cy = square.getY() + u * cloud.y;
            const auto r  = u * cloud.r;

            juce::ColourGradient grad (cloud.c.withAlpha (cloud.a), cx, cy,
                                       cloud.c.withAlpha (0.0f), cx + r, cy, true);
            grad.addColour (0.42, cloud.c.withAlpha (cloud.a * 0.40f));
            g.setGradientFill (grad);
            g.fillEllipse (cx - r, cy - r * 0.86f, r * 2.0f, r * 1.72f);
        }

        // ---- star field ----------------------------------------------------
        // Deterministic: the icon has to come out of every build byte for byte, or it
        // shows up as a diff on any machine that regenerates it.
        {
            dsp::Xorshift rng (0x57a12b0du);

            for (int i = 0; i < 140; ++i)
            {
                const auto x = square.getX() + rng.nextFloat() * u;
                const auto y = square.getY() + rng.nextFloat() * u;
                const auto m = rng.nextFloat();

                // Nothing inside the halo: the middle of the mark belongs to the star.
                if (juce::Point<float> (x, y).getDistanceFrom (c) < u * 0.30f)
                    continue;

                const auto size = u * (0.0028f + m * m * m * 0.0125f);
                const auto tint = rng.nextFloat();
                const auto col = tint > 0.92f ? juce::Colour (0xffffd0a0)
                               : tint > 0.84f ? juce::Colour (0xffa8c8ff)
                                              : skin::colour::starWhite;

                g.setColour (col.withAlpha (0.14f + m * m * 0.70f));
                g.fillEllipse (x - size, y - size, size * 2.0f, size * 2.0f);

                if (m > 0.94f)
                    skin::glowEllipse (g, { x, y }, size * 1.1f, col, 0.35f, 3);
            }
        }

        // ---- vignette ------------------------------------------------------
        // Darkens the corners, which is most of what keeps the mark's weight in the
        // middle once a platform has rounded it off.
        {
            const auto r = u * 0.76f;
            juce::ColourGradient v (juce::Colours::transparentBlack, c.x, c.y,
                                    juce::Colours::black.withAlpha (0.70f), c.x + r, c.y, true);
            v.addColour (0.52, juce::Colours::black.withAlpha (0.05f));
            g.setGradientFill (v);
            g.fillRect (square);
        }
    }

    /** Grains riding the near edge of the disk. Two or three pixels each at icon
        sizes, and they are what stops the ring looking like a drawn ellipse. */
    void paintGrains (juce::Graphics& g, juce::Point<float> c, float u, juce::Colour tint)
    {
        dsp::Xorshift rng (0x2f8b1147u);
        const auto rx = u * kDisk;
        const auto ry = rx * kSquash;

        for (int i = 0; i < 16; ++i)
        {
            // Along the front of the disk, and only there.
            const auto a = dsp::kPi * (0.10f + 0.80f * rng.nextFloat());
            const auto spread = 0.86f + 0.30f * rng.nextFloat();

            auto p = juce::Point<float> (c.x + std::cos (a) * rx * spread,
                                         c.y + std::sin (a) * ry * spread);
            p = p.transformedBy (juce::AffineTransform::rotation (kTilt, c.x, c.y));

            const auto size = u * (0.0022f + 0.0050f * rng.nextFloat());
            const auto alpha = 0.22f + 0.45f * rng.nextFloat();

            g.setColour (tint.withAlpha (alpha));
            g.fillEllipse (p.x - size, p.y - size, size * 2.0f, size * 2.0f);
        }
    }
}

void paint (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto u = juce::jmin (area.getWidth(), area.getHeight());

    if (u <= 0.0f)
        return;

    const auto square = juce::Rectangle<float> (u, u).withCentre (area.getCentre());
    const auto centre = square.getCentre().translated (0.0f, -u * kCentreLift);

    juce::Path plate;
    plate.addRoundedRectangle (square, u * kCorner);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (plate);

    paintPlate (g, square, u, centre);

    const auto radius = u * kCore;
    const auto diskRx = u * kDisk;
    const auto diskRy = diskRx * kSquash;

    // ---- halo ---------------------------------------------------------------
    // Amber pulled toward violet: the star's own light, and the gas it is lighting up.
    {
        const auto haloTint = kEmber.interpolatedWith (juce::Colour (0xffb060ff), 0.34f);
        const auto r = u * 0.50f;

        juce::ColourGradient halo (haloTint.withAlpha (0.34f), centre.x, centre.y,
                                   haloTint.withAlpha (0.0f), centre.x + r, centre.y, true);
        halo.addColour (0.22, haloTint.withAlpha (0.20f));
        halo.addColour (0.46, haloTint.withAlpha (0.09f));
        halo.addColour (0.72, haloTint.withAlpha (0.025f));
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
    }

    // ---- the disk, behind the star ------------------------------------------
    {
        paintDiskPlane (g, centre, diskRx * 1.04f,
                        kAmber.interpolatedWith (skin::colour::spectrum, 0.30f));

        paintDiskHalf (g, centre, diskRx, diskRy, false,
                       kEmber.interpolatedWith (skin::colour::spectrum, 0.55f),
                       0.68f, u * 0.009f);
    }

    // ---- flare --------------------------------------------------------------
    paintFlare (g, centre, u, kWhite);

    // ---- the star -----------------------------------------------------------
    {
        // The envelope of a star that has swollen and cooled: white at the surface,
        // through amber, to an exhausted red where it gives out. One gradient with a
        // handful of stops rather than the panel's layered glow - the layers are a few
        // pixels apart on a knob and a hundred apart here, where they band visibly.
        {
            const auto reach = radius * 3.6f;

            juce::ColourGradient envelope (kWhite.withAlpha (0.92f), centre.x, centre.y,
                                           kAsh.withAlpha (0.0f),
                                           centre.x + reach, centre.y, true);
            envelope.addColour (0.20, kWhite.withAlpha (0.80f));
            envelope.addColour (0.32, kAmber.withAlpha (0.62f));
            envelope.addColour (0.46, kEmber.withAlpha (0.42f));
            envelope.addColour (0.62, kAsh.withAlpha (0.26f));
            envelope.addColour (0.80, kAsh.withAlpha (0.10f));
            g.setGradientFill (envelope);
            g.fillEllipse (juce::Rectangle<float> (reach * 2.0f, reach * 2.0f)
                               .withCentre (centre));
        }

        // Off-centre highlight, so the core is a sphere rather than a disk.
        juce::ColourGradient core (juce::Colours::white,
                                   centre.x - radius * 0.22f, centre.y - radius * 0.26f,
                                   kAsh.withAlpha (0.0f),
                                   centre.x + radius * 1.15f, centre.y + radius * 1.15f, true);
        core.addColour (0.34, kWhite);
        core.addColour (0.62, kAmber);
        core.addColour (0.86, kEmber.withAlpha (0.85f));
        g.setGradientFill (core);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    }

    // ---- the near half of the disk, in front of it --------------------------
    {
        const auto front = kAmber.interpolatedWith (skin::colour::starWhite, 0.30f);
        paintDiskHalf (g, centre, diskRx, diskRy, true, front, 1.0f, u * 0.0115f);
        paintGrains (g, centre, u, front);
    }

    // ---- shimmer ------------------------------------------------------------
    // Two rings on their way out, which is what the plug-in does to a spectrum. Kept
    // near the threshold of visible on purpose: any louder and the mark reads as a
    // planet with orbits round it.
    for (int i = 0; i < 2; ++i)
    {
        // Both inside the plate: a ring the rounded corners cut through reads as a
        // mistake rather than as something passing behind the frame.
        const auto r = radius * (2.35f + 1.05f * (float) i);

        juce::Path ring;
        ring.addEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
        g.setColour (skin::colour::ice.withAlpha (0.085f - 0.035f * (float) i));
        g.strokePath (ring, juce::PathStrokeType (juce::jmax (0.6f, u * 0.0035f)));
    }

    // ---- the edge of the plate ----------------------------------------------
    // A hairline of the panel's ice blue along the top, fading to nothing at the
    // bottom, and a dark line just inside it. Without them the mark dissolves into a
    // dark dock or a dark title bar.
    {
        const auto edge = square.reduced (u * 0.004f);
        juce::ColourGradient rim (skin::colour::ice.withAlpha (0.30f),
                                  edge.getCentreX(), edge.getY(),
                                  skin::colour::ice.withAlpha (0.04f),
                                  edge.getCentreX(), edge.getBottom(), false);
        g.setGradientFill (rim);
        g.drawRoundedRectangle (edge, u * kCorner, juce::jmax (0.7f, u * 0.008f));

        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawRoundedRectangle (square.reduced (u * 0.016f), u * (kCorner - 0.016f),
                                juce::jmax (0.5f, u * 0.005f));
    }
}

juce::Image render (int sizePixels)
{
    const auto side = juce::jmax (8, sizePixels);

    juce::Image image (juce::Image::ARGB, side, side, true);
    juce::Graphics g (image);
    paint (g, juce::Rectangle<float> ((float) side, (float) side));

    return image;
}

} // namespace dying::icon
