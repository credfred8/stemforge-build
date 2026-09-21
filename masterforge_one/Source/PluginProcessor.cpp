#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatP = juce::AudioParameterFloat;
using BoolP = juce::AudioParameterBool;

juce::NormalisableRange<float> dbRange (float lo, float hi, float step = 0.1f)
{
    return { lo, hi, step };
}
}

MasterForgeAudioProcessor::MasterForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MASTERFORGE_PARAMETERS", createLayout())
{
    applyPreset (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterForgeAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<BoolP> ("masterBypass", "Master Bypass", false));
    l.add (std::make_unique<BoolP> ("smartGain", "Smart Input Gain", true));
    l.add (std::make_unique<FloatP> ("inputTrim", "Input Trim", dbRange (-18.0f, 18.0f), 0.0f));
    l.add (std::make_unique<FloatP> ("targetInput", "Target Input RMS", dbRange (-24.0f, -12.0f), -18.0f));

    l.add (std::make_unique<BoolP> ("cleanEqOn", "Clean EQ", true));
    l.add (std::make_unique<FloatP> ("lowShelf", "Low Shelf", dbRange (-6.0f, 6.0f), 0.0f));
    l.add (std::make_unique<FloatP> ("lowMid", "Low Mid", dbRange (-6.0f, 6.0f), -0.6f));
    l.add (std::make_unique<FloatP> ("presence", "Presence", dbRange (-6.0f, 6.0f), 0.7f));
    l.add (std::make_unique<FloatP> ("air", "Air", dbRange (-6.0f, 8.0f), 0.8f));

    l.add (std::make_unique<BoolP> ("dynamicEqOn", "Dynamic EQ", true));
    l.add (std::make_unique<FloatP> ("dynamicEq", "Dynamic EQ Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f));

    l.add (std::make_unique<BoolP> ("resonanceOn", "Resonance Control", true));
    l.add (std::make_unique<FloatP> ("resonance", "Resonance Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.22f));

    l.add (std::make_unique<BoolP> ("glueOn", "Glue Compressor", true));
    l.add (std::make_unique<FloatP> ("glue", "Glue Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));

    l.add (std::make_unique<BoolP> ("multibandOn", "Multiband Dynamics", true));
    l.add (std::make_unique<FloatP> ("multiband", "Multiband Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f));

    l.add (std::make_unique<BoolP> ("impactOn", "Impact", true));
    l.add (std::make_unique<FloatP> ("impact", "Impact Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f));

    l.add (std::make_unique<BoolP> ("analogOn", "Analog Color", true));
    l.add (std::make_unique<FloatP> ("analog", "Analog Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.16f));

    l.add (std::make_unique<BoolP> ("exciterOn", "Exciter", true));
    l.add (std::make_unique<FloatP> ("exciter", "Exciter Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.12f));

    l.add (std::make_unique<BoolP> ("bassMonoOn", "Bass Mono", true));
    l.add (std::make_unique<FloatP> ("bassMonoHz", "Bass Mono Frequency", juce::NormalisableRange<float> (45.0f, 220.0f, 1.0f), 115.0f));

    l.add (std::make_unique<BoolP> ("imagerOn", "Stereo Imager", true));
    l.add (std::make_unique<FloatP> ("widthLow", "Low Width", juce::NormalisableRange<float> (0.0f, 1.5f, 0.001f), 0.92f));
    l.add (std::make_unique<FloatP> ("widthMid", "Mid Width", juce::NormalisableRange<float> (0.0f, 1.8f, 0.001f), 1.02f));
    l.add (std::make_unique<FloatP> ("widthHigh", "High Width", juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.08f));

    l.add (std::make_unique<FloatP> ("dryWet", "Dry Wet", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    l.add (std::make_unique<BoolP> ("clipperOn", "Clipper", true));
    l.add (std::make_unique<FloatP> ("clipDrive", "Clipper Drive", dbRange (0.0f, 12.0f), 1.5f));
    l.add (std::make_unique<FloatP> ("clipMix", "Clipper Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    l.add (std::make_unique<BoolP> ("limiterOn", "Maximizer", true));
    l.add (std::make_unique<FloatP> ("limiterDrive", "Maximizer Drive", dbRange (0.0f, 14.0f), 3.0f));
    l.add (std::make_unique<FloatP> ("ceiling", "Ceiling", dbRange (-3.0f, -0.1f, 0.01f), -0.9f));

    l.add (std::make_unique<FloatP> ("outputTrim", "Output Trim", dbRange (-12.0f, 6.0f), 0.0f));
    l.add (std::make_unique<BoolP> ("ditherOn", "Dither", true));

    return l;
}

void MasterForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine.setSettings (readSettings());
    setLatencySamples (engine.getLatencySamples());
}

void MasterForgeAudioProcessor::releaseResources() {}

bool MasterForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

MasterSettings MasterForgeAudioProcessor::readSettings() const
{
    MasterSettings s;
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [&g] (const char* id) { return g (id) >= 0.5f; };

    s.masterBypass = b ("masterBypass");
    s.smartGain = b ("smartGain");
    s.inputTrimDb = g ("inputTrim");
    s.targetInputRmsDb = g ("targetInput");

    s.cleanEqOn = b ("cleanEqOn");
    s.lowShelfDb = g ("lowShelf");
    s.lowMidDb = g ("lowMid");
    s.presenceDb = g ("presence");
    s.airDb = g ("air");

    s.dynamicEqOn = b ("dynamicEqOn");
    s.dynamicEq = g ("dynamicEq");
    s.resonanceOn = b ("resonanceOn");
    s.resonance = g ("resonance");
    s.glueOn = b ("glueOn");
    s.glue = g ("glue");
    s.multibandOn = b ("multibandOn");
    s.multiband = g ("multiband");
    s.impactOn = b ("impactOn");
    s.impact = g ("impact");
    s.analogOn = b ("analogOn");
    s.analog = g ("analog");
    s.exciterOn = b ("exciterOn");
    s.exciter = g ("exciter");
    s.bassMonoOn = b ("bassMonoOn");
    s.bassMonoHz = g ("bassMonoHz");
    s.imagerOn = b ("imagerOn");
    s.widthLow = g ("widthLow");
    s.widthMid = g ("widthMid");
    s.widthHigh = g ("widthHigh");
    s.dryWet = g ("dryWet");
    s.clipperOn = b ("clipperOn");
    s.clipDriveDb = g ("clipDrive");
    s.clipMix = g ("clipMix");
    s.limiterOn = b ("limiterOn");
    s.limiterDriveDb = g ("limiterDrive");
    s.limiterCeilingDb = g ("ceiling");
    s.outputTrimDb = g ("outputTrim");
    s.ditherOn = b ("ditherOn");
    return s;
}

void MasterForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (readSettings());
    engine.process (buffer);
}

void MasterForgeAudioProcessor::applyPreset (int index)
{
    currentPreset = juce::jlimit (0, (int) presetNames.size() - 1, index);

    struct P
    {
        float lowShelf, lowMid, presence, air, dyn, res, glue, mb, impact, analog, excite;
        float bassHz, wLow, wMid, wHigh, clip, lim, ceiling, target;
    };

    P p {};
    switch (currentPreset)
    {
        case 0: p = { 1.1f,-1.0f,0.8f,0.5f,0.34f,0.24f,0.48f,0.36f,0.46f,0.23f,0.10f,110.0f,0.82f,1.02f,1.08f,2.2f,4.4f,-0.9f,-18.0f }; break;
        case 1: p = { 1.5f,-0.7f,-0.3f,-1.0f,0.27f,0.18f,0.52f,0.31f,0.32f,0.48f,0.06f,125.0f,0.78f,0.98f,1.02f,2.6f,3.8f,-1.0f,-18.0f }; break;
        case 2: p = { 0.7f,-1.2f,1.1f,1.2f,0.40f,0.30f,0.44f,0.42f,0.36f,0.16f,0.18f,105.0f,0.88f,1.06f,1.16f,2.0f,5.4f,-0.8f,-18.0f }; break;
        case 3: p = { 0.4f,-1.0f,1.0f,1.8f,0.45f,0.34f,0.38f,0.48f,0.28f,0.10f,0.22f,100.0f,0.90f,1.08f,1.20f,3.0f,6.2f,-0.8f,-18.0f }; break;
        case 4: p = { 0.2f,-0.5f,0.3f,0.4f,0.22f,0.16f,0.24f,0.20f,0.18f,0.07f,0.06f,95.0f,0.92f,1.00f,1.04f,0.7f,2.0f,-1.0f,-18.0f }; break;
        case 5: p = { 1.2f,0.2f,-0.5f,-0.9f,0.22f,0.14f,0.50f,0.25f,0.22f,0.55f,0.04f,130.0f,0.78f,0.98f,1.00f,1.8f,3.2f,-1.0f,-18.0f }; break;
        case 6: p = { 0.8f,-0.8f,1.3f,0.6f,0.30f,0.20f,0.42f,0.26f,0.70f,0.15f,0.12f,90.0f,0.84f,1.00f,1.05f,3.2f,3.8f,-0.7f,-18.0f }; break;
        case 7: p = { 0.3f,-0.4f,0.2f,0.3f,0.18f,0.12f,0.20f,0.16f,0.14f,0.05f,0.04f,90.0f,0.95f,1.00f,1.02f,0.5f,1.2f,-1.0f,-18.0f }; break;
        default:p = { 0.0f,-0.3f,0.2f,0.2f,0.20f,0.12f,0.25f,0.22f,0.16f,0.05f,0.04f,100.0f,0.90f,1.00f,1.04f,0.8f,2.0f,-1.0f,-18.0f }; break;
    }

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    auto setBool = [&set] (const char* id, bool value) { set (id, value ? 1.0f : 0.0f); };

    setBool ("masterBypass", false);
    setBool ("smartGain", true);
    set ("inputTrim", 0.0f);
    set ("targetInput", p.target);

    setBool ("cleanEqOn", true);
    set ("lowShelf", p.lowShelf);
    set ("lowMid", p.lowMid);
    set ("presence", p.presence);
    set ("air", p.air);

    setBool ("dynamicEqOn", true); set ("dynamicEq", p.dyn);
    setBool ("resonanceOn", true); set ("resonance", p.res);
    setBool ("glueOn", true); set ("glue", p.glue);
    setBool ("multibandOn", true); set ("multiband", p.mb);
    setBool ("impactOn", true); set ("impact", p.impact);
    setBool ("analogOn", true); set ("analog", p.analog);
    setBool ("exciterOn", true); set ("exciter", p.excite);
    setBool ("bassMonoOn", true); set ("bassMonoHz", p.bassHz);
    setBool ("imagerOn", true); set ("widthLow", p.wLow); set ("widthMid", p.wMid); set ("widthHigh", p.wHigh);
    set ("dryWet", 1.0f);
    setBool ("clipperOn", true); set ("clipDrive", p.clip); set ("clipMix", 1.0f);
    setBool ("limiterOn", true); set ("limiterDrive", p.lim); set ("ceiling", p.ceiling);
    set ("outputTrim", 0.0f);
    setBool ("ditherOn", true);
}

void MasterForgeAudioProcessor::setCurrentProgram (int index)
{
    applyPreset (index);
}

const juce::String MasterForgeAudioProcessor::getProgramName (int index)
{
    return presetNames[(size_t) juce::jlimit (0, (int) presetNames.size() - 1, index)];
}

void MasterForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MasterForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("preset", 0);
            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessorEditor* MasterForgeAudioProcessor::createEditor()
{
    return new MasterForgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterForgeAudioProcessor();
}
