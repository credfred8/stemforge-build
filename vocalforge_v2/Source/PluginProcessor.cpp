#include "PluginProcessor.h"
#include "PluginEditor.h"

VocalForgeAudioProcessor::VocalForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    applyPreset (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalForgeAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> ("input", "Input", juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> ("gate", "Gate", juce::NormalisableRange<float> (-75.0f, -25.0f, 0.1f), -52.0f));
    l.add (std::make_unique<P> ("lowcut", "Low Cut", juce::NormalisableRange<float> (45.0f, 180.0f, 1.0f), 75.0f));
    l.add (std::make_unique<P> ("body", "Body", juce::NormalisableRange<float> (-8.0f, 8.0f, 0.1f), 0.0f));
    l.add (std::make_unique<P> ("presence", "Presence", juce::NormalisableRange<float> (-8.0f, 10.0f, 0.1f), 2.0f));
    l.add (std::make_unique<P> ("air", "Air", juce::NormalisableRange<float> (-4.0f, 12.0f, 0.1f), 2.5f));
    l.add (std::make_unique<P> ("deess", "De-Esser", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f));
    l.add (std::make_unique<P> ("comp", "Peak Comp", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));
    l.add (std::make_unique<P> ("leveler", "Leveler", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.40f));
    l.add (std::make_unique<P> ("parallel", "Parallel", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.22f));
    l.add (std::make_unique<P> ("sat", "Saturation", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.16f));
    l.add (std::make_unique<P> ("grit", "Grit", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.10f));
    l.add (std::make_unique<P> ("doubler", "Doubler", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.16f));
    l.add (std::make_unique<P> ("width", "Width", juce::NormalisableRange<float> (0.55f, 1.65f, 0.001f), 1.05f));
    l.add (std::make_unique<P> ("delay", "Delay", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.05f));
    l.add (std::make_unique<P> ("reverb", "Reverb", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.06f));
    l.add (std::make_unique<P> ("output", "Output", juce::NormalisableRange<float> (-18.0f, 6.0f, 0.1f), -0.5f));
    return l;
}

void VocalForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine.setSettings (readSettings());
}

void VocalForgeAudioProcessor::releaseResources() {}

bool VocalForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

VocalSettings VocalForgeAudioProcessor::readSettings() const
{
    VocalSettings s;
    auto g = [this](const char* id){ return apvts.getRawParameterValue (id)->load(); };
    s.inputDb = g("input"); s.gateDb = g("gate"); s.lowCutHz = g("lowcut");
    s.bodyDb = g("body"); s.presenceDb = g("presence"); s.airDb = g("air");
    s.deEss = g("deess"); s.comp = g("comp"); s.leveler = g("leveler");
    s.parallel = g("parallel"); s.saturation = g("sat"); s.grit = g("grit");
    s.doubler = g("doubler"); s.width = g("width"); s.delay = g("delay");
    s.reverb = g("reverb"); s.outputDb = g("output");
    return s;
}

void VocalForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
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

    auto set = [this](const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    set("input",s.inputDb); set("gate",s.gateDb); set("lowcut",s.lowCutHz);
    set("body",s.bodyDb); set("presence",s.presenceDb); set("air",s.airDb);
    set("deess",s.deEss); set("comp",s.comp); set("leveler",s.leveler);
    set("parallel",s.parallel); set("sat",s.saturation); set("grit",s.grit);
    set("doubler",s.doubler); set("width",s.width); set("delay",s.delay);
    set("reverb",s.reverb); set("output",s.outputDb);
}

void VocalForgeAudioProcessor::setCurrentProgram (int index) { applyPreset (index); }
const juce::String VocalForgeAudioProcessor::getProgramName (int index)
{
    return presetNames[(size_t) juce::jlimit (0, (int) presetNames.size() - 1, index)];
}

void VocalForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void VocalForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("preset", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* VocalForgeAudioProcessor::createEditor()
{
    return new VocalForgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalForgeAudioProcessor();
}
