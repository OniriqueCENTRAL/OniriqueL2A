#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
//  Les réglages d'ONIRIQUE 1176 et d'ONIRIQUE LA-2A (le même code : SOLO_LA2A choisit l'appareil). Les plages et les
//  valeurs d'usine sont celles des modules 1176 FET et LA-2A OPTO de VOCALBOX : le même moteur, le même son.
//==============================================================================
namespace params
{
// 1176
inline const juce::String input = "input", output = "output", attack = "attack", release = "release", ratio = "ratio",
                          revision = "revision";
// LA-2A
inline const juce::String gain = "gain", peak = "peakReduction", limit = "limit", model = "model";
// les deux
inline const juce::String meter = "meter", oversampling = "oversampling", mix = "mix", bypass = "bypass";

#if SOLO_LA2A
// les modèles de l'appareil (CompLA2A.h) : TUBE, le son d'origine ; GRAY, SILVER et L2, les trois générations
inline const juce::StringArray modelNames { "TUBE", "GRAY", "SILVER", "L2" };
inline const juce::StringArray meterNames { "GAIN RED", "+10", "+4" };
#else
inline const juce::StringArray meterNames { "GR", "+8", "+4", "OFF" };
#endif

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto dbText = [] (float v, int) { v = std::round (v * 10.0f) / 10.0f + 0.0f; return String (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };

#if SOLO_LA2A
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { gain, 1 }, "Gain", NormalisableRange<float> (-12.0f, 30.0f, 0.1f), 0.0f,
                                                        Attr().withLabel ("dB").withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { peak, 1 }, "Peak Reduction", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f,
                                                        Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); })));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { limit, 1 }, "Mode", StringArray { "Compress", "Limit" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { model, 2 }, "Model", modelNames, 0));
#else
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { input, 1 }, "Input", NormalisableRange<float> (-12.0f, 36.0f, 0.1f), 0.0f,
                                                        Attr().withLabel ("dB").withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { output, 1 }, "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                        Attr().withLabel ("dB").withStringFromValueFunction (dbText)));
    // ATTACK 1..7 (7 = le plus rapide) : 800 µs -> 20 µs ; RELEASE 1..7 : 1.1 s -> 50 ms (Comp1176.h)
    auto atkText = [] (float v, int) { const float us = 800.0f * std::pow (20.0f / 800.0f, (v - 1.0f) / 6.0f); return us >= 1000.0f ? String (us / 1000.0f, 2) + " ms" : String (us / 1000.0f, 2) + " ms"; };
    auto relText = [] (float v, int) { const float ms = 1100.0f * std::pow (50.0f / 1100.0f, (v - 1.0f) / 6.0f); return ms >= 1000.0f ? String (ms / 1000.0f, 2) + " s" : String (roundToInt (ms)) + " ms"; };
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { attack, 1 }, "Attack", NormalisableRange<float> (1.0f, 7.0f, 0.01f), 3.0f,
                                                        Attr().withStringFromValueFunction (atkText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { release, 1 }, "Release", NormalisableRange<float> (1.0f, 7.0f, 0.01f), 5.0f,
                                                        Attr().withStringFromValueFunction (relText)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ratio, 1 }, "Ratio", StringArray { "4", "8", "12", "20", "All" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { revision, 1 }, "Revision", StringArray { "BLACK", "BLUE" }, 0));
#endif
    // les deux appareils
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { meter, 1 }, "Meter", meterNames, 0,
                                                         AudioParameterChoiceAttributes().withAutomatable (false)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { oversampling, 1 }, "Oversampling", StringArray { "OFF", "2x", "4x", "8x" }, 1,
                                                         AudioParameterChoiceAttributes().withAutomatable (false)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                        Attr().withLabel ("%").withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
    return { p.begin(), p.end() };
}

/** Les réglages de session (ce qu'on écoute, la latence) : un preset les laisse. */
inline juce::StringArray notStored() { return { bypass, meter, oversampling }; }
} // namespace params
