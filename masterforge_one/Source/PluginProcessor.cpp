#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatP = juce::AudioParameterFloat;
using BoolP = juce::AudioParameterBool;

juce::NormalisableRange<float> range (float lo, float hi, float step = 0.01f)
{
    return { lo, hi, step };
}

juce::NormalisableRange<float> freqRange (float lo, float hi, float centre, float step = 1.0f)
{
    juce::NormalisableRange<float> r { lo, hi, step };
    r.setSkewForCentre (centre);
    return r;
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
    l.add (std::make_unique<FloatP> ("inputTrim", "Input Trim", range (-18.0f, 18.0f, 0.1f), 0.0f));
    l.add (std::make_unique<FloatP> ("targetInput", "Target Input RMS", range (-24.0f, -12.0f, 0.1f), -18.0f));
    l.add (std::make_unique<FloatP> ("smartSpeed", "Smart Gain Speed", range (0.0f, 1.0f, 0.001f), 0.35f));
    l.add (std::make_unique<FloatP> ("smartMaxGain", "Smart Gain Range", range (3.0f, 18.0f, 0.1f), 9.0f));

    l.add (std::make_unique<BoolP> ("cleanEqOn", "Clean EQ", true));
    l.add (std::make_unique<FloatP> ("lowShelf", "Low Shelf Gain", range (-6.0f, 6.0f, 0.1f), 0.0f));
    l.add (std::make_unique<FloatP> ("lowShelfHz", "Low Shelf Frequency", freqRange (35.0f, 350.0f, 105.0f), 105.0f));
    l.add (std::make_unique<FloatP> ("lowMid", "Low Mid Gain", range (-6.0f, 6.0f, 0.1f), -0.6f));
    l.add (std::make_unique<FloatP> ("lowMidHz", "Low Mid Frequency", freqRange (90.0f, 1200.0f, 320.0f), 320.0f));
    l.add (std::make_unique<FloatP> ("lowMidQ", "Low Mid Q", range (0.25f, 4.0f, 0.01f), 0.85f));
    l.add (std::make_unique<FloatP> ("midGain", "Mid Gain", range (-12.0f, 12.0f, 0.1f), 0.0f));
    l.add (std::make_unique<FloatP> ("midHz", "Mid Frequency", freqRange (180.0f, 4000.0f, 900.0f), 900.0f));
    l.add (std::make_unique<FloatP> ("midQ", "Mid Q", range (0.25f, 4.0f, 0.01f), 0.90f));
    l.add (std::make_unique<FloatP> ("presence", "Presence Gain", range (-6.0f, 6.0f, 0.1f), 0.7f));
    l.add (std::make_unique<FloatP> ("presenceHz", "Presence Frequency", freqRange (900.0f, 8000.0f, 3200.0f), 3200.0f));
    l.add (std::make_unique<FloatP> ("presenceQ", "Presence Q", range (0.25f, 4.0f, 0.01f), 0.90f));
    l.add (std::make_unique<FloatP> ("highMidGain", "High Mid Gain", range (-12.0f, 12.0f, 0.1f), 0.0f));
    l.add (std::make_unique<FloatP> ("highMidHz", "High Mid Frequency", freqRange (1800.0f, 14000.0f, 6200.0f), 6200.0f));
    l.add (std::make_unique<FloatP> ("highMidQ", "High Mid Q", range (0.25f, 4.0f, 0.01f), 0.90f));
    l.add (std::make_unique<FloatP> ("air", "Air Gain", range (-6.0f, 8.0f, 0.1f), 0.8f));
    l.add (std::make_unique<FloatP> ("airHz", "Air Frequency", freqRange (5000.0f, 18000.0f, 10500.0f), 10500.0f));

    l.add (std::make_unique<BoolP> ("dynamicEqOn", "Dynamic EQ", true));
    l.add (std::make_unique<FloatP> ("dynamicEq", "Dynamic EQ Amount", range (0.0f, 1.0f, 0.001f), 0.30f));
    l.add (std::make_unique<FloatP> ("dynThreshold", "Dynamic EQ Threshold", range (-36.0f, -8.0f, 0.1f), -20.0f));
    l.add (std::make_unique<FloatP> ("dynAttack", "Dynamic EQ Attack", range (1.0f, 80.0f, 0.1f), 18.0f));
    l.add (std::make_unique<FloatP> ("dynRelease", "Dynamic EQ Release", range (40.0f, 500.0f, 1.0f), 160.0f));
    l.add (std::make_unique<FloatP> ("dynLowHz", "Dynamic EQ Low Crossover", freqRange (80.0f, 700.0f, 260.0f), 260.0f));
    l.add (std::make_unique<FloatP> ("dynHighHz", "Dynamic EQ High Crossover", freqRange (1800.0f, 12000.0f, 5200.0f), 5200.0f));

    l.add (std::make_unique<BoolP> ("resonanceOn", "Resonance Control", true));
    l.add (std::make_unique<FloatP> ("resonance", "Resonance Amount", range (0.0f, 1.0f, 0.001f), 0.22f));
    l.add (std::make_unique<FloatP> ("resonanceHz", "Resonance Frequency", freqRange (500.0f, 12000.0f, 2850.0f), 2850.0f));
    l.add (std::make_unique<FloatP> ("resonanceQ", "Resonance Q", range (0.4f, 10.0f, 0.01f), 2.6f));

    l.add (std::make_unique<BoolP> ("glueOn", "Glue Compressor", true));
    l.add (std::make_unique<FloatP> ("glue", "Glue Amount", range (0.0f, 1.0f, 0.001f), 0.35f));
    l.add (std::make_unique<FloatP> ("glueThreshold", "Glue Threshold", range (-36.0f, -2.0f, 0.1f), -16.0f));
    l.add (std::make_unique<FloatP> ("glueRatio", "Glue Ratio", range (1.1f, 10.0f, 0.1f), 2.0f));
    l.add (std::make_unique<FloatP> ("glueAttack", "Glue Attack", range (0.5f, 100.0f, 0.1f), 24.0f));
    l.add (std::make_unique<FloatP> ("glueRelease", "Glue Release", range (30.0f, 600.0f, 1.0f), 180.0f));
    l.add (std::make_unique<FloatP> ("glueMakeup", "Glue Makeup", range (-6.0f, 6.0f, 0.1f), 0.0f));
    l.add (std::make_unique<FloatP> ("glueMix", "Glue Mix", range (0.0f, 1.0f, 0.001f), 0.72f));

    l.add (std::make_unique<BoolP> ("multibandOn", "Multiband Dynamics", true));
    l.add (std::make_unique<FloatP> ("multiband", "Multiband Amount", range (0.0f, 1.0f, 0.001f), 0.30f));
    l.add (std::make_unique<FloatP> ("mbLowHz", "Multiband Low Crossover", freqRange (70.0f, 500.0f, 150.0f), 150.0f));
    l.add (std::make_unique<FloatP> ("mbHighHz", "Multiband High Crossover", freqRange (1800.0f, 12000.0f, 4500.0f), 4500.0f));
    l.add (std::make_unique<FloatP> ("mbLowAmount", "Multiband Low Amount", range (0.0f, 1.0f, 0.001f), 0.40f));
    l.add (std::make_unique<FloatP> ("mbMidAmount", "Multiband Mid Amount", range (0.0f, 1.0f, 0.001f), 0.30f));
    l.add (std::make_unique<FloatP> ("mbHighAmount", "Multiband High Amount", range (0.0f, 1.0f, 0.001f), 0.24f));

    l.add (std::make_unique<BoolP> ("impactOn", "Impact", true));
    l.add (std::make_unique<FloatP> ("impact", "Impact Amount", range (0.0f, 1.0f, 0.001f), 0.30f));
    l.add (std::make_unique<FloatP> ("impactSpeed", "Impact Speed", range (0.0f, 1.0f, 0.001f), 0.45f));
    l.add (std::make_unique<FloatP> ("impactMix", "Impact Mix", range (0.0f, 1.0f, 0.001f), 0.70f));

    l.add (std::make_unique<BoolP> ("analogOn", "Analog Color", true));
    l.add (std::make_unique<FloatP> ("analog", "Analog Drive", range (0.0f, 1.0f, 0.001f), 0.16f));
    l.add (std::make_unique<FloatP> ("analogTone", "Analog Tone", range (0.0f, 1.0f, 0.001f), 0.58f));
    l.add (std::make_unique<FloatP> ("analogMix", "Analog Mix", range (0.0f, 1.0f, 0.001f), 0.55f));

    l.add (std::make_unique<BoolP> ("exciterOn", "Exciter", true));
    l.add (std::make_unique<FloatP> ("exciter", "Exciter Amount", range (0.0f, 1.0f, 0.001f), 0.12f));
    l.add (std::make_unique<FloatP> ("exciterHz", "Exciter Frequency", freqRange (2500.0f, 14000.0f, 6500.0f), 6500.0f));
    l.add (std::make_unique<FloatP> ("exciterMix", "Exciter Mix", range (0.0f, 1.0f, 0.001f), 0.45f));

    l.add (std::make_unique<BoolP> ("bassMonoOn", "Bass Mono", true));
    l.add (std::make_unique<FloatP> ("bassMonoHz", "Bass Mono Frequency", freqRange (45.0f, 250.0f, 115.0f), 115.0f));
    l.add (std::make_unique<FloatP> ("bassMonoAmount", "Bass Mono Amount", range (0.0f, 1.0f, 0.001f), 1.0f));

    l.add (std::make_unique<BoolP> ("imagerOn", "Stereo Imager", true));
    l.add (std::make_unique<FloatP> ("widthLow", "Low Width", range (0.0f, 1.5f, 0.001f), 0.92f));
    l.add (std::make_unique<FloatP> ("widthMid", "Mid Width", range (0.0f, 1.8f, 0.001f), 1.02f));
    l.add (std::make_unique<FloatP> ("widthHigh", "High Width", range (0.0f, 2.0f, 0.001f), 1.08f));
    l.add (std::make_unique<FloatP> ("imagerLowHz", "Imager Low Crossover", freqRange (80.0f, 500.0f, 180.0f), 180.0f));
    l.add (std::make_unique<FloatP> ("imagerHighHz", "Imager High Crossover", freqRange (1800.0f, 12000.0f, 5000.0f), 5000.0f));
    l.add (std::make_unique<FloatP> ("imagerSafety", "Imager Correlation Safety", range (0.0f, 1.0f, 0.001f), 0.80f));

    l.add (std::make_unique<FloatP> ("dryWet", "Master Dry Wet", range (0.0f, 1.0f, 0.001f), 1.0f));

    l.add (std::make_unique<BoolP> ("clipperOn", "Clipper", true));
    l.add (std::make_unique<FloatP> ("clipDrive", "Clipper Drive", range (0.0f, 12.0f, 0.1f), 1.5f));
    l.add (std::make_unique<FloatP> ("clipMix", "Clipper Mix", range (0.0f, 1.0f, 0.001f), 1.0f));
    l.add (std::make_unique<FloatP> ("clipCeiling", "Clipper Ceiling", range (-3.0f, 0.0f, 0.01f), -0.35f));
    l.add (std::make_unique<FloatP> ("clipShape", "Clipper Shape", range (0.0f, 1.0f, 0.001f), 0.55f));

    l.add (std::make_unique<BoolP> ("limiterOn", "Maximizer", true));
    l.add (std::make_unique<FloatP> ("limiterDrive", "Maximizer Drive", range (0.0f, 14.0f, 0.1f), 3.0f));
    l.add (std::make_unique<FloatP> ("ceiling", "Ceiling", range (-3.0f, -0.1f, 0.01f), -0.9f));
    l.add (std::make_unique<FloatP> ("limiterRelease", "Limiter Release", range (20.0f, 500.0f, 1.0f), 120.0f));

    l.add (std::make_unique<FloatP> ("outputTrim", "Output Trim", range (-12.0f, 6.0f, 0.1f), 0.0f));
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
    s.smartSpeed = g ("smartSpeed");
    s.smartMaxGainDb = g ("smartMaxGain");

    s.cleanEqOn = b ("cleanEqOn");
    s.lowShelfDb = g ("lowShelf");
    s.lowShelfHz = g ("lowShelfHz");
    s.lowMidDb = g ("lowMid");
    s.lowMidHz = g ("lowMidHz");
    s.lowMidQ = g ("lowMidQ");
    s.midDb = g ("midGain");
    s.midHz = g ("midHz");
    s.midQ = g ("midQ");
    s.presenceDb = g ("presence");
    s.presenceHz = g ("presenceHz");
    s.presenceQ = g ("presenceQ");
    s.highMidDb = g ("highMidGain");
    s.highMidHz = g ("highMidHz");
    s.highMidQ = g ("highMidQ");
    s.airDb = g ("air");
    s.airHz = g ("airHz");

    s.dynamicEqOn = b ("dynamicEqOn");
    s.dynamicEq = g ("dynamicEq");
    s.dynThresholdDb = g ("dynThreshold");
    s.dynAttackMs = g ("dynAttack");
    s.dynReleaseMs = g ("dynRelease");
    s.dynLowHz = g ("dynLowHz");
    s.dynHighHz = g ("dynHighHz");

    s.resonanceOn = b ("resonanceOn");
    s.resonance = g ("resonance");
    s.resonanceHz = g ("resonanceHz");
    s.resonanceQ = g ("resonanceQ");

    s.glueOn = b ("glueOn");
    s.glue = g ("glue");
    s.glueThresholdDb = g ("glueThreshold");
    s.glueRatio = g ("glueRatio");
    s.glueAttackMs = g ("glueAttack");
    s.glueReleaseMs = g ("glueRelease");
    s.glueMakeupDb = g ("glueMakeup");
    s.glueMix = g ("glueMix");

    s.multibandOn = b ("multibandOn");
    s.multiband = g ("multiband");
    s.mbLowHz = g ("mbLowHz");
    s.mbHighHz = g ("mbHighHz");
    s.mbLowAmount = g ("mbLowAmount");
    s.mbMidAmount = g ("mbMidAmount");
    s.mbHighAmount = g ("mbHighAmount");

    s.impactOn = b ("impactOn");
    s.impact = g ("impact");
    s.impactSpeed = g ("impactSpeed");
    s.impactMix = g ("impactMix");

    s.analogOn = b ("analogOn");
    s.analog = g ("analog");
    s.analogTone = g ("analogTone");
    s.analogMix = g ("analogMix");

    s.exciterOn = b ("exciterOn");
    s.exciter = g ("exciter");
    s.exciterHz = g ("exciterHz");
    s.exciterMix = g ("exciterMix");

    s.bassMonoOn = b ("bassMonoOn");
    s.bassMonoHz = g ("bassMonoHz");
    s.bassMonoAmount = g ("bassMonoAmount");

    s.imagerOn = b ("imagerOn");
    s.widthLow = g ("widthLow");
    s.widthMid = g ("widthMid");
    s.widthHigh = g ("widthHigh");
    s.imagerLowHz = g ("imagerLowHz");
    s.imagerHighHz = g ("imagerHighHz");
    s.imagerSafety = g ("imagerSafety");

    s.dryWet = g ("dryWet");

    s.clipperOn = b ("clipperOn");
    s.clipDriveDb = g ("clipDrive");
    s.clipMix = g ("clipMix");
    s.clipCeilingDb = g ("clipCeiling");
    s.clipShape = g ("clipShape");

    s.limiterOn = b ("limiterOn");
    s.limiterDriveDb = g ("limiterDrive");
    s.limiterCeilingDb = g ("ceiling");
    s.limiterReleaseMs = g ("limiterRelease");

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

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    auto setBool = [&set] (const char* id, bool value) { set (id, value ? 1.0f : 0.0f); };

    setBool ("masterBypass", false);
    setBool ("smartGain", true);
    set ("inputTrim", 0.0f);
    set ("targetInput", -17.0f);
    set ("smartSpeed", 0.32f);
    set ("smartMaxGain", 9.0f);

    setBool ("cleanEqOn", true);
    set ("lowShelf", 0.4f); set ("lowShelfHz", 105.0f);
    set ("lowMid", -0.6f); set ("lowMidHz", 320.0f); set ("lowMidQ", 0.85f);
    set ("midGain", 0.0f); set ("midHz", 900.0f); set ("midQ", 0.90f);
    set ("presence", 0.5f); set ("presenceHz", 3200.0f); set ("presenceQ", 0.90f);
    set ("highMidGain", 0.0f); set ("highMidHz", 6200.0f); set ("highMidQ", 0.90f);
    set ("air", 0.5f); set ("airHz", 10500.0f);

    setBool ("dynamicEqOn", true);
    set ("dynamicEq", 0.28f); set ("dynThreshold", -20.0f);
    set ("dynAttack", 18.0f); set ("dynRelease", 160.0f);
    set ("dynLowHz", 260.0f); set ("dynHighHz", 5200.0f);

    setBool ("resonanceOn", true);
    set ("resonance", 0.20f); set ("resonanceHz", 2850.0f); set ("resonanceQ", 2.6f);

    setBool ("glueOn", true);
    set ("glue", 0.35f); set ("glueThreshold", -16.0f); set ("glueRatio", 2.0f);
    set ("glueAttack", 24.0f); set ("glueRelease", 180.0f); set ("glueMakeup", 0.0f); set ("glueMix", 0.72f);

    setBool ("multibandOn", true);
    set ("multiband", 0.28f); set ("mbLowHz", 150.0f); set ("mbHighHz", 4500.0f);
    set ("mbLowAmount", 0.40f); set ("mbMidAmount", 0.30f); set ("mbHighAmount", 0.24f);

    setBool ("impactOn", true);
    set ("impact", 0.30f); set ("impactSpeed", 0.45f); set ("impactMix", 0.70f);

    setBool ("analogOn", true);
    set ("analog", 0.14f); set ("analogTone", 0.58f); set ("analogMix", 0.52f);

    setBool ("exciterOn", true);
    set ("exciter", 0.10f); set ("exciterHz", 6500.0f); set ("exciterMix", 0.42f);

    setBool ("bassMonoOn", true);
    set ("bassMonoHz", 115.0f); set ("bassMonoAmount", 1.0f);

    setBool ("imagerOn", true);
    set ("widthLow", 0.90f); set ("widthMid", 1.02f); set ("widthHigh", 1.08f);
    set ("imagerLowHz", 180.0f); set ("imagerHighHz", 5000.0f); set ("imagerSafety", 0.85f);

    set ("dryWet", 1.0f);

    setBool ("clipperOn", true);
    set ("clipDrive", 1.2f); set ("clipMix", 1.0f); set ("clipCeiling", -0.35f); set ("clipShape", 0.55f);

    setBool ("limiterOn", true);
    set ("limiterDrive", 3.2f); set ("ceiling", -0.9f); set ("limiterRelease", 120.0f);

    set ("outputTrim", 0.0f);
    setBool ("ditherOn", false);

    switch (currentPreset)
    {
        case 0: // Boom Bap - DENSE PUNCH
            set ("targetInput", -16.0f);
            set ("lowShelf", 1.0f); set ("lowShelfHz", 92.0f);
            set ("lowMid", -0.9f);
            set ("midGain", -0.25f); set ("midHz", 820.0f);
            set ("highMidGain", 0.35f); set ("highMidHz", 5400.0f); set ("lowMidHz", 300.0f);
            set ("presence", 0.7f); set ("air", 0.35f);
            set ("glue", 0.52f); set ("glueThreshold", -18.0f); set ("glueRatio", 2.5f);
            set ("glueAttack", 28.0f); set ("glueRelease", 150.0f); set ("glueMakeup", 0.5f); set ("glueMix", 0.78f);
            set ("impact", 0.52f); set ("impactSpeed", 0.62f);
            set ("analog", 0.22f); set ("analogMix", 0.60f);
            set ("clipDrive", 2.4f); set ("clipShape", 0.58f);
            set ("limiterDrive", 5.0f); set ("ceiling", -0.8f); set ("limiterRelease", 110.0f);
            set ("widthLow", 0.78f); set ("widthMid", 1.01f); set ("widthHigh", 1.08f);
            break;

        case 1: // Dusty analog
            set ("lowShelf", 1.3f); set ("lowMid", -0.3f); set ("presence", -0.4f); set ("air", -0.9f);
            set ("dynamicEq", 0.20f); set ("glue", 0.55f); set ("glueAttack", 32.0f);
            set ("analog", 0.48f); set ("analogTone", 0.40f); set ("analogMix", 0.74f);
            set ("exciter", 0.04f); set ("clipDrive", 1.8f); set ("limiterDrive", 3.2f); set ("ceiling", -1.0f);
            break;

        case 2: // Modern hip-hop
            set ("targetInput", -16.0f);
            set ("lowShelf", 0.6f); set ("lowMid", -1.1f); set ("midGain", -0.2f); set ("highMidGain", 0.45f); set ("presence", 1.0f); set ("air", 1.2f);
            set ("dynamicEq", 0.42f); set ("resonance", 0.28f);
            set ("multiband", 0.42f); set ("impact", 0.36f);
            set ("exciter", 0.18f); set ("widthHigh", 1.16f);
            set ("clipDrive", 2.2f); set ("limiterDrive", 5.2f); set ("limiterRelease", 90.0f);
            break;

        case 3: // Trap loud clean
            set ("targetInput", -15.5f);
            set ("lowShelf", 0.35f); set ("lowMid", -1.0f); set ("presence", 0.9f); set ("air", 1.6f);
            set ("dynamicEq", 0.46f); set ("multiband", 0.48f);
            set ("glue", 0.30f); set ("impact", 0.30f); set ("exciter", 0.22f);
            set ("clipDrive", 3.0f); set ("clipShape", 0.68f);
            set ("limiterDrive", 6.0f); set ("ceiling", -0.8f); set ("limiterRelease", 75.0f);
            break;

        case 4: // Streaming transparent
            set ("targetInput", -18.0f);
            set ("lowShelf", 0.1f); set ("lowMid", -0.35f); set ("presence", 0.2f); set ("air", 0.3f);
            set ("dynamicEq", 0.18f); set ("resonance", 0.14f);
            set ("glue", 0.18f); set ("glueMix", 0.50f); set ("multiband", 0.16f); set ("impact", 0.12f);
            set ("analog", 0.04f); set ("exciter", 0.04f);
            set ("clipDrive", 0.5f); set ("limiterDrive", 1.8f); set ("ceiling", -1.0f); set ("limiterRelease", 180.0f);
            break;

        case 5: // Vinyl warm glue
            set ("targetInput", -17.0f);
            set ("lowShelf", 1.1f); set ("lowMid", 0.1f); set ("presence", -0.5f); set ("air", -1.1f);
            set ("glue", 0.58f); set ("glueThreshold", -19.0f); set ("glueRatio", 2.2f);
            set ("analog", 0.55f); set ("analogTone", 0.34f); set ("analogMix", 0.80f);
            set ("widthHigh", 1.00f); set ("clipDrive", 1.4f); set ("limiterDrive", 2.8f); set ("ceiling", -1.0f);
            break;

        case 6: // Drums hard punch
            set ("targetInput", -16.5f);
            set ("lowShelf", 0.7f); set ("presence", 1.1f);
            set ("glue", 0.42f); set ("glueAttack", 35.0f); set ("glueRelease", 110.0f);
            set ("impact", 0.76f); set ("impactSpeed", 0.78f); set ("impactMix", 0.86f);
            set ("clipDrive", 3.2f); set ("limiterDrive", 3.5f); set ("ceiling", -0.7f);
            break;

        case 7: // Mixbus open dynamic
            set ("targetInput", -18.0f);
            set ("lowShelf", 0.2f); set ("lowMid", -0.3f); set ("presence", 0.2f); set ("air", 0.2f);
            set ("dynamicEq", 0.14f); set ("resonance", 0.10f);
            set ("glue", 0.18f); set ("glueMix", 0.45f); set ("multiband", 0.12f); set ("impact", 0.10f);
            set ("analog", 0.04f); set ("exciter", 0.03f); set ("clipDrive", 0.3f);
            set ("limiterDrive", 0.8f); set ("ceiling", -1.0f); set ("limiterRelease", 220.0f);
            break;

        default: // Safe master
            set ("targetInput", -18.0f);
            set ("lowShelf", 0.0f); set ("lowMid", -0.2f); set ("presence", 0.1f); set ("air", 0.1f);
            set ("dynamicEq", 0.16f); set ("resonance", 0.10f);
            set ("glue", 0.20f); set ("multiband", 0.16f); set ("impact", 0.10f);
            set ("analog", 0.03f); set ("exciter", 0.03f); set ("clipDrive", 0.4f);
            set ("limiterDrive", 1.5f); set ("ceiling", -1.0f); set ("limiterRelease", 180.0f);
            break;
    }
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
