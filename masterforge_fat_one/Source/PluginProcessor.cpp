#include "PluginProcessor.h"
#include "PluginEditor.h"

MasterForgeFatOneAudioProcessor::MasterForgeFatOneAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterForgeFatOneAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fat", 1 }, "FAT",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.72f), 35.0f));
    return layout;
}

void MasterForgeFatOneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

bool MasterForgeFatOneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    return mainIn == mainOut;
}

void MasterForgeFatOneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    inputPeak.store (buffer.getMagnitude (0, buffer.getNumSamples()));
    const float amount = *apvts.getRawParameterValue ("fat") * 0.01f;
    engine.setAmount (amount);
    engine.process (buffer);
    outputPeak.store (buffer.getMagnitude (0, buffer.getNumSamples()));
}

void MasterForgeFatOneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MasterForgeFatOneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MasterForgeFatOneAudioProcessor::createEditor()
{
    return new MasterForgeFatOneAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterForgeFatOneAudioProcessor();
}
