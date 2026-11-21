#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#if SOLO_LA2A
 #include "CompLA2A.h"
#else
 #include "Comp1176.h"
#endif

namespace solo
{
struct Params
{
#if SOLO_LA2A
    float gain = 0.0f, peak = 40.0f;
    bool limit = false;
#else
    float input = 0.0f, output = 0.0f, attack = 3.0f, release = 5.0f;
    int ratio = 0, revision = 0;
#endif
    float mix = 1.0f;
    bool bypass = false;
};

/*  Le moteur : le compresseur de VOCALBOX (Comp1176.h ou CompLA2A.h, sans changement), dans le même suréchantillonnage que
    VOCALBOX (demi-bande FIR équiondulation, 0,05 de transition au premier étage, 0,20 aux suivants, -110 dB ; latence non
    entière complétée au rythme suréchantillonné pour tomber sur un nombre entier d'échantillons de l'hôte).
    MIX : le son direct passe par une ligne de retard de la même latence, puis se mélange : en phase à toutes les valeurs.
    BYPASS : fondu de 20 ms vers le son direct retardé (la latence reste la même, rien ne saute dans le DAW). */
class Engine
{
public:
    static constexpr int kChunk = 512;

    void prepare (double sampleRate, int oversamplingOrder, int channels)
    {
        fs = sampleRate;
        numCh = juce::jlimit (1, 2, channels);
        order = juce::jlimit (0, 3, oversamplingOrder);
        factor = 1 << order;
        os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh);
        if (order == 0) os->addDummyOversamplingStage();
        for (int st = 0; st < order; ++st)
            os->addOversamplingStage (juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                      st == 0 ? 0.05f : 0.20f, -110.0f, st == 0 ? 0.05f : 0.20f, -110.0f);
        os->setUsingIntegerLatency (false);
        os->initProcessing ((size_t) kChunk);
        const int latOS = juce::roundToInt (os->getLatencyInSamples() * factor);
        pad = (factor - latOS % factor) % factor;
        latency = (latOS + pad) / factor;
        for (auto& d : padLine) d.assign ((size_t) std::max (1, pad), 0.0f);   // retard de pad échantillons exactement
        padPos = 0;
        comp.prepare (fs * factor);
        dry.setSize (2, kChunk);
        for (auto& d : dryLine) d.assign ((size_t) (latency + kChunk + 1), 0.0f);
        dryPos = 0;
        mixS.reset (fs, 0.03); mixS.setCurrentAndTargetValue (1.0f);
        byS.reset (fs, 0.02); byS.setCurrentAndTargetValue (0.0f);
        outPeak.store (0.0f);
    }

    int getLatency() const noexcept { return latency; }
    float getReductionDb() const noexcept { return comp.getGainReductionDb(); }
    /** la crête de sortie depuis la dernière lecture (VU en +8 / +4 / +10) */
    float consumeOutputPeak() noexcept { return outPeak.exchange (0.0f); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
#if SOLO_LA2A
        comp.setParams (p.gain, juce::jlimit (0.0f, 100.0f, p.peak), p.limit, false);
#else
        comp.setRevision (p.revision);
        comp.setParams (juce::jlimit (-12.0f, 36.0f, p.input), p.output, p.attack, p.release, p.ratio, false);
#endif
        mixS.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mix));
        byS.setTargetValue (p.bypass ? 1.0f : 0.0f);
        const int total = buffer.getNumSamples(), ch = std::min (numCh, buffer.getNumChannels());
        for (int start = 0; start < total; start += kChunk)
        {
            const int n = std::min (kChunk, total - start);
            float* data[2] { buffer.getWritePointer (0, start), ch > 1 ? buffer.getWritePointer (1, start) : nullptr };
            for (int c = 0; c < ch; ++c) dry.copyFrom (c, 0, data[c], n);
            // le compresseur, au rythme suréchantillonné
            juce::dsp::AudioBlock<float> block (data, (size_t) ch, (size_t) n);
            auto up = os->processSamplesUp (block);
            const int nUp = (int) up.getNumSamples();
            float* upData[2] { up.getChannelPointer (0), ch > 1 ? up.getChannelPointer (1) : nullptr };
            comp.process (upData, ch, nUp);
            // la latence non entière complétée (pad échantillons au rythme suréchantillonné)
            if (pad > 0)
                for (int i = 0; i < nUp; ++i)
                {
                    for (int c = 0; c < ch; ++c)
                    {
                        auto& line = padLine[(size_t) c];
                        const float x = upData[c][i];
                        upData[c][i] = line[(size_t) padPos];
                        line[(size_t) padPos] = x;
                    }
                    padPos = (padPos + 1) % pad;
                }
            os->processSamplesDown (block);
            // le son direct retardé, le MIX et le BYPASS
            const int L = (int) dryLine[0].size();
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = mixS.getNextValue(), by = byS.getNextValue();
                for (int c = 0; c < ch; ++c)
                {
                    auto& line = dryLine[(size_t) c];
                    line[(size_t) dryPos] = dry.getSample (c, i);
                    const float d = line[(size_t) ((dryPos - latency + L) % L)];
                    float y = d + m * (data[c][i] - d);
                    y = y + by * (d - y);
                    data[c][i] = y;
                    pk = std::max (pk, std::abs (y));
                }
                dryPos = (dryPos + 1) % L;
            }
            if (pk > outPeak.load (std::memory_order_relaxed)) outPeak.store (pk, std::memory_order_relaxed);
        }
    }

private:
#if SOLO_LA2A
    onirique::OptoCompressor comp;
#else
    onirique::FetCompressor comp;
#endif
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    double fs = 48000.0;
    int numCh = 2, order = 1, factor = 2, pad = 0, latency = 0, padPos = 0, dryPos = 0;
    std::array<std::vector<float>, 2> padLine, dryLine;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> mixS { 1.0f }, byS { 0.0f };
    std::atomic<float> outPeak { 0.0f };
};
} // namespace solo
