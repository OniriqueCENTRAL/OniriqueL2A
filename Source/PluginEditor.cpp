#include "PluginEditor.h"
#include "OniriqueWindowSize.h"
#include "BinaryData.h"

namespace
{
const juce::String pluginName = SOLO_NAME;
using R = juce::Rectangle<float>;
juce::String fr (const char* s) { return juce::String::fromUTF8 (s); }

namespace layout
{
const R face { 20, 70, 1140, 470 };      // la façade de l'appareil
const R strip { 20, 552, 1140, 72 };     // la bande du bas
#if SOLO_LA2A
const R vuR { 430, 150, 320, 195 };
const juce::Point<float> gainC { 297, 300 }, peakC { 883, 300 };
const R modeR { 72, 190, 96, 170 }, meterR { 1016, 170, 96, 160 };
#else
const R vuR { 690, 158, 284, 173 };
const juce::Point<float> inC { 168, 300 }, outC { 358, 300 }, atkC { 520, 222 }, relC { 520, 382 };
const R ratioR { 586, 136, 74, 300 }, meterR { 1012, 172, 96, 220 };
#endif
}
} // namespace

SoloEditor::SoloEditor (SoloProcessor& proc)
    : AudioProcessorEditor (proc), processor (proc),
#if SOLO_LA2A
      gain (p (params::gain), "GAIN", knobLook), peak (p (params::peak), "PEAK REDUCTION", knobLook),
      mode (p (params::limit), { "COMPRESS", "LIMIT" }, true),
#else
      input (p (params::input), "INPUT", knobLook, true), output (p (params::output), "OUTPUT", knobLook, true),
      attack (p (params::attack), "ATTACK", knobLook), release (p (params::release), "RELEASE", knobLook),
      ratio (p (params::ratio), { "4", "8", "12", "20", "ALL" }, true),
      revision (p (params::revision), { "BLACK", "BLUE" }),
#endif
      meter (p (params::meter), params::meterNames, false),
      vu ([this] { return vuValue(); }),
      oversampling (p (params::oversampling), { "OFF", "2x", "4x", "8x" }),
      mix (p (params::mix), "MIX", knobLook),
      bar (proc.presets, proc.apvts, SOLO_LA2A ? "LA-2A" : "1176", {}, baseWidth, baseWidth)
{
    onirique::look::load();
    setLookAndFeel (&menuLook);
    bar.onThemeChanged = [this]
    {
        renderBackground();
        std::function<void (juce::Component&)> all = [&all] (juce::Component& k) { k.repaint(); for (auto* ch : k.getChildren()) all (*ch); };
        all (*this);
    };
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource ("logo_png", size))
            logo = juce::ImageFileFormat::loadFrom (data, (size_t) size);
    }
#if SOLO_LA2A
    for (juce::Component* c : std::initializer_list<juce::Component*> { &gain, &peak, &mode })
        addAndMakeVisible (c);
    gain.setTip (fr ("GAIN : le niveau de sortie (le rattrapage de la compression)."));
    peak.setTip (fr ("PEAK REDUCTION : la quantité de compression. Plus haut, la cellule optique réduit davantage : douce, lente, musicale."));
    mode.setTooltip (fr ("MODE : COMPRESS, une compression douce ; LIMIT, un ratio plus fort, qui tient davantage les crêtes."));
#else
    for (juce::Component* c : std::initializer_list<juce::Component*> { &input, &output, &attack, &release, &ratio, &revision })
        addAndMakeVisible (c);
    input.setTip (fr ("INPUT : on pousse le signal dans le 1176 (le seuil est fixe, comme sur l'appareil). Plus haut, plus de compression."));
    output.setTip (fr ("OUTPUT : le niveau de sortie."));
    attack.setTip (fr ("ATTACK : de 800 µs (tout à gauche) à 20 µs (tout à droite), comme sur l'appareil : à droite, c'est plus rapide."));
    release.setTip (fr ("RELEASE : de 1,1 s (tout à gauche) à 50 ms (tout à droite)."));
    ratio.setTooltip (fr ("RATIO : 4, 8, 12, 20, ou ALL (tous les boutons enfoncés : le grain et l'écrasement du « all buttons »)."));
    revision.setTooltip (fr ("BLACK : le 1176LN, propre et serré. BLUE : la première génération, plus colorée, plus de grain, des médiums en avant."));
#endif
    for (juce::Component* c : std::initializer_list<juce::Component*> { &meter, &vu, &oversampling, &mix })
        addAndMakeVisible (c);
    meter.setTooltip (fr ("METER : ce que montre le VU. GR : la réduction de gain ; +8, +4 (ou +10, +4) : le niveau de sortie."));
    oversampling.setTooltip (fr ("OVERSAMPLING : le compresseur tourne 2, 4 ou 8 fois plus vite (moins de repliement dans les aigus). "
                                 "La latence est déclarée au DAW."));
    mix.setTip (fr ("MIX : le son direct mêlé au son compressé (compression parallèle), en phase à toutes les valeurs."));

    addAndMakeVisible (bar);
    bar.setBrand (300, [this] (juce::Graphics& g, juce::Rectangle<float> area) { paintTitle (g, area); });
    addMouseListener (this, true);

#if SOLO_LICENCE
    licenseOverlay = std::make_unique<onirique::license::Overlay> (pluginName, processor.license);
    addAndMakeVisible (*licenseOverlay);
   #if ONIRIQUE_UI_TEST
    if (juce::SystemStats::getEnvironmentVariable ("ONIRIQUE_UI_TEST", {}).contains ("nolicense"))
        licenseOverlay->setVisible (false);
   #endif
#endif

    auto heightFor = [] (int w) { return juce::roundToInt ((float) w * (float) baseHeight / (float) baseWidth); };
    const int minW = juce::roundToInt (baseWidth * 0.6f), maxW = baseWidth * 2;
    const auto size = onirique::window::openingSize (processor.apvts.state, pluginName, { baseWidth, heightFor (baseWidth) },
                                                     { minW, heightFor (minW) }, { maxW, heightFor (maxW) });
    setResizable (true, true);
    setResizeLimits (minW, heightFor (minW), maxW, heightFor (maxW));
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (size.x, heightFor (size.x));
    sizeReady = true;
    onirique::window::remember (processor.apvts.state, getWidth(), getHeight());
    startTimerHz (60);

    if (auto* png = std::getenv ("ONIRIQUE_UI_PNG"))
    {
        const juce::String path (png);
        juce::Component::SafePointer<SoloEditor> safe (this);
        juce::Timer::callAfterDelay (2500, [safe, path]
        {
            if (safe == nullptr) return;
            juce::Image img (juce::Image::ARGB, safe->getWidth() * 2, safe->getHeight() * 2, true, juce::SoftwareImageType());
            { juce::Graphics g (img); g.addTransform (juce::AffineTransform::scale (2.0f)); safe->paintEntireComponent (g, true); }
            juce::File f (path); f.deleteFile();
            if (auto os = f.createOutputStream()) juce::PNGImageFormat().writeImageToStream (img, *os);
        });
    }
}

SoloEditor::~SoloEditor()
{
    stopTimer();
    juce::PopupMenu::dismissAllActiveMenus();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
    onirique::window::rememberOnThisComputer (pluginName, getWidth(), getHeight());
}

// ce que montre le VU : la réduction de gain, ou le niveau de sortie lu 8 / 4 (ou 10 / 4) dB plus bas, ou rien (OFF)
float SoloEditor::vuValue()
{
    const int m = juce::roundToInt (p (params::meter).convertFrom0to1 (p (params::meter).getValue()));
    if (m == 0) return -std::abs (processor.getReductionDb());
#if ! SOLO_LA2A
    if (m == 3) return -30.0f;
#endif
    const float ref = SOLO_LA2A ? (m == 1 ? 10.0f : 4.0f) : (m == 1 ? 8.0f : 4.0f);
    return outDb + 18.0f - ref;
}

void SoloEditor::timerCallback()
{
    if (onirique::look::changedSince (themeSeen) && bar.onThemeChanged) bar.onThemeChanged();
    // le niveau de sortie (crête, retombée douce) pour le VU en +8 / +4
    const float pk = processor.consumeOutputPeak();
    const float db = pk > 1.0e-6f ? 20.0f * std::log10 (pk) : -100.0f;
    outDb = db > outDb ? db : outDb - 0.6f;
    vu.tick();
    // la réduction écrite dans la bande du bas
    const float gr = std::abs (processor.getReductionDb());
    if (std::abs (gr - grShown) > 0.05f) { grShown += 0.35f * (gr - grShown); repaint (juce::Rectangle<float> (layout::strip.getRight() - 260, layout::strip.getY(), 260, layout::strip.getHeight())
                                                                                     .transformedBy (juce::AffineTransform::scale (scale())).toNearestInt()); }
}

//==============================================================================
void SoloEditor::resized()
{
    const float s = scale();
    auto place = [s] (juce::Component& c, R r) { c.setBounds (R (r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s).toNearestInt()); };
    auto knob = [&] (ui::Knob& k, juce::Point<float> c, juce::Point<float> sz) { place (k, R (sz.x, sz.y).withCentre (c)); k.setScale (s); };

#if SOLO_LA2A
    knob (gain, layout::gainC, { 186, 236 });
    knob (peak, layout::peakC, { 186, 236 });
    place (mode, layout::modeR); mode.setScale (s);
#else
    knob (input, layout::inC, { 176, 220 });
    knob (output, layout::outC, { 176, 220 });
    knob (attack, layout::atkC, { 110, 140 });
    knob (release, layout::relC, { 110, 140 });
    place (ratio, layout::ratioR); ratio.setScale (s);
    place (revision, R (180, 30).withCentre ({ layout::vuR.getCentreX(), layout::vuR.getY() - 34 })); revision.setScale (s);
#endif
    place (meter, layout::meterR); meter.setScale (s);
    place (vu, layout::vuR); vu.unit = s;
    // la bande du bas : OVERSAMPLING, MIX ; la réduction à droite (dessinée)
    place (oversampling, R (layout::strip.getX() + 170, layout::strip.getCentreY() - 15, 260, 30)); oversampling.setScale (s);
    knob (mix, { layout::strip.getX() + 620, layout::strip.getCentreY() - 2 }, { 150, 66 });

#if SOLO_LICENCE
    const int pillW = licenseOverlay != nullptr ? licenseOverlay->pillWidth() : 0;
#else
    const int pillW = 0;
#endif
    bar.setPillWidth (pillW);
    bar.place (s, getLocalBounds());
#if SOLO_LICENCE
    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), s, bar.getPillArea(), juce::Justification::centredRight);
#endif
    renderBackground();
    if (sizeReady) onirique::window::remember (processor.apvts.state, getWidth(), getHeight());
}

void SoloEditor::renderBackground()
{
    const float k = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    background = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * k)),
                              juce::jmax (1, juce::roundToInt ((float) getHeight() * k)), true);
    juce::Graphics g (background);
    g.drawImageAt (onirique::da::backdrop (background.getWidth(), background.getHeight(), scale() * k), 0, 0);
    g.addTransform (juce::AffineTransform::scale (scale() * k));
    using namespace ui;

    // ---- la façade : métal sombre brossé, liseré braise, oreilles de rack vissées
    const auto f = layout::face;
    onirique::da::softGlowRect (g, f, 18.0f, juce::Colours::black.withAlpha (0.6f), 26.0f, 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff221b1d), 0.0f, f.getY(), juce::Colour (0xff0f0b0c), 0.0f, f.getBottom(), false));
    g.fillRoundedRectangle (f, 18.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (f.toNearestInt());
        juce::Random rnd (1176);
        for (float y = f.getY() + 1.5f; y < f.getBottom(); y += 2.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.008f + 0.016f * rnd.nextFloat()));
            g.drawHorizontalLine (juce::roundToInt (y), f.getX(), f.getRight());
        }
        // un reflet en haut
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0.0f, f.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, f.getY() + 90.0f, false));
        g.fillRect (f.withHeight (90.0f));
    }
    g.setColour (accent.withAlpha (0.45f));
    g.drawRoundedRectangle (f.reduced (0.5f), 18.0f, 1.2f);
    for (const auto& ear : { R (f.getX(), f.getY(), 44, f.getHeight()), R (f.getRight() - 44, f.getY(), 44, f.getHeight()) })
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (ear.reduced (6, 10), 10.0f);
        for (float yy : { ear.getY() + 34.0f, ear.getBottom() - 34.0f })
        {
            const auto sc = R (16, 16).withCentre ({ ear.getCentreX(), yy });
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff57494d), sc.getX(), sc.getY(), juce::Colour (0xff181314), sc.getRight(), sc.getBottom(), false));
            g.fillEllipse (sc);
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.drawLine (sc.getX() + 4, sc.getCentreY() - 2, sc.getRight() - 4, sc.getCentreY() + 2, 1.6f);
        }
    }
    // la plaque du modèle, sous le VU
    {
        const auto v = layout::vuR;
        const float pw = std::min (300.0f, v.getWidth() - 10.0f);
        const R plate (v.getCentreX() - pw * 0.5f, v.getBottom() + 22, pw, 74);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (plate, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (plate, 10.0f, 1.0f);
        text (g, "ONIRIQUE", plate.withHeight (22).translated (0, 6), font (Face::display, 10.0f, 0.3f), bone.withAlpha (0.6f));
        text (g, SOLO_LA2A ? "LA-2A" : "1176", plate.withHeight (30).translated (0, 24), font (Face::display, 28.0f, 0.06f), accent);
        text (g, SOLO_LA2A ? "OPTICAL LEVELING AMPLIFIER" : "FET LIMITING AMPLIFIER", plate.withHeight (14).translated (0, 56),
              font (Face::mono, 8.5f, 0.2f), haze);
    }
    // les noms des colonnes et les séparations
    const auto cap = font (Face::display, 11.0f, 0.16f);
#if SOLO_LA2A
    text (g, "MODE", R (layout::modeR.getX(), layout::modeR.getY() - 28, layout::modeR.getWidth(), 20), cap, bone);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float x : { 186.0f, 410.0f, 770.0f, 998.0f }) g.fillRect (x, f.getY() + 34, 1.0f, f.getHeight() - 68);
#else
    text (g, "RATIO", R (layout::ratioR.getX(), layout::ratioR.getY() - 26, layout::ratioR.getWidth(), 20), cap, bone);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float x : { 263.0f, 450.0f, 574.0f, 672.0f, 994.0f }) g.fillRect (x, f.getY() + 34, 1.0f, f.getHeight() - 68);
#endif
    text (g, "METER", R (layout::meterR.getX(), layout::meterR.getY() - 28, layout::meterR.getWidth(), 20), cap, bone);

    // ---- la bande du bas (verre) : OVERSAMPLING, MIX ; la réduction (dessinée en direct)
    onirique::da::glass (g, layout::strip, 16.0f, 1.0f);
    text (g, "OVERSAMPLING", R (layout::strip.getX() + 24, layout::strip.getY(), 140, layout::strip.getHeight()), font (Face::display, 10.0f, 0.16f), bone,
          juce::Justification::centredLeft);
}

void SoloEditor::paintTitle (juce::Graphics& g, juce::Rectangle<float> area) const
{
    using namespace ui;
    const float cy = 25.0f;
    float x = area.getX();
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (logo.isValid()) g.drawImage (logo, R (x, cy - 13, 28, 26), juce::RectanglePlacement::centred);
    x += 38;
    text (g, "ONIRIQUE", R (x, cy - 16, 120, 14), font (Face::display, 11.0f, 0.14f), bone.withAlpha (0.75f), juce::Justification::centredLeft);
    const juce::String name = SOLO_LA2A ? "LA-2A" : "1176";
    text (g, name, R (x - 1, cy - 4, 160, 22), font (Face::display, 20.0f, 0.1f), accent, juce::Justification::centredLeft);
    const float w = juce::GlyphArrangement::getStringWidth (font (Face::display, 20.0f, 0.1f), name);
    text (g, SOLO_LA2A ? "OPTICAL LEVELER" : "FET LIMITER", R (x + w + 10, cy + 1, 160, 14), font (Face::body, 8.5f, 0.14f), dim, juce::Justification::centredLeft);
}

void SoloEditor::paint (juce::Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat());
    else g.fillAll (ui::ink);
    // la réduction de gain, à droite de la bande du bas
    using namespace ui;
    g.addTransform (juce::AffineTransform::scale (scale()));
    const auto st = layout::strip;
    const R box (st.getRight() - 250, st.getY() + 14, 226, st.getHeight() - 28);
    text (g, fr ("RÉDUCTION"), box.withHeight (16), font (Face::display, 10.0f, 0.16f), bone, juce::Justification::centredLeft);
    text (g, (grShown > 0.05f ? "-" : "") + juce::String (grShown, 1) + " dB", box.withHeight (16), font (Face::mono, 12.0f), bone, juce::Justification::centredRight);
    const R track (box.getX(), box.getBottom() - 10, box.getWidth(), 8);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (track, 4.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (track.withWidth (std::max (8.0f, track.getWidth() * juce::jlimit (0.0f, 1.0f, grShown / 20.0f))), 4.0f);
}

