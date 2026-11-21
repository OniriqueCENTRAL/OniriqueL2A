#include "PluginProcessor.h"
#include "PluginEditor.h"

SoloProcessor::SoloProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "SOLO", params::layout())
{
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pMix = raw (params::mix); pBypass = raw (params::bypass); pOs = raw (params::oversampling);
#if SOLO_LA2A
    pGain = raw (params::gain); pPeak = raw (params::peak); pLimit = raw (params::limit);
#else
    pIn = raw (params::input); pOut = raw (params::output); pAtk = raw (params::attack); pRel = raw (params::release);
    pRatio = raw (params::ratio); pRev = raw (params::revision);
#endif
    apvts.state.setProperty ("preset", "INIT", nullptr);   // l'état de départ : les valeurs par défaut
    startTimerHz (10);
}

SoloProcessor::~SoloProcessor() { stopTimer(); }

bool SoloProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void SoloProcessor::configure()
{
    osOrder = juce::roundToInt (pOs->load());
    engine.prepare (rate, osOrder, std::max (1, getMainBusNumOutputChannels()));
    setLatencySamples (engine.getLatency());
}

void SoloProcessor::prepareToPlay (double sampleRate, int)
{
#if SOLO_LICENCE
    license.refresh();
#endif
    rate = sampleRate;
    configure();
    ready.store (true);
}

// OVERSAMPLING a changé : le moteur est refait ici (fil des messages), audio suspendu le temps du changement
void SoloProcessor::timerCallback()
{
    if (! ready.load() || juce::roundToInt (pOs->load()) == osOrder) return;
    suspendProcessing (true);
    configure();
    suspendProcessing (false);
}

solo::Params SoloProcessor::read() const
{
    solo::Params p;
#if SOLO_LA2A
    p.gain = pGain->load(); p.peak = pPeak->load(); p.limit = pLimit->load() > 0.5f;
#else
    p.input = pIn->load(); p.output = pOut->load(); p.attack = pAtk->load(); p.release = pRel->load();
    p.ratio = juce::roundToInt (pRatio->load()); p.revision = juce::roundToInt (pRev->load());
#endif
    p.mix = pMix->load() / 100.0f;
    p.bypass = pBypass->load() > 0.5f;
    return p;
}

void SoloProcessor::process (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), ch = std::min (2, getMainBusNumOutputChannels());
    // un hôte envoie parfois des NaN / inf : ils ne doivent jamais entrer dans les mémoires des filtres
    for (int c = 0; c < ch; ++c)
    {
        auto* s = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (s[i])) s[i] = 0.0f;
    }
    for (int c = ch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    auto p = read();
    if (bypassed) p.bypass = true;
    juce::AudioBuffer<float> main (buffer.getArrayOfWritePointers(), ch, n);
    engine.process (main, p);
#if SOLO_LICENCE
    license.process (main, getSampleRate(), isNonRealtime());
#endif
}

void SoloProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process (buffer, false); }
void SoloProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process (buffer, true); }

void SoloProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    presets.writeState (state);   // le côté A/B écouté, et l'autre
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SoloProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    presets.readState (state);
    apvts.replaceState (state);
    undo.clearUndoHistory();
}

juce::AudioProcessorEditor* SoloProcessor::createEditor() { return new SoloEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SoloProcessor(); }
