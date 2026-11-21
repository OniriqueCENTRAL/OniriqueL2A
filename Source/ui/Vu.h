#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Theme.h"
#include "OniriqueDA.h"

//==============================================================================
//  Le VU-mètre de VOCALBOX (1176 et LA-2A) : un cadran éclairé de l'intérieur, une aiguille qui projette son ombre, un verre
//  bombé, le voyant PEAK au-delà de 0 VU. Repris tel quel ; tick() à appeler à chaque image (60 Hz).
//==============================================================================
namespace vuh
{
constexpr float kPi = juce::MathConstants<float>::pi;
inline bool approach (float& v, float target, float k) noexcept
{
    if (std::abs (target - v) < 1.0e-4f) { if (v == target) return false; v = target; return true; }
    v += k * (target - v);
    return true;
}
inline juce::Image gpu (const juce::Image& img) { return juce::NativeImageType().convert (img); }
inline void glowEllipse (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radius)
{
    juce::Path p; p.addEllipse (r); onirique::da::softGlow (g, p, c, radius);
}
} // namespace vuh

namespace ui
{
using vuh::kPi;
class VintageVu : public juce::Component
{
public:
    explicit VintageVu (std::function<float()> vuDb) : read (std::move (vuDb)) { setInterceptsMouseClicks (false, false); }

    float unit = 1.0f;   // l'échelle de l'interface (le cadre et sa lueur)
    void tick()
    {
        bool moved = vuh::approach (angle, angleFor (read ? read() : -30.0f), 0.16f);
        // le voyant PEAK : au-delà de 0 VU (niveau de sortie), il s'allume vite et s'éteint doucement
        moved |= vuh::approach (peak, angle > 21.0f ? 1.0f : 0.0f, angle > 21.0f ? 0.5f : 0.06f);
        if (moved) repaint();
    }

    void resized() override { baseCache = {}; glassCache = {}; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float pad = 22.0f * unit;
        const float s = std::min ((b.getWidth() - 2.0f * pad) / 320.0f, (b.getHeight() - 2.0f * pad) / 195.0f);
        if (s <= 0.0f) return;
        const auto frame = juce::Rectangle<float> (320.0f * s, 195.0f * s).withCentre (b.getCentre());
        const auto face = frame.reduced (13.0f * s);
        const float k = face.getWidth() / 300.0f;
        // le décor ne change qu'avec la taille : dessiné une fois, l'aiguille et le voyant bougent entre les deux couches
        if (! baseCache.isValid() || baseCache.getWidth() != getWidth() || baseCache.getHeight() != getHeight())
        {
            juce::Image base (juce::Image::ARGB, std::max (1, getWidth()), std::max (1, getHeight()), true, juce::SoftwareImageType());
            juce::Image glass (juce::Image::ARGB, std::max (1, getWidth()), std::max (1, getHeight()), true, juce::SoftwareImageType());
            { juce::Graphics cg (base); paintBase (cg, frame, face, s, k, pad); }
            { juce::Graphics cg (glass); paintGlass (cg, face, k); }
            baseCache = vuh::gpu (base);
            glassCache = vuh::gpu (glass);
        }
        g.drawImageAt (baseCache, 0, 0);

        auto P = [&] (float x, float y) { return juce::Point<float> (face.getX() + x * k, face.getY() + y * k); };
        auto polar = [&] (float r, float deg) { const float a = deg * kPi / 180.0f; return P (150.0f + r * std::sin (a), 165.0f - r * std::cos (a)); };
        {
            juce::Graphics::ScopedSaveState st (g);
            juce::Path clip; clip.addRoundedRectangle (face, 9.0f * k);
            g.reduceClipRegion (clip);
            // le voyant PEAK, en haut à droite du cadran
            const auto led = juce::Rectangle<float> (9.0f * k, 9.0f * k).withCentre (P (276, 20));
            if (peak > 0.01f)
                vuh::glowEllipse (g, led.expanded (2.0f * k), juce::Colour (0xffff3b2a).withAlpha (0.65f * peak), 9.0f * k);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a1410).interpolatedWith (juce::Colour (0xffff6a50), peak), led.getCentreX(), led.getY(),
                                                     juce::Colour (0xff230806).interpolatedWith (juce::Colour (0xffd8241a), peak), led.getCentreX(), led.getBottom(), false));
            g.fillEllipse (led);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (led, 0.8f * k);

            // l'aiguille effilée et son ombre portée sur le cadran
            const auto pivot = P (150, 165);
            auto needle = [&] (juce::Point<float> off)
            {
                const auto tip = polar (117.0f, angle) + off, base = pivot + off;
                const auto dir = (tip - base) / std::max (1.0e-3f, tip.getDistanceFrom (base));
                const juce::Point<float> nrm (-dir.y, dir.x);
                juce::Path p;
                p.startNewSubPath (base + nrm * (1.6f * k) - dir * (14.0f * k));
                p.lineTo (tip + nrm * (0.35f * k));
                p.lineTo (tip - nrm * (0.35f * k));
                p.lineTo (base - nrm * (1.6f * k) - dir * (14.0f * k));
                p.closeSubPath();
                return p;
            };
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            g.fillPath (needle ({ 3.0f * k, 4.0f * k }));
            g.setColour (juce::Colours::black.withAlpha (0.08f));
            g.fillPath (needle ({ 5.0f * k, 7.0f * k }));
            g.setColour (juce::Colour (0xff141010));
            g.fillPath (needle ({}));

            // le capot noir du pivot, bombé, en bas du cadran
            const auto hood = juce::Rectangle<float> (92.0f * k, 52.0f * k).withCentre (P (150, 184));
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillEllipse (hood.expanded (3.0f * k).translated (0.0f, -1.0f * k));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2729), hood.getCentreX(), hood.getY(), juce::Colour (0xff0b0a0b), hood.getCentreX(), hood.getY() + 22.0f * k, false));
            g.fillEllipse (hood);
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            juce::Path rim; rim.addCentredArc (hood.getCentreX(), hood.getCentreY(), hood.getWidth() * 0.5f - 1.0f * k, hood.getHeight() * 0.5f - 1.0f * k, 0.0f, -1.1f, 1.1f, true);
            g.strokePath (rim, juce::PathStrokeType (1.0f * k));
            // la vis du pivot
            const auto screw = juce::Rectangle<float> (8.0f * k, 8.0f * k).withCentre (P (150, 171));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa9abb2), screw.getX(), screw.getY(), juce::Colour (0xff3d3e43), screw.getRight(), screw.getBottom(), false));
            g.fillEllipse (screw);
            g.setColour (juce::Colour (0xff1a1a1c));
            g.drawLine ({ screw.getX() + 1.5f * k, screw.getCentreY(), screw.getRight() - 1.5f * k, screw.getCentreY() }, 1.0f * k);
        }
        g.drawImageAt (glassCache, 0, 0);
    }

private:
    void paintBase (juce::Graphics& g, juce::Rectangle<float> frame, juce::Rectangle<float> face, float s, float k, float pad)
    {
        // la lueur braise autour du boîtier
        onirique::da::softGlowRect (g, frame, 16.0f * s, onirique::look::ember.withAlpha (0.30f), pad * 0.9f);
        // le boîtier : noir, un bord à peine plus clair (pas de liseré blanc)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1719), 0.0f, frame.getY(), juce::Colour (0xff060506), 0.0f, frame.getBottom(), false));
        g.fillRoundedRectangle (frame, 16.0f * s);
        g.setColour (juce::Colour (0xff2a2427));
        g.drawRoundedRectangle (frame.reduced (0.5f * s), 16.0f * s, 1.0f * s);
        // la fenêtre creusée : un rebord sombre autour du cadran
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (face.expanded (3.0f * k), 11.0f * k);

        auto P = [&] (float x, float y) { return juce::Point<float> (face.getX() + x * k, face.getY() + y * k); };
        auto polar = [&] (float r, float deg) { const float a = deg * kPi / 180.0f; return P (150.0f + r * std::sin (a), 165.0f - r * std::cos (a)); };

        juce::Graphics::ScopedSaveState st (g);
        juce::Path clip; clip.addRoundedRectangle (face, 9.0f * k);
        g.reduceClipRegion (clip);
        // le cadran crème, éclairé de l'intérieur : une lampe chaude derrière l'échelle, les bords plus sombres
        juce::ColourGradient fg (juce::Colour (0xfffff3cf), P (150, 120).x, P (150, 120).y, juce::Colour (0xffb98a44), P (150, 120).x + 230.0f * k, P (150, 120).y, true);
        fg.addColour (0.45, juce::Colour (0xfff3d79c));
        fg.addColour (0.8, juce::Colour (0xffd9ae68));
        g.setGradientFill (fg);
        g.fillRect (face);
        juce::ColourGradient lamp (juce::Colour (0xffffe9b0).withAlpha (0.55f), P (150, 95).x, P (150, 95).y, juce::Colour (0xffffe9b0).withAlpha (0.0f), P (150, 95).x + 120.0f * k, P (150, 95).y, true);
        g.setGradientFill (lamp);
        g.fillRect (face);
        // une pointe de braise dans les coins du bas, pour la DA
        for (const float x : { 0.0f, 300.0f })
        {
            juce::ColourGradient e (onirique::look::ember.withAlpha (0.16f), P (x, 175).x, P (x, 175).y, onirique::look::ember.withAlpha (0.0f), P (x, 175).x + 90.0f * k, P (x, 175).y, true);
            g.setGradientFill (e);
            g.fillRect (face);
        }

        const juce::Colour ink (0xff2a2016), red (0xffc0301f), soft (0xff6a5236);
        auto arc = [&] (float r, float a0, float a1, juce::Colour c, float w)
        {
            juce::Path p;
            for (int i = 0; i <= 48; ++i) { const auto q = polar (r, a0 + (a1 - a0) * (float) i / 48.0f); if (i == 0) p.startNewSubPath (q); else p.lineTo (q); }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (w * k, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        };
        auto tickAt = [&] (float r1, float r2, float a, juce::Colour c, float w) { g.setColour (c); g.drawLine ({ polar (r1, a), polar (r2, a) }, w * k); };

        // la zone rouge pleine (0 .. +3), puis les deux arcs de l'échelle
        {
            juce::Path band;
            for (int i = 0; i <= 32; ++i) { const auto q = polar (124.0f, 20.0f + 28.0f * (float) i / 32.0f); if (i == 0) band.startNewSubPath (q); else band.lineTo (q); }
            for (int i = 32; i >= 0; --i) band.lineTo (polar (118.0f, 20.0f + 28.0f * (float) i / 32.0f));
            band.closeSubPath();
            g.setColour (red.withAlpha (0.85f));
            g.fillPath (band);
        }
        arc (118.0f, -48.0f, 48.0f, ink, 1.3f);
        arc (124.0f, -48.0f, 20.0f, ink.withAlpha (0.55f), 0.6f);
        // les graduations fines : chaque dB jusqu'à -3, chaque demi-dB au-dessus
        for (float db = -20.0f; db <= 3.001f; db += (db < -3.0f ? 1.0f : 0.5f))
        {
            const float a = angleFor (db);
            tickAt (118.0f, 123.0f, a, db >= 0.0f ? ink.withAlpha (0.8f) : ink, 0.7f);
        }
        const std::pair<const char*, float> major[] { { "20", -20.0f }, { "10", -10.0f }, { "7", -7.0f }, { "5", -5.0f }, { "3", -3.0f }, { "2", -2.0f },
                                                      { "1", -1.0f }, { "0", 0.0f }, { "+1", 1.0f }, { "+2", 2.0f }, { "+3", 3.0f } };
        g.setFont (ui::font (ui::Face::body, 11.5f * k));
        for (const auto& [l, db] : major)
        {
            const float a = angleFor (db);
            const auto c = db >= 0.0f ? red : ink;
            tickAt (114.0f, 130.0f, a, db >= 0.0f ? ink : c, 1.5f);
            g.setColour (c);
            g.drawText (l, juce::Rectangle<float> (30.0f * k, 14.0f * k).withCentre (polar (140.0f, a)), juce::Justification::centred, false);
        }
        // l'échelle en pourcentage, en dessous
        arc (104.0f, -48.0f, 20.0f, soft, 0.6f);
        g.setFont (ui::font (ui::Face::mono, 6.5f * k));
        for (const auto& [l, a] : std::initializer_list<std::pair<const char*, float>> { { "0", -48.0f }, { "20", -30.0f }, { "40", -14.0f }, { "60", 0.0f }, { "80", 10.0f }, { "100", 20.0f } })
        {
            tickAt (101.0f, 107.0f, a, soft, 0.8f);
            g.setColour (soft);
            g.drawText (l, juce::Rectangle<float> (24.0f * k, 10.0f * k).withCentre (polar (93.0f, a)), juce::Justification::centred, false);
        }
        // VU en grand, et ce que l'aiguille lit
        g.setColour (ink.withAlpha (0.88f));
        g.setFont (ui::font (ui::Face::display, 24.0f * k, 0.08f));
        g.drawText ("VU", juce::Rectangle<float> (80.0f * k, 26.0f * k).withCentre (P (150, 102)), juce::Justification::centred, false);
        g.setColour (soft);
        g.setFont (ui::font (ui::Face::mono, 6.5f * k, 0.25f));
        g.drawText ("GAIN REDUCTION", juce::Rectangle<float> (200.0f * k, 10.0f * k).withCentre (P (150, 122)), juce::Justification::centred, false);
        g.drawText ("PEAK", juce::Rectangle<float> (40.0f * k, 9.0f * k).withCentre (P (276, 32)), juce::Justification::centred, false);
    }

    // le verre bombé : un reflet doux en haut, l'ombre des bords de la fenêtre
    void paintGlass (juce::Graphics& g, juce::Rectangle<float> face, float k)
    {
        juce::Graphics::ScopedSaveState st (g);
        juce::Path clip; clip.addRoundedRectangle (face, 9.0f * k);
        g.reduceClipRegion (clip);
        const auto sheen = juce::Rectangle<float> (face.getWidth() * 1.5f, face.getHeight() * 1.1f).withCentre ({ face.getCentreX() - face.getWidth() * 0.12f, face.getY() - face.getHeight() * 0.18f });
        juce::ColourGradient sg (juce::Colours::white.withAlpha (0.30f), 0.0f, face.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, sheen.getBottom(), false);
        g.setGradientFill (sg);
        g.fillEllipse (sheen);
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.14f - 0.03f * (float) i));
            g.drawRoundedRectangle (face.reduced ((1.0f + 2.0f * (float) i) * k), 9.0f * k, 2.5f * k);
        }
        juce::ColourGradient low (juce::Colours::black.withAlpha (0.0f), 0.0f, face.getY() + face.getHeight() * 0.6f, juce::Colours::black.withAlpha (0.18f), 0.0f, face.getBottom(), false);
        g.setGradientFill (low);
        g.fillRect (face);
    }

    // dB de VU (-20 .. +3) -> angle de l'aiguille sur l'échelle (0 VU à +20°)
    static float angleFor (float vu)
    {
        static const float db[] { -30.0f, -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 6.0f };
        static const float an[] { -52.0f, -48.0f, -30.0f, -20.0f, -11.0f, 0.0f, 6.0f, 12.0f, 20.0f, 28.0f, 37.0f, 48.0f, 52.0f };
        vu = juce::jlimit (-30.0f, 6.0f, vu);
        for (int i = 0; i < 12; ++i)
            if (vu <= db[i + 1]) return an[i] + (an[i + 1] - an[i]) * (vu - db[i]) / (db[i + 1] - db[i]);
        return 52.0f;
    }
    std::function<float()> read;
    float angle = 20.0f, peak = 0.0f;
    juce::Image baseCache, glassCache;
};

// la compression en direct (bas des façades) : la voix qui entre (pâle), qui sort (blanche), et la réduction en braise
} // namespace ui
