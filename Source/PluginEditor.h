#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/Vu.h"
#include "ui/OniriqueBar.h"
#if SOLO_LICENCE
 #include "License/LicenseOverlay.h"
#endif

//==============================================================================
//  Une colonne de boutons liée à un réglage à choix (RATIO, METER, MODE) : la case choisie s'allume en braise, comme les
//  boutons enfoncés d'un appareil de studio. reversed : le dernier choix en haut (RATIO : ALL en haut, 4 en bas).
//==============================================================================
class ButtonColumn : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ButtonColumn (juce::RangedAudioParameter& p, juce::StringArray names, bool reversedOrder)
        : items (std::move (names)), reversed (reversedOrder),
          attachment (p, [this] (float v) { index = (int) std::lround (v); startTimerHz (60); })
    {
        attachment.sendInitialUpdate();
        lit.resize ((size_t) items.size(), 0.0f);
        for (int i = 0; i < items.size(); ++i) lit[(size_t) i] = i == index ? 1.0f : 0.0f;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void setScale (float s) { scale = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < items.size(); ++i)
        {
            const auto r = cell (i);
            // chaque case se voit, même éteinte (le bouton d'un appareil)
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r, 8.0f * scale);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f * scale, 1.0f);
            onirique::da::seg (g, r, lit[(size_t) i], i == hover ? 1.0f : 0.0f, scale, 8.0f * scale);
            ui::text (g, items[i], r, ui::font (ui::Face::mono, 11.0f * scale, 0.12f),
                      ui::haze.interpolatedWith (ui::bone, juce::jmax (lit[(size_t) i], i == hover ? 0.5f : 0.0f)));
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = at (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int i = at (e.position); if (i >= 0) attachment.setValueAsCompleteGesture ((float) i); }

private:
    juce::Rectangle<float> cell (int i) const
    {
        const int n = items.size(), row = reversed ? n - 1 - i : i;
        const float h = (float) getHeight() / (float) n;
        return juce::Rectangle<float> (0.0f, h * (float) row, (float) getWidth(), h).reduced (2.0f * scale);
    }
    int at (juce::Point<float> p) const { for (int i = 0; i < items.size(); ++i) if (cell (i).contains (p)) return i; return -1; }
    void timerCallback() override
    {
        bool moving = false;
        for (int i = 0; i < items.size(); ++i)
        {
            auto& l = lit[(size_t) i];
            const float t = i == index ? 1.0f : 0.0f;
            if (std::abs (l - t) > 0.01f) { l += 0.25f * (t - l); moving = true; } else l = t;
        }
        repaint();
        if (! moving) stopTimer();
    }
    juce::StringArray items;
    bool reversed = false;
    juce::ParameterAttachment attachment;
    std::vector<float> lit;
    int index = 0, hover = -1;
    float scale = 1.0f;
};

//==============================================================================
//  L'interface : la barre ONIRIQUE (presets, IN, A/B, licence), la façade de l'appareil dans la DA ONIRIQUE (métal sombre
//  brossé, oreilles de rack vissées, liseré braise), puis la bande du bas (OVERSAMPLING, MIX, réduction).
//    1176   INPUT et OUTPUT en grand, ATTACK et RELEASE empilés, la colonne RATIO, le VU (BLACK / BLUE au-dessus), METER
//    LA-2A  la colonne MODE, GAIN en grand, le VU au centre, PEAK REDUCTION en grand, METER
//==============================================================================
class SoloEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SoloEditor (SoloProcessor&);
    ~SoloEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { processor.undo.beginNewTransaction(); }

    static constexpr int baseWidth = 1180, baseHeight = 640;

private:
    juce::RangedAudioParameter& p (const juce::String& id) { return *processor.apvts.getParameter (id); }
    void renderBackground();
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) const;
    void timerCallback() override;
    float vuValue();
    float scale() const { return (float) getWidth() / (float) baseWidth; }

    SoloProcessor& processor;
    ui::FontScope fonts;
    ui::KnobLook knobLook;
    ui::MenuLook menuLook;
    juce::Image background, logo;
    juce::TooltipWindow tooltips { this, 600 };

#if SOLO_LA2A
    ui::Knob gain, peak;
    ButtonColumn mode;
#else
    ui::Knob input, output, attack, release;
    ButtonColumn ratio;
    ui::Segments revision;
#endif
    ButtonColumn meter;
    ui::VintageVu vu;
    ui::Segments oversampling;
    ui::Knob mix;
    float outDb = -60.0f, grShown = 0.0f;

    onirique::nuit::Bar bar;
    bool sizeReady = false;
    int themeSeen = -1;
#if SOLO_LICENCE
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloEditor)
};
