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

#include "PluginEditor.h"
#include "Presets.h"

namespace dying
{

namespace
{
    constexpr float kMargin = 14.0f;

    // Panel geometry, in logical pixels. Written out rather than computed so the
    // backdrop's frames and the widgets inside them can never drift apart.
    constexpr float kHeaderY = 6.0f,   kHeaderH = 42.0f;
    constexpr float kStarY   = 52.0f,  kStarH   = 246.0f;
    constexpr float kPanelY  = 306.0f, kPanelH  = 268.0f;
    constexpr float kMasterY = 586.0f, kMasterH = 146.0f;
    constexpr float kFooterY = 736.0f;

    constexpr float kGravityX  =  14.0f, kGravityW  = 234.0f;
    constexpr float kSpectrumX = 266.0f, kSpectrumW = 234.0f;
    constexpr float kDriftX    = 518.0f, kDriftW    = 334.0f;
    constexpr float kCollapseX = 870.0f, kCollapseW = 136.0f;

    constexpr float kTitleStripH = 32.0f;

    /** Lays out n cells across the width of a row, honouring a maximum cell width so a
        row of two does not end up with the knobs a mile apart. */
    void layOutRow (juce::Rectangle<float> row, const std::vector<CosmicKnob*>& items,
                    float cellWidth, float diameter)
    {
        if (items.empty())
            return;

        // Each knob gets exactly the height it needs, centred in the row. Letting it
        // fill the row instead would push the readout to the row's bottom edge, hard
        // against the next row's caption - and then it is not obvious which value
        // belongs to which control.
        const auto contentHeight = juce::jmin (row.getHeight(),
                                               CosmicKnob::captionHeight + diameter
                                                   + CosmicKnob::readoutHeight);
        const auto y = row.getCentreY() - contentHeight * 0.5f;

        const auto total = cellWidth * (float) items.size();
        auto x = row.getCentreX() - total * 0.5f;

        for (auto* item : items)
        {
            item->setKnobDiameter (diameter);
            item->setBounds (juce::Rectangle<float> (x, y, cellWidth, contentHeight)
                                 .toNearestInt());
            x += cellWidth;
        }
    }
}

// ============================================================================
//  Chrome
// ============================================================================

void DyingStarEditor::Chrome::paint (juce::Graphics& g)
{
    // ---- header ------------------------------------------------------------
    auto header = headerArea;
    auto titleArea = header.removeFromTop (24.0f).withTrimmedLeft (26.0f);
    auto subtitleArea = header.withTrimmedLeft (26.0f);

    const auto title = juce::String ("THE SOUND OF A DYING STAR");
    const auto font = skin::labelFont (15.0f, true);

    skin::drawTracked (g, title, font, titleArea.translated (0.0f, 1.5f),
                       juce::Justification::left, juce::Colours::black.withAlpha (0.6f), 5.5f);
    skin::drawTracked (g, title, font, titleArea, juce::Justification::left,
                       skin::colour::master.withAlpha (0.30f), 5.5f);
    skin::drawTracked (g, title, font, titleArea, juce::Justification::left,
                       skin::colour::starWhite, 5.5f);

    skin::drawTracked (g, "COSMIC REVERB " + skin::u8 ("\xc2\xb7") + " INFINITE SPACES",
                       skin::labelFont (8.5f, false), subtitleArea, juce::Justification::left,
                       skin::colour::textFaint, 3.0f);

    // A small sun mark at the head of the title, so the corner is not just type.
    {
        const auto centre = juce::Point<float> (headerArea.getX() + 9.0f,
                                                headerArea.getY() + 16.0f);
        skin::glowEllipse (g, centre, 3.2f, skin::colour::master, 0.85f, 4);
        g.setColour (skin::colour::starWhite);
        g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);

        for (int i = 0; i < 4; ++i)
        {
            const auto a = dsp::kPi * 0.25f + dsp::kPi * 0.5f * (float) i;
            g.setColour (skin::colour::master.withAlpha (0.5f));
            g.drawLine (centre.x + std::cos (a) * 4.5f, centre.y + std::sin (a) * 4.5f,
                        centre.x + std::cos (a) * 8.5f, centre.y + std::sin (a) * 8.5f, 1.0f);
        }
    }

    skin::drawTracked (g, "PRESET", skin::labelFont (8.5f, true),
                       juce::Rectangle<float> (headerArea.getRight() - 314.0f,
                                               headerArea.getY() + 8.0f, 62.0f, 26.0f),
                       juce::Justification::right, skin::colour::textFaint, 2.2f);

    // ---- footer ------------------------------------------------------------
    juce::String build = "v" JucePlugin_VersionString "  " + skin::u8 ("\xc2\xb7")
                           + "  AGPLv3  " + skin::u8 ("\xc2\xb7") + "  BUILT WITH JUCE 8";

   #ifdef DYINGSTAR_BUILD_ID
    build += juce::String ("  ") + skin::u8 ("\xc2\xb7") + "  " + DYINGSTAR_BUILD_ID;
   #endif

    skin::drawTracked (g, build, skin::labelFont (8.0f, false), footerArea,
                       juce::Justification::horizontallyCentred,
                       skin::colour::textFaint.withAlpha (0.85f), 1.6f);
}

// ============================================================================
//  Editor
// ============================================================================

std::unique_ptr<CosmicKnob> DyingStarEditor::makeKnob (const juce::String& parameterID,
                                                       const juce::String& caption,
                                                       juce::Colour accent,
                                                       bool bipolar)
{
    auto knob = std::make_unique<CosmicKnob> (processor.getState(), parameterID, caption,
                                              accent, bipolar);
    content.addAndMakeVisible (*knob);
    return knob;
}

DyingStarEditor::DyingStarEditor (DyingStarProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      freeze (p.getState(), pid::freeze)
{
    addAndMakeVisible (content);
    content.addAndMakeVisible (backdrop);
    content.addAndMakeVisible (chrome);
    content.addAndMakeVisible (star);
    content.addAndMakeVisible (tailMeter);
    content.addAndMakeVisible (freeze);
    content.addAndMakeVisible (presets);

    using namespace skin::colour;

    kPreDelay  = makeKnob (pid::preDelay,  "Pre-Delay", gravity);
    kSize      = makeKnob (pid::size,      "Size",      gravity);
    kDecay     = makeKnob (pid::decay,     "Decay",     gravity);
    kDiffusion = makeKnob (pid::diffusion, "Diffusion", gravity);

    kDamping   = makeKnob (pid::damping,   "Damping",   spectrum);
    kLowCut    = makeKnob (pid::lowCut,    "Low Cut",   spectrum);
    kHighCut   = makeKnob (pid::highCut,   "High Cut",  spectrum);
    kWidth     = makeKnob (pid::width,     "Width",     spectrum);

    kShimmer   = makeKnob (pid::shimmer,   "Shimmer",   drift);
    kPitch     = makeKnob (pid::shimPitch, "Pitch",     drift, true);
    kDetune    = makeKnob (pid::detune,    "Detune",    drift);
    kModRate   = makeKnob (pid::modRate,   "Mod Rate",  drift);
    kModDepth  = makeKnob (pid::modDepth,  "Mod Depth", drift);

    kCollapse  = makeKnob (pid::collapse,  "Collapse",  skin::colour::collapse);
    kMass      = makeKnob (pid::mass,      "Mass",      skin::colour::collapse);

    kMix       = makeKnob (pid::mix,       "Mix",       master);
    kOutput    = makeKnob (pid::output,    "Output",    master, true);

    // ---- live feeds ---------------------------------------------------------
    star.setStateProvider ([this]
    {
        auto& state = processor.getState();
        auto value = [&state] (const char* id)
        {
            auto* raw = state.getRawParameterValue (id);
            return raw != nullptr ? raw->load() : 0.0f;
        };

        StarState s;
        s.level      = processor.getWetLevel();
        s.brightness = processor.getBrightness();
        s.collapse   = value (pid::collapse) * 0.01f;
        s.mass       = value (pid::mass)     * 0.01f;
        s.decay      = value (pid::decay)    * 0.01f;
        s.shimmer    = value (pid::shimmer)  * 0.01f;
        s.size       = value (pid::size)     * 0.01f;
        s.modRate    = value (pid::modRate);
        s.freeze     = value (pid::freeze) > 0.5f;
        return s;
    });

    tailMeter.setProvider ([this] { return processor.getWetLevel(); });

    presets.getNames = []
    {
        juce::StringArray names;

        for (const auto& preset : getFactoryPresets())
            names.add (preset.name);

        return names;
    };

    presets.currentIndex = [this] { return processor.getCurrentProgram(); };
    presets.onSelect = [this] (int index) { processor.setCurrentProgram (index); };
    presets.refresh();

    // ---- window -------------------------------------------------------------
    setResizable (true, true);
    setResizeLimits (816, 605, 1632, 1210);

    if (auto* sizeConstrainer = getConstrainer())
        sizeConstrainer->setFixedAspectRatio ((double) kLogicalWidth / (double) kLogicalHeight);

    const auto storedWidth = (int) processor.getState().state.getProperty ("uiWidth",
                                                                          kLogicalWidth);
    const auto width = juce::jlimit (816, 1632, storedWidth);
    setSize (width, juce::roundToInt ((double) width * kLogicalHeight / kLogicalWidth));
}

DyingStarEditor::~DyingStarEditor() = default;

void DyingStarEditor::paint (juce::Graphics& g)
{
    // Only ever visible in the letterboxed margin when a host forces an odd size.
    g.fillAll (skin::colour::voidDeep);
}

void DyingStarEditor::resized()
{
    const auto scale = juce::jmin ((float) getWidth()  / (float) kLogicalWidth,
                                   (float) getHeight() / (float) kLogicalHeight);

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, kLogicalWidth, kLogicalHeight);

    layOutContent();
    storeEditorSize();
}

void DyingStarEditor::storeEditorSize()
{
    processor.getState().state.setProperty ("uiWidth", getWidth(), nullptr);
}

void DyingStarEditor::layOutContent()
{
    backdrop.setBounds (content.getLocalBounds());
    chrome.setBounds (content.getLocalBounds());

    const auto contentWidth = (float) kLogicalWidth;

    chrome.headerArea = { kMargin, kHeaderY, contentWidth - 2.0f * kMargin, kHeaderH };
    chrome.footerArea = { kMargin, kFooterY, contentWidth - 2.0f * kMargin, 16.0f };

    presets.setBounds (juce::Rectangle<float> (contentWidth - kMargin - 238.0f,
                                               kHeaderY + 8.0f, 238.0f, 26.0f).toNearestInt());

    star.setBounds (juce::Rectangle<float> (kMargin, kStarY,
                                            contentWidth - 2.0f * kMargin, kStarH).toNearestInt());

    // ---- section frames, shared with the backdrop ---------------------------
    const juce::Rectangle<float> gravityPanel  { kGravityX,  kPanelY, kGravityW,  kPanelH };
    const juce::Rectangle<float> spectrumPanel { kSpectrumX, kPanelY, kSpectrumW, kPanelH };
    const juce::Rectangle<float> driftPanel    { kDriftX,    kPanelY, kDriftW,    kPanelH };
    const juce::Rectangle<float> collapsePanel { kCollapseX, kPanelY, kCollapseW, kPanelH };
    const juce::Rectangle<float> masterPanel   { kMargin,    kMasterY,
                                                 contentWidth - 2.0f * kMargin, kMasterH };
    const juce::Rectangle<float> starPanel     { kMargin,    kStarY,
                                                 contentWidth - 2.0f * kMargin, kStarH };

    backdrop.setSections ({
        { starPanel,     {},           skin::colour::master.withAlpha (0.5f) },
        { gravityPanel,  "GRAVITY",    skin::colour::gravity  },
        { spectrumPanel, "SPECTRUM",   skin::colour::spectrum },
        { driftPanel,    "DRIFT",      skin::colour::drift    },
        { collapsePanel, "COLLAPSE",   skin::colour::collapse },
        { masterPanel,   "EMISSION",   skin::colour::master   },
    });

    // ---- knobs --------------------------------------------------------------
    auto panelBody = [] (juce::Rectangle<float> panel)
    {
        auto body = panel.reduced (14.0f, 0.0f);
        body.removeFromTop (kTitleStripH);
        body.removeFromBottom (10.0f);
        return body;
    };

    {
        auto body = panelBody (gravityPanel);
        const auto rowH = body.getHeight() * 0.5f;
        auto top = body.removeFromTop (rowH);
        layOutRow (top,  { kPreDelay.get(), kSize.get() },      101.0f, 62.0f);
        layOutRow (body, { kDecay.get(), kDiffusion.get() },    101.0f, 62.0f);
    }

    {
        auto body = panelBody (spectrumPanel);
        const auto rowH = body.getHeight() * 0.5f;
        auto top = body.removeFromTop (rowH);
        layOutRow (top,  { kLowCut.get(), kHighCut.get() },     101.0f, 62.0f);
        layOutRow (body, { kDamping.get(), kWidth.get() },      101.0f, 62.0f);
    }

    {
        auto body = panelBody (driftPanel);
        const auto rowH = body.getHeight() * 0.5f;
        auto top = body.removeFromTop (rowH);
        layOutRow (top,  { kShimmer.get(), kPitch.get(), kDetune.get() }, 101.0f, 62.0f);
        layOutRow (body, { kModRate.get(), kModDepth.get() },             101.0f, 62.0f);
    }

    {
        auto body = panelBody (collapsePanel);
        const auto rowH = body.getHeight() * 0.5f;
        auto top = body.removeFromTop (rowH);
        layOutRow (top,  { kCollapse.get() }, 106.0f, 62.0f);
        layOutRow (body, { kMass.get() },     106.0f, 62.0f);
    }

    // ---- master row ---------------------------------------------------------
    {
        auto body = masterPanel.reduced (20.0f, 0.0f);
        body.removeFromTop (34.0f);
        body.removeFromBottom (8.0f);

        auto freezeArea = body.removeFromLeft (268.0f);
        freeze.setBounds (freezeArea.withSizeKeepingCentre (252.0f, 58.0f).toNearestInt());

        body.removeFromLeft (26.0f);

        auto meterArea = body.removeFromLeft (368.0f);
        tailMeter.setBounds (meterArea.withSizeKeepingCentre (368.0f, 44.0f).toNearestInt());

        layOutRow (body.removeFromRight (128.0f), { kOutput.get() }, 128.0f, 74.0f);
        layOutRow (body.removeFromRight (128.0f), { kMix.get() },    128.0f, 74.0f);
    }
}

} // namespace dying
