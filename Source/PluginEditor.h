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
    /** montre cette case allumée quel que soit le réglage (-1 : le réglage) ; L2, câblé en LIMIT */
    void setForced (int i) { if (i != forced) { forced = i; startTimerHz (60); } }

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
            const float t = i == (forced >= 0 ? forced : index) ? 1.0f : 0.0f;
            if (std::abs (l - t) > 0.01f) { l += 0.25f * (t - l); moving = true; } else l = t;
        }
        repaint();
        if (! moving) stopTimer();
    }
    juce::StringArray items;
    bool reversed = false;
    juce::ParameterAttachment attachment;
    std::vector<float> lit;
    int index = 0, hover = -1, forced = -1;
    float scale = 1.0f;
};

//==============================================================================
//  Le menu du modèle (L2A) : une pastille « MODEL  TUBE ▾ » au-dessus du VU ; un clic ouvre la liste des modèles.
class ModelMenu : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModelMenu (juce::RangedAudioParameter& p, juce::StringArray names, juce::StringArray details)
        : items (std::move (names)), notes (std::move (details)),
          attachment (p, [this] (float v) { index = (int) std::lround (v); repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void setScale (float s) { scale = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float rad = r.getHeight() * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (hover || open ? ui::accent.withAlpha (0.7f) : juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (r, rad, 1.0f);
        auto in = r.reduced (14.0f * scale, 0.0f);
        const auto chev = in.removeFromRight (12.0f * scale);
        ui::text (g, "MODEL", in.removeFromLeft (56.0f * scale), ui::font (ui::Face::mono, 9.5f * scale, 0.16f), ui::haze,
                  juce::Justification::centredLeft);
        ui::text (g, items[juce::jlimit (0, items.size() - 1, index)], in, ui::font (ui::Face::display, 12.5f * scale, 0.1f),
                  ui::bone, juce::Justification::centredLeft);
        const auto c = chev.getCentre();
        juce::Path p;
        p.startNewSubPath (c.x - 4.0f * scale, c.y - 2.0f * scale);
        p.lineTo (c.x, c.y + 2.5f * scale);
        p.lineTo (c.x + 4.0f * scale, c.y - 2.0f * scale);
        g.setColour (hover || open ? ui::bone : ui::accent);
        g.strokePath (p, juce::PathStrokeType (1.6f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override
    {
        juce::PopupMenu m;
        for (int i = 0; i < items.size(); ++i)
            m.addItem (i + 1, items[i] + "   " + notes[i], true, i == index);
        open = true; repaint();
        juce::Component::SafePointer<ModelMenu> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                         [safe] (int r)
                         {
                             if (safe == nullptr) return;
                             safe->open = false; safe->repaint();
                             if (r > 0) safe->attachment.setValueAsCompleteGesture ((float) (r - 1));
                         });
    }

private:
    juce::StringArray items, notes;
    juce::ParameterAttachment attachment;
    int index = 0;
    bool hover = false, open = false;
    float scale = 1.0f;
};

//==============================================================================
//  L'interface : la barre ONIRIQUE (presets, IN, A/B, licence), la façade de l'appareil dans la DA ONIRIQUE (métal sombre
//  brossé, oreilles de rack vissées, liseré braise), puis la bande du bas (OVERSAMPLING, MIX, réduction).
//    1176   INPUT et OUTPUT en grand, ATTACK et RELEASE empilés, la colonne RATIO, le VU (BLACK / BLUE au-dessus), METER
//    LA-2A  la colonne MODE, GAIN en grand, le VU au centre (le menu MODEL au-dessus), PEAK REDUCTION en grand, METER
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
    ModelMenu model;
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
