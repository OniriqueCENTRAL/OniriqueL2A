// Banc du moteur d'ONIRIQUE 1176 / LA-2A : MIX à 0 et BYPASS rendent le son direct exactement retardé de la latence (aucune
// coloration, en phase), la compression agit et rattrape quelque chose, chaque suréchantillonnage reste stable.
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include "dsp/SoloEngine.h"

namespace
{
int fails = 0;
void check (bool ok, const char* what) { std::printf ("  %s  %s\n", ok ? "ok  " : "ÉCHEC", what); if (! ok) ++fails; }

juce::AudioBuffer<float> voice (double fs, int n)
{
    juce::AudioBuffer<float> b (2, n);
    juce::Random rnd (7);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const float env = 0.25f + 0.75f * (float) (0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1.3 * t));
        const float s = env * (0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t)
                             + 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1760.0 * t) + 0.02f * (rnd.nextFloat() - 0.5f));
        b.setSample (0, i, s); b.setSample (1, i, 0.9f * s);
    }
    return b;
}
} // namespace

int main()
{
    const double fs = 48000.0;
    const int n = (int) fs * 2;
    for (int order = 0; order <= 3; ++order)
    {
        std::printf ("suréchantillonnage x%d\n", 1 << order);
        solo::Engine e;
        e.prepare (fs, order, 2);
        const int lat = e.getLatency();
        const auto in = voice (fs, n);

        // MIX à 0 : le son direct, retardé de la latence, à l'échantillon près
        {
            solo::Params p; p.mix = 0.0f;
            e.prepare (fs, order, 2);
            auto b = in;
            // le MIX glisse en 30 ms depuis 100 % : on mesure après
            juce::AudioBuffer<float> warm (2, 4096); warm.clear();
            e.process (warm, p);
            e.prepare (fs, order, 2);
            solo::Engine z; z.prepare (fs, order, 2);
            // un moteur neuf avec MIX à 0 dès le départ : son lissage part de 1, on compare après 50 ms
            b = in; z.process (b, p);
            double err = 0.0;
            for (int i = (int) (0.05 * fs) + lat; i < n; ++i)
                for (int c = 0; c < 2; ++c) err = std::max (err, (double) std::abs (b.getSample (c, i) - in.getSample (c, i - lat)));
            char t[120]; std::snprintf (t, sizeof t, "MIX 0 : son direct retardé de %d échantillons (écart max %.2e)", lat, err);
            check (err < 1.0e-6, t);
        }
        // BYPASS : pareil
        {
            solo::Engine z; z.prepare (fs, order, 2);
            solo::Params p; p.bypass = true;
            auto b = in; z.process (b, p);
            double err = 0.0;
            for (int i = (int) (0.05 * fs) + lat; i < n; ++i)
                for (int c = 0; c < 2; ++c) err = std::max (err, (double) std::abs (b.getSample (c, i) - in.getSample (c, i - lat)));
            check (err < 1.0e-6, "BYPASS : le son direct, même latence");
        }
        // le son compressé est calé sur le son direct (MIX à mi-chemin : pas de filtre en peigne) : corrélation maximale au décalage 0
        {
            solo::Engine z; z.prepare (fs, order, 2);
            solo::Params p;
#if SOLO_LA2A
            p.peak = 0.0f;
#else
            p.input = -12.0f;
#endif
            auto b = in; z.process (b, p);
            int best = -99; double bestC = -1.0;
            for (int lag = -12; lag <= 12; ++lag)
            {
                double c = 0.0;
                for (int i = (int) (0.2 * fs); i < n - 20; ++i) c += (double) b.getSample (0, i) * in.getSample (0, i - z.getLatency() + lag);
                if (c > bestC) { bestC = c; best = lag; }
            }
            char t[120]; std::snprintf (t, sizeof t, "son compressé calé sur le son direct (décalage trouvé : %d)", best);
            check (best == 0, t);
        }
        // la compression agit
        {
            solo::Engine z; z.prepare (fs, order, 2);
            solo::Params p;
#if SOLO_LA2A
            p.peak = 70.0f;
#else
            p.input = 14.0f; p.ratio = 1;
#endif
            auto b = in;
            z.process (b, p);
            bool finite = true;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) finite &= std::isfinite (b.getSample (c, i));
            char t[120]; std::snprintf (t, sizeof t, "compression : %.1f dB de réduction, sortie finie", std::abs (z.getReductionDb()));
            check (finite && std::abs (z.getReductionDb()) > 1.0f, t);
        }
    }
    std::printf (fails ? "\nÉCHEC : %d\n" : "\nTOUT EST BON\n", fails);
    return fails;
}
