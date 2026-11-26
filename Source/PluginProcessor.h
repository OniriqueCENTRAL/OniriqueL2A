#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/SoloEngine.h"
#if SOLO_LICENCE
 #include "License/LicenseGuard.h"
#endif

#if SOLO_LA2A
 #define SOLO_NAME "ONIRIQUE L2A"
 #define SOLO_SLUG "onirique-l2a"
#else
 #define SOLO_NAME "ONIRIQUE F76"
 #define SOLO_SLUG "onirique-f76"
#endif

//==============================================================================
//  ONIRIQUE 1176 / ONIRIQUE LA-2A : le processeur. Mono ou stéréo. La latence déclarée est celle du suréchantillonnage en
//  service ; quand OVERSAMPLING change, le moteur est refait sur le fil des messages (audio suspendu le temps du changement)
//  et la nouvelle latence est déclarée au DAW.
//==============================================================================
class SoloProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SoloProcessor();
    ~SoloProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (params::bypass); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return SOLO_NAME; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // pour l'interface
    float getReductionDb() const noexcept { return engine.getReductionDb(); }
    float consumeOutputPeak() noexcept { return engine.consumeOutputPeak(); }

    juce::UndoManager undo { 30000, 60 };
    juce::AudioProcessorValueTreeState apvts;
    onirique::PresetBank presets { apvts, SOLO_LA2A ? "L2A" : "F76", solopresets::stockPresets(), solopresets::tagNames(), params::notStored() };
#if SOLO_LICENCE
    onirique::license::Guard license { SOLO_SLUG };
#endif

private:
    void process (juce::AudioBuffer<float>&, bool bypassed);
    void timerCallback() override;
    solo::Params read() const;
    void configure();

    solo::Engine engine;
    double rate = 0.0;
    int osOrder = -1;
    std::atomic<bool> ready { false };
    std::atomic<float> *pMix = nullptr, *pBypass = nullptr, *pOs = nullptr;
#if SOLO_LA2A
    std::atomic<float> *pGain = nullptr, *pPeak = nullptr, *pLimit = nullptr, *pModel = nullptr;
#else
    std::atomic<float> *pIn = nullptr, *pOut = nullptr, *pAtk = nullptr, *pRel = nullptr, *pRatio = nullptr, *pRev = nullptr;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoloProcessor)
};
