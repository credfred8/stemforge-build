#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int stateSchemaVersion = 2;
}

VocalForgeAudioProcessor::VocalForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOCALFORGE_STATE", createLayout())
{
    // IMPORTANT:
    // Do not call setValueNotifyingHost() from the processor constructor.
    // FL Studio can still be constructing its VST3 wrapper at this point.
    // Parameter defaults below already represent factory preset #0.
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalForgeAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Defaults are the Male Rap - MID-TOP FORWARD preset, so startup needs
    // no host notifications and is deterministic in FL Studio.
    l.add (std::make_unique<P> (id("input"),    "Input",       R(-18.0f, 18.0f, 0.1f),   1.0f));
    l.add (std::make_unique<P> (id("gate"),     "Gate",        R(-75.0f,-25.0f, 0.1f), -54.0f));
    l.add (std::make_unique<P> (id("lowcut"),   "Low Cut",     R(45.0f, 180.0f, 1.0f),  82.0f));
    l.add (std::make_unique<P> (id("body"),     "Body",        R(-8.0f,   8.0f, 0.1f),  -1.2f));
    l.add (std::make_unique<P> (id("presence"), "Presence",    R(-8.0f,  10.0f, 0.1f),   4.2f));
    l.add (std::make_unique<P> (id("air"),      "Air",         R(-4.0f,  12.0f, 0.1f),   4.8f));
    l.add (std::make_unique<P> (id("deess"),    "De-Esser",    R(0.0f,    1.0f, 0.001f), 0.52f));
    l.add (std::make_unique<P> (id("comp"),     "Peak Comp",   R(0.0f,    1.0f, 0.001f), 0.64f));
    l.add (std::make_unique<P> (id("leveler"),  "Leveler",     R(0.0f,    1.0f, 0.001f), 0.46f));
    l.add (std::make_unique<P> (id("parallel"), "Parallel",    R(0.0f,    1.0f, 0.001f), 0.28f));
    l.add (std::make_unique<P> (id("sat"),      "Saturation",  R(0.0f,    1.0f, 0.001f), 0.18f));
    l.add (std::make_unique<P> (id("grit"),     "Grit",        R(0.0f,    1.0f, 0.001f), 0.12f));
    l.add (std::make_unique<P> (id("doubler"),  "Doubler",     R(0.0f,    1.0f, 0.001f), 0.18f));
    l.add (std::make_unique<P> (id("width"),    "Width",       R(0.55f,   1.65f,0.001f), 1.08f));
    l.add (std::make_unique<P> (id("delay"),    "Delay",       R(0.0f,    1.0f, 0.001f), 0.035f));
    l.add (std::make_unique<P> (id("reverb"),   "Reverb",      R(0.0f,    1.0f, 0.001f), 0.045f));
    l.add (std::make_unique<P> (id("output"),   "Output",      R(-18.0f,  6.0f, 0.1f),  -0.7f));

    return l;
}

void VocalForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepared.store (false, std::memory_order_release);

    const auto safeRate = juce::jlimit (8000.0, 384000.0, sampleRate);
    const auto safeBlock = juce::jlimit (1, 32768, samplesPerBlock);

    engine.prepare (safeRate, safeBlock, juce::jmax (1, getTotalNumOutputChannels()));
    engine.setSettings (readSettings());

    prepared.store (true, std::memory_order_release);
}

void VocalForgeAudioProcessor::releaseResources()
{
    prepared.store (false, std::memory_order_release);
    engine.reset();
}

bool VocalForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // FL Studio normally uses stereo/stereo, but accepting mono/mono also
    // keeps the plugin valid in other vocal routing scenarios.
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

VocalSettings VocalForgeAudioProcessor::readSettings() const
{
    VocalSettings s;

    auto g = [this] (const char* paramId, float fallback)
    {
        if (auto* v = apvts.getRawParameterValue (paramId))
            return v->load();
        return fallback;
    };

    s.inputDb    = g("input", 1.0f);
    s.gateDb     = g("gate", -54.0f);
    s.lowCutHz   = g("lowcut", 82.0f);
    s.bodyDb     = g("body", -1.2f);
    s.presenceDb = g("presence", 4.2f);
    s.airDb      = g("air", 4.8f);
    s.deEss      = g("deess", 0.52f);
    s.comp       = g("comp", 0.64f);
    s.leveler    = g("leveler", 0.46f);
    s.parallel   = g("parallel", 0.28f);
    s.saturation = g("sat", 0.18f);
    s.grit       = g("grit", 0.12f);
    s.doubler    = g("doubler", 0.18f);
    s.width      = g("width", 1.08f);
    s.delay      = g("delay", 0.035f);
    s.reverb     = g("reverb", 0.045f);
    s.outputDb   = g("output", -0.7f);

    return s;
}

void VocalForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Some hosts probe processors in unusual orders during scan/load.
    // Never touch unprepared DSP state.
    if (! prepared.load (std::memory_order_acquire) || buffer.getNumSamples() <= 0)
        return;

    engine.setSettings (readSettings());
    engine.process (buffer);
}

void VocalForgeAudioProcessor::applyPreset (int index)
{
    currentPreset = juce::jlimit (0, (int) presetNames.size() - 1, index);

    VocalSettings s;
    switch (currentPreset)
    {
        case 0: s = { 1.0f,-54.0f,82.0f,-1.2f,4.2f,4.8f,0.52f,0.64f,0.46f,0.28f,0.18f,0.12f,0.18f,1.08f,0.035f,0.045f,-0.7f }; break;
        case 1: s = { 1.8f,-52.0f,88.0f,-1.8f,5.3f,4.2f,0.58f,0.76f,0.52f,0.33f,0.31f,0.34f,0.16f,1.04f,0.025f,0.035f,-0.8f }; break;
        case 2: s = { 0.7f,-55.0f,65.0f,3.4f,1.8f,1.9f,0.42f,0.58f,0.55f,0.31f,0.23f,0.08f,0.12f,1.02f,0.03f,0.05f,-0.7f }; break;
        case 3: s = { 1.2f,-50.0f,72.0f,1.8f,0.8f,-0.7f,0.40f,0.67f,0.50f,0.36f,0.38f,0.28f,0.14f,1.06f,0.055f,0.07f,-0.9f }; break;
        case 4: s = { 0.8f,-58.0f,95.0f,-0.8f,3.6f,6.8f,0.62f,0.54f,0.42f,0.22f,0.12f,0.07f,0.17f,1.10f,0.04f,0.075f,-0.8f }; break;
        case 5: s = { 0.5f,-57.0f,88.0f,0.8f,2.4f,4.2f,0.68f,0.65f,0.56f,0.30f,0.20f,0.09f,0.13f,1.05f,0.03f,0.06f,-0.8f }; break;
        case 6: s = { 0.0f,-54.0f,110.0f,-1.6f,2.0f,2.5f,0.52f,0.60f,0.43f,0.24f,0.13f,0.08f,0.48f,1.48f,0.08f,0.14f,-1.0f }; break;
        case 7: s = { 0.0f,-52.0f,125.0f,-2.6f,3.4f,5.0f,0.58f,0.66f,0.40f,0.18f,0.19f,0.20f,0.32f,1.34f,0.24f,0.22f,-1.0f }; break;
    }

    auto set = [this] (const char* paramId, float value)
    {
        if (auto* p = apvts.getParameter (paramId))
        {
            const float normalised = p->convertTo0to1 (value);
            p->beginChangeGesture();
            p->setValueNotifyingHost (normalised);
            p->endChangeGesture();
        }
    };

    set("input",s.inputDb); set("gate",s.gateDb); set("lowcut",s.lowCutHz);
    set("body",s.bodyDb); set("presence",s.presenceDb); set("air",s.airDb);
    set("deess",s.deEss); set("comp",s.comp); set("leveler",s.leveler);
    set("parallel",s.parallel); set("sat",s.saturation); set("grit",s.grit);
    set("doubler",s.doubler); set("width",s.width); set("delay",s.delay);
    set("reverb",s.reverb); set("output",s.outputDb);
}

void VocalForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("schemaVersion", stateSchemaVersion, nullptr);
    state.setProperty ("preset", juce::jlimit (0, 7, currentPreset), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Defensive state restore: ignore empty, absurd or incompatible chunks.
    // This also prevents old v2.0/v2.1 FL cache state from destabilising v2.2.
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > 8 * 1024 * 1024)
        return;

    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    const int version = (int) state.getProperty ("schemaVersion", 0);
    if (version != stateSchemaVersion)
        return;

    currentPreset = juce::jlimit (0, 7, (int) state.getProperty ("preset", 0));
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* VocalForgeAudioProcessor::createEditor()
{
    return new VocalForgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalForgeAudioProcessor();
}
