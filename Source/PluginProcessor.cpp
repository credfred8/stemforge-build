#include "PluginProcessor.h"
#include "PluginEditor.h"

StemForgeAudioProcessor::StemForgeAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void StemForgeAudioProcessor::prepareToPlay(double, int) {}
void StemForgeAudioProcessor::releaseResources() {}

bool StemForgeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && out == in;
}

void StemForgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* StemForgeAudioProcessor::createEditor()
{
    return new StemForgeAudioProcessorEditor(*this);
}

void StemForgeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void StemForgeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.getType()))
            state = juce::ValueTree::fromXml(*xml);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StemForgeAudioProcessor();
}
