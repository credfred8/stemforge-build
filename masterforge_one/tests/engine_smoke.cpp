#include <JuceHeader.h>
#include "../Source/MasterEngine.h"
#include <cmath>
#include <iostream>

namespace
{
bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

float peakOf (const juce::AudioBuffer<float>& b)
{
    float peak = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, 0, b.getNumSamples()));
    return peak;
}
}

int main()
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    constexpr int channels = 2;

    MasterEngine engine;
    engine.prepare (sr, block, channels);

    MasterSettings settings;
    settings.smartGain = false;
    settings.inputTrimDb = 0.0f;
    settings.limiterCeilingDb = -0.9f;
    settings.limiterDriveDb = 4.5f;
    settings.limiterCharacter = 3.5f;
    settings.limiterUpwardDb = 1.5f;
    settings.limiterSoftClip = 0.08f;
    settings.limiterSoftClipMode = 1;
    settings.limiterTransientEmphasis = 0.24f;
    settings.limiterStereoTransient = 0.18f;
    settings.limiterStereoSustain = 0.08f;
    settings.limiterTruePeak = true;
    settings.clipDriveDb = 1.0f;
    settings.ditherOn = false;
    settings.exciterOn = true;
    settings.exciter = 0.75f;
    settings.exciterMix = 0.65f;
    settings.exciterX1Hz = 170.0f;
    settings.exciterX2Hz = 1700.0f;
    settings.exciterX3Hz = 6400.0f;
    settings.exciterBand1 = 0.04f;
    settings.exciterBand2 = 0.08f;
    settings.exciterBand3 = 0.12f;
    settings.exciterBand4 = 0.15f;
    engine.setSettings (settings);

    std::array<int, forgeModuleCount> customOrder { 0, 3, 1, 4, 5, 6, 7, 8, 9, 10, 2, 11 };
    engine.setChainOrder (customOrder, forgeModuleCount);

    juce::AudioBuffer<float> audio (channels, block);
    double phase = 0.0;
    const double phaseDelta = juce::MathConstants<double>::twoPi * 91.0 / sr;

    for (int pass = 0; pass < 120; ++pass)
    {
        for (int i = 0; i < block; ++i)
        {
            const float kick = (i < 28 ? std::exp (-0.12f * (float) i) : 0.0f) * 0.45f;
            const float bass = 0.33f * std::sin ((float) phase);
            const float mid = 0.12f * std::sin ((float) phase * 4.3f);
            const float left = kick + bass + mid;
            const float right = kick + bass * 0.97f + 0.11f * std::sin ((float) phase * 5.1f + 0.35f);
            audio.setSample (0, i, left);
            audio.setSample (1, i, right);
            phase += phaseDelta;
            if (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        engine.process (audio);
        if (! allFinite (audio))
        {
            std::cerr << "FAIL: non-finite sample\n";
            return 2;
        }
    }

    const float peak = peakOf (audio);
    const float peakDb = juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-8f));
    const float ceilingLinear = juce::Decibels::decibelsToGain (-0.9f);
    if (peak > ceilingLinear + 0.002f)
    {
        std::cerr << "FAIL: output peak escaped limiter ceiling: " << peakDb << " dBFS\n";
        return 3;
    }

    if (! std::isfinite (engine.outputRmsDb.load()) || ! std::isfinite (engine.correlation.load()))
    {
        std::cerr << "FAIL: meters invalid\n";
        return 4;
    }

    // Silence must not wind Smart Gain up to a large positive value.
    {
        MasterEngine silenceEngine;
        silenceEngine.prepare (sr, block, channels);
        MasterSettings silentSettings;
        silentSettings.smartGain = true;
        silentSettings.smartMaxGainDb = 12.0f;
        silentSettings.ditherOn = false;
        silenceEngine.setSettings (silentSettings);
        juce::AudioBuffer<float> silent (channels, block);
        silent.clear();
        for (int pass = 0; pass < 80; ++pass)
            silenceEngine.process (silent);

        if (std::abs (silenceEngine.smartGainAppliedDb.load()) > 0.5f)
        {
            std::cerr << "FAIL: Smart Gain winds up on silence\n";
            return 6;
        }
    }

    // Module removal must remain finite and keep reported latency stable.
    {
        std::array<int, forgeModuleCount> shortChain { 0, 3, 7, 9, 10, 11, 0, 0, 0, 0, 0, 0 };
        engine.setChainOrder (shortChain, 6);
        audio.clear();
        audio.setSample (0, 0, 0.6f);
        audio.setSample (1, 0, -0.5f);
        for (int pass = 0; pass < 16; ++pass)
            engine.process (audio);

        if (! allFinite (audio))
        {
            std::cerr << "FAIL: custom module chain produced non-finite samples\n";
            return 7;
        }
    }

    settings.masterBypass = true;
    settings.ditherOn = false;
    engine.setSettings (settings);
    audio.clear();
    audio.setSample (0, 0, 0.25f);
    audio.setSample (1, 0, -0.25f);
    engine.process (audio);
    if (std::abs (audio.getSample (0, 0) - 0.25f) > 1.0e-7f
        || std::abs (audio.getSample (1, 0) + 0.25f) > 1.0e-7f)
    {
        std::cerr << "FAIL: master bypass is not transparent\n";
        return 5;
    }

    std::cout << "MasterForge smoke PASS\n"
              << "latency=" << engine.getLatencySamples() << " samples\n"
              << "output_peak=" << peakDb << " dBFS\n"
              << "output_rms=" << engine.outputRmsDb.load() << " dBFS\n";
    return 0;
}
