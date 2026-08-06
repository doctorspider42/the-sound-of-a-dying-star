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

#include "CosmicKnob.h"

namespace dying
{

namespace
{
    constexpr float kStartAngle = -2.35619449f;   // -135 degrees
    constexpr float kEndAngle   =  2.35619449f;   // +135 degrees
}

class CosmicKnob::LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (juce::Colour accentColour, bool isBipolar)
        : accent (accentColour), bipolar (isBipolar) {}

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (1.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
        const auto active = s.isMouseOverOrDragging() || s.isMouseButtonDown();

        // ---- graduation ticks ------------------------------------------------
        const auto tickOuter = radius * 0.99f;
        const auto tickInner = radius * 0.90f;

        for (int i = 0; i <= 10; ++i)
        {
            const auto t = (float) i / 10.0f;
            const auto a = startAngle + t * (endAngle - startAngle);
            const auto dx = std::sin (a), dy = -std::cos (a);
            const auto passed = bipolar ? (std::abs (t - 0.5f) <= std::abs (sliderPos - 0.5f)
                                            && ((t - 0.5f) * (sliderPos - 0.5f)) >= 0.0f)
                                        : (t <= sliderPos);

            g.setColour (passed ? accent.withAlpha (0.55f)
                                : skin::colour::textFaint.withAlpha (0.45f));
            g.drawLine (centre.x + dx * tickInner, centre.y + dy * tickInner,
                        centre.x + dx * tickOuter, centre.y + dy * tickOuter, 1.1f);
        }

        // ---- track -----------------------------------------------------------
        const auto arcRadius = radius * 0.80f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (juce::Colour (0x22ffffff));
        g.strokePath (track, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // ---- value arc -------------------------------------------------------
        const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        if (std::abs (angle - from) > 0.004f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 juce::jmin (from, angle), juce::jmax (from, angle), true);
            skin::glowPath (g, value, accent, 3.6f, active ? 1.0f : 0.86f, 4);
        }

        // ---- body ------------------------------------------------------------
        const auto bodyRadius = radius * 0.655f;
        const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f)
                              .withCentre (centre);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.5f));

        juce::ColourGradient sphere (juce::Colour (0xff222a4e),
                                     centre.x - bodyRadius * 0.5f, centre.y - bodyRadius * 0.7f,
                                     juce::Colour (0xff05070f),
                                     centre.x + bodyRadius * 0.4f, centre.y + bodyRadius * 0.9f,
                                     true);
        g.setGradientFill (sphere);
        g.fillEllipse (body);

        // Rim light along the top-left, matching the light direction used everywhere.
        juce::Path rim;
        rim.addCentredArc (centre.x, centre.y, bodyRadius * 0.96f, bodyRadius * 0.96f, 0.0f,
                           -2.5f, -0.35f, true);
        g.setColour (juce::Colours::white.withAlpha (active ? 0.24f : 0.15f));
        g.strokePath (rim, juce::PathStrokeType (1.3f));

        g.setColour (accent.withAlpha (active ? 0.30f : 0.14f));
        g.drawEllipse (body, 1.0f);

        // ---- pointer ---------------------------------------------------------
        juce::Path pointer;
        pointer.startNewSubPath (0.0f, -bodyRadius * 0.30f);
        pointer.lineTo (0.0f, -bodyRadius * 0.90f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                    .translated (centre.x, centre.y));
        skin::glowPath (g, pointer, accent.brighter (0.35f), 2.5f, active ? 1.0f : 0.9f, 3);

        skin::glowEllipse (g, centre, bodyRadius * 0.085f, accent.brighter (0.5f),
                           active ? 0.9f : 0.65f, 4);
    }

private:
    juce::Colour accent;
    bool bipolar;
};

CosmicKnob::CosmicKnob (juce::AudioProcessorValueTreeState& state,
                        const juce::String& parameterID,
                        const juce::String& captionText,
                        juce::Colour accentColour,
                        bool bipolar)
    : caption (captionText), accent (accentColour)
{
    lookAndFeel = std::make_unique<LookAndFeel> (accent, bipolar);

    if (auto* p = state.getParameter (parameterID))
    {
        suffix = p->getLabel();
        paramRange = p->getNormalisableRange();
    }

    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    slider.setMouseDragSensitivity (190);
    slider.setLookAndFeel (lookAndFeel.get());
    slider.setWantsKeyboardFocus (false);
    slider.setMouseCursor (juce::MouseCursor::PointingHandCursor);

    // The readout has to follow the value, and the dial has to brighten on hover -
    // both are repaints of this component, not of the slider.
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart   = [this] { repaint(); };
    slider.onDragEnd     = [this] { repaint(); };

    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                     state, parameterID, slider);
}

CosmicKnob::~CosmicKnob()
{
    slider.setLookAndFeel (nullptr);
}

void CosmicKnob::setStepped (bool wholeUnits)
{
    // A step is the one thing a plain range can carry that the attachment's skew-aware
    // one does not need to; anything with a law of its own would lose it here.
    jassert (juce::approximatelyEqual (paramRange.skew, 1.0f));

    slider.setRange ((double) paramRange.start, (double) paramRange.end,
                     wholeUnits ? 1.0 : (double) paramRange.interval);

    // The slider re-snaps its own value without telling the parameter, which is what
    // should happen: the engine already reads the value the same way, so the readout
    // follows the sound rather than the panel writing over a stored setting.
    repaint();
}

void CosmicKnob::resized()
{
    auto area = getLocalBounds().toFloat();
    area.removeFromTop (captionHeight);
    area.removeFromBottom (readoutHeight);

    const auto d = juce::jmin (diameter, juce::jmin (area.getWidth(), area.getHeight()));
    slider.setBounds (juce::Rectangle<float> (d, d).withCentre (area.getCentre()).toNearestInt());
}

void CosmicKnob::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto active = slider.isMouseOverOrDragging() || slider.isMouseButtonDown();

    auto captionArea = area.removeFromTop (captionHeight);
    skin::drawTracked (g, caption.toUpperCase(), skin::labelFont (9.5f, true),
                       captionArea.translated (0.0f, 1.0f), juce::Justification::centred,
                       juce::Colours::black.withAlpha (0.6f), 1.5f);
    skin::drawTracked (g, caption.toUpperCase(), skin::labelFont (9.5f, true), captionArea,
                       juce::Justification::centred,
                       active ? skin::colour::text : skin::colour::textDim, 1.5f);

    auto readoutArea = area.removeFromBottom (readoutHeight);
    auto text = slider.getTextFromValue (slider.getValue());

    if (suffix.isNotEmpty())
        text += " " + suffix;

    g.setFont (skin::displayFont (11.5f));
    g.setColour (active ? accent.brighter (0.4f) : skin::colour::text.withAlpha (0.72f));
    g.drawText (text, readoutArea, juce::Justification::centred, false);
}

} // namespace dying
