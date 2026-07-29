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

#include "Controls.h"

#include "../dsp/Utils.h"

namespace dying
{

// ============================================================================
//  FreezePill
// ============================================================================

FreezePill::FreezePill (juce::AudioProcessorValueTreeState& state,
                        const juce::String& parameterID)
    : juce::Button ("Freeze")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                     state, parameterID, *this);

    lit = getToggleState() ? 1.0f : 0.0f;

    // The initial value has to be right before the first paint: a headless screenshot
    // never runs a timer, and neither does the first frame the user sees.
    if (getToggleState())
        startTimerHz (24);
}

FreezePill::~FreezePill() = default;

void FreezePill::buttonStateChanged()
{
    if (getToggleState() && ! isTimerRunning())
        startTimerHz (24);

    repaint();
}

void FreezePill::timerCallback()
{
    const auto target = getToggleState() ? 1.0f : 0.0f;
    lit += (target - lit) * 0.18f;

    phase += 0.04f;
    if (phase > dsp::kTwoPi) phase -= dsp::kTwoPi;

    if (! getToggleState() && lit < 0.01f)
    {
        lit = 0.0f;
        stopTimer();
    }

    repaint();
}

void FreezePill::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const auto corner = bounds.getHeight() * 0.5f;
    const auto ice = skin::colour::ice;
    const auto shimmer = 0.5f + 0.5f * std::sin (phase);

    // Body
    juce::ColourGradient body (juce::Colour (0xff141a30).interpolatedWith (
                                   ice.withMultipliedBrightness (0.55f), lit * 0.55f),
                               bounds.getCentreX(), bounds.getY(),
                               juce::Colour (0xff070a16).interpolatedWith (
                                   ice.withMultipliedBrightness (0.25f), lit * 0.5f),
                               bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (bounds, corner);

    if (lit > 0.01f)
    {
        // Inner light, strongest just under the top edge.
        juce::ColourGradient inner (ice.withAlpha (0.38f * lit * (0.8f + 0.2f * shimmer)),
                                    bounds.getCentreX(), bounds.getY(),
                                    ice.withAlpha (0.0f),
                                    bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (bounds, corner);

        juce::Path outline;
        outline.addRoundedRectangle (bounds, corner);
        skin::glowPath (g, outline, ice, 1.4f, 0.55f * lit * (0.85f + 0.15f * shimmer), 4);
    }

    g.setColour (ice.withAlpha (0.18f + 0.5f * lit + (highlighted ? 0.15f : 0.0f)));
    g.drawRoundedRectangle (bounds, corner, 1.2f);

    // Frost spurs either side of the label when lit.
    if (lit > 0.05f)
    {
        const auto cy = bounds.getCentreY();

        for (int side = 0; side < 2; ++side)
        {
            const auto x = side == 0 ? bounds.getX() + bounds.getHeight() * 0.55f
                                     : bounds.getRight() - bounds.getHeight() * 0.55f;

            for (int i = -1; i <= 1; ++i)
            {
                const auto len = bounds.getHeight() * (0.20f + 0.06f * shimmer)
                                   * (i == 0 ? 1.0f : 0.6f);
                const auto a = (float) i * 0.9f + (side == 0 ? dsp::kPi : 0.0f);
                g.setColour (ice.withAlpha (0.45f * lit));
                g.drawLine (x, cy, x + std::cos (a) * len, cy + std::sin (a) * len, 1.1f);
            }
        }
    }

    const auto textColour = skin::colour::text.interpolatedWith (juce::Colours::white, lit)
                                .withAlpha (down ? 0.75f : 1.0f);

    auto textArea = bounds;
    skin::drawTracked (g, "FREEZE", skin::labelFont (13.0f, true),
                       textArea.translated (0.0f, 1.0f), juce::Justification::centred,
                       juce::Colours::black.withAlpha (0.5f), 4.0f);
    skin::drawTracked (g, "FREEZE", skin::labelFont (13.0f, true), textArea,
                       juce::Justification::centred, textColour, 4.0f);
}

// ============================================================================
//  EngagePill
// ============================================================================

EngagePill::EngagePill (juce::AudioProcessorValueTreeState& state,
                        const juce::String& parameterID,
                        const juce::String& captionText,
                        juce::Colour accentColour)
    : juce::Button (captionText), caption (captionText), accent (accentColour)
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                     state, parameterID, *this);

    // Right before the first paint, not after the first timer tick: a headless
    // screenshot never runs a timer and neither does the frame the user opens on.
    lit = getToggleState() ? 1.0f : 0.0f;
}

EngagePill::~EngagePill() = default;

void EngagePill::buttonStateChanged()
{
    if (! isTimerRunning() && std::abs ((getToggleState() ? 1.0f : 0.0f) - lit) > 0.01f)
        startTimerHz (30);

    repaint();
}

void EngagePill::timerCallback()
{
    const auto target = getToggleState() ? 1.0f : 0.0f;
    lit += (target - lit) * 0.25f;

    if (std::abs (target - lit) < 0.01f)
    {
        lit = target;
        stopTimer();
    }

    repaint();
}

void EngagePill::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const auto corner = bounds.getHeight() * 0.5f;

    juce::ColourGradient body (juce::Colour (0xff141a30).interpolatedWith (
                                   accent.withMultipliedBrightness (0.5f), lit * 0.5f),
                               bounds.getCentreX(), bounds.getY(),
                               juce::Colour (0xff070a16).interpolatedWith (
                                   accent.withMultipliedBrightness (0.22f), lit * 0.45f),
                               bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (bounds, corner);

    if (lit > 0.01f)
    {
        juce::ColourGradient inner (accent.withAlpha (0.32f * lit),
                                    bounds.getCentreX(), bounds.getY(),
                                    accent.withAlpha (0.0f),
                                    bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (bounds, corner);

        juce::Path outline;
        outline.addRoundedRectangle (bounds, corner);
        skin::glowPath (g, outline, accent, 1.2f, 0.5f * lit, 4);
    }

    g.setColour (accent.withAlpha (0.16f + 0.45f * lit + (highlighted ? 0.14f : 0.0f)));
    g.drawRoundedRectangle (bounds, corner, 1.1f);

    // A small lamp at the head of the label - the one part of the pill that reads as
    // on or off from across a room.
    const auto lamp = juce::Point<float> (bounds.getX() + bounds.getHeight() * 0.62f,
                                          bounds.getCentreY());
    if (lit > 0.02f)
        skin::glowEllipse (g, lamp, 2.6f, accent, 0.85f * lit, 4);

    g.setColour (juce::Colours::white.withAlpha (0.22f + 0.7f * lit));
    g.fillEllipse (lamp.x - 2.0f, lamp.y - 2.0f, 4.0f, 4.0f);

    const auto textColour = skin::colour::textDim.interpolatedWith (juce::Colours::white, lit)
                                .withAlpha (down ? 0.75f : 1.0f);

    auto textArea = bounds.withTrimmedLeft (bounds.getHeight() * 0.55f);
    skin::drawTracked (g, caption.toUpperCase(), skin::labelFont (10.5f, true),
                       textArea.translated (0.0f, 1.0f), juce::Justification::centred,
                       juce::Colours::black.withAlpha (0.5f), 3.0f);
    skin::drawTracked (g, caption.toUpperCase(), skin::labelFont (10.5f, true), textArea,
                       juce::Justification::centred, textColour, 3.0f);
}

// ============================================================================
//  TailMeter
// ============================================================================

TailMeter::TailMeter()
{
    setInterceptsMouseClicks (false, false);
}

void TailMeter::setProvider (std::function<float()> fn)
{
    provider = std::move (fn);
    refresh();
}

void TailMeter::refresh()
{
    if (provider)
        level = juce::jlimit (0.0f, 1.0f, provider());

    repaint();
}

void TailMeter::visibilityChanged()
{
    if (isVisible())
        startTimerHz (30);
    else
        stopTimer();
}

void TailMeter::timerCallback()
{
    const auto target = provider ? juce::jlimit (0.0f, 1.0f, provider()) : 0.0f;

    // Asymmetric ballistics: quick to rise so a transient registers, slow to fall so a
    // long tail stays readable instead of flickering.
    level += (target - level) * (target > level ? 0.45f : 0.09f);
    repaint();
}

void TailMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto captionArea = bounds.removeFromTop (13.0f);
    auto bar = bounds.reduced (0.0f, 2.0f);

    skin::drawTracked (g, "TAIL ENERGY", skin::labelFont (8.5f, true), captionArea,
                       juce::Justification::centredLeft, skin::colour::textFaint, 2.0f);

    const auto corner = bar.getHeight() * 0.5f;

    g.setColour (juce::Colour (0x40060a16));
    g.fillRoundedRectangle (bar, corner);
    g.setColour (juce::Colour (0x1affffff));
    g.drawRoundedRectangle (bar, corner, 1.0f);

    // Perceptual-ish curve: the interesting part of a reverb tail lives in the bottom
    // of the linear range, so a square root spreads it across the bar.
    const auto filled = std::sqrt (level);

    if (filled > 0.005f)
    {
        auto fill = bar.reduced (1.5f).withWidth ((bar.getWidth() - 3.0f) * filled);

        juce::ColourGradient grad (skin::colour::gravity, fill.getX(), 0.0f,
                                   skin::colour::collapse, bar.getRight(), 0.0f, false);
        grad.addColour (0.45, skin::colour::master);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, juce::jmin (corner, fill.getWidth() * 0.5f));

        juce::Path cap;
        cap.addRoundedRectangle (fill, juce::jmin (corner, fill.getWidth() * 0.5f));
        skin::glowPath (g, cap, skin::colour::master, 1.0f, 0.35f + 0.4f * level, 3);
    }

    for (int i = 1; i < 8; ++i)
    {
        const auto x = bar.getX() + bar.getWidth() * (float) i / 8.0f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (x, bar.getY() + 2.0f, x, bar.getBottom() - 2.0f, 1.0f);
    }
}

// ============================================================================
//  GlyphButton
// ============================================================================

GlyphButton::GlyphButton (bool pointsRight)
    : juce::Button ({}), right (pointsRight)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GlyphButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (getWidth() * 0.28f, getHeight() * 0.30f);

    juce::Path triangle;

    if (right)
    {
        triangle.startNewSubPath (bounds.getX(), bounds.getY());
        triangle.lineTo (bounds.getRight(), bounds.getCentreY());
        triangle.lineTo (bounds.getX(), bounds.getBottom());
    }
    else
    {
        triangle.startNewSubPath (bounds.getRight(), bounds.getY());
        triangle.lineTo (bounds.getX(), bounds.getCentreY());
        triangle.lineTo (bounds.getRight(), bounds.getBottom());
    }

    triangle.closeSubPath();

    g.setColour (skin::colour::textDim.withAlpha (down ? 0.6f : (highlighted ? 1.0f : 0.7f)));
    g.fillPath (triangle);
}

// ============================================================================
//  PresetBar
// ============================================================================

PresetBar::PresetBar()
{
    menuLookAndFeel.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf00a0d1c));
    menuLookAndFeel.setColour (juce::PopupMenu::textColourId, skin::colour::text);
    menuLookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId,
                               juce::Colour (0xff2a3a6a));
    menuLookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    addAndMakeVisible (prev);
    addAndMakeVisible (next);

    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step ( 1); };

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PresetBar::step (int delta)
{
    if (! getNames || ! currentIndex || ! onSelect)
        return;

    const auto names = getNames();

    if (names.isEmpty())
        return;

    auto index = currentIndex() + delta;
    index = (index % names.size() + names.size()) % names.size();
    onSelect (index);
    refresh();
}

void PresetBar::refresh()
{
    if (getNames && currentIndex)
    {
        const auto names = getNames();
        const auto index = currentIndex();
        displayName = juce::isPositiveAndBelow (index, names.size()) ? names[index]
                                                                    : juce::String ("--");
    }

    repaint();
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    const auto side = area.getHeight();
    prev.setBounds (area.removeFromLeft (side));
    next.setBounds (area.removeFromRight (side));
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    if (! getNames || ! onSelect || e.mouseWasDraggedSinceMouseDown())
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&menuLookAndFeel);

    const auto names = getNames();
    const auto index = currentIndex ? currentIndex() : -1;

    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, i == index);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
                        {
                            if (result > 0 && onSelect)
                            {
                                onSelect (result - 1);
                                refresh();
                            }
                        });
}

void PresetBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto corner = 5.0f;

    g.setColour (juce::Colour (0x33101832));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (skin::colour::master.withAlpha (hovering ? 0.35f : 0.16f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    auto textArea = bounds.reduced (bounds.getHeight(), 0.0f);
    skin::drawTracked (g, displayName.toUpperCase(), skin::labelFont (10.5f, true), textArea,
                       juce::Justification::centred,
                       hovering ? skin::colour::text : skin::colour::textDim, 1.8f);
}

} // namespace dying
