#pragma once
#include <JuceHeader.h>
#include <cmath>

class OneKnobEngine
{
public:
    void prepare (double newSampleRate, int maxBlockSize, int channels)
    {
        sampleRate = newSampleRate;
        amountSmoothed.reset (sampleRate, 0.08);
        amountSmoothed.setCurrentAndTargetValue (0.0f);

        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize),
                                      static_cast<juce::uint32> (juce::jmax (1, channels)) };

        lowShelf.prepare (spec);
        midPeak.prepare (spec);
        highShelf.prepare (spec);
        compressor.prepare (spec);
        limiter.prepare (spec);

        compressor.setAttack (22.0f);
        compressor.setRelease (130.0f);
        limiter.setRelease (80.0f);

        sideLP_L = sideLP_R = 0.0f;
        rmsEnvelope = 0.0f;
        updateFilters (0.0f);
        reset();
    }

    void reset()
    {
        lowShelf.reset();
        midPeak.reset();
        highShelf.reset();
        compressor.reset();
        limiter.reset();
        sideLP_L = sideLP_R = 0.0f;
        rmsEnvelope = 0.0f;
    }

    void setAmount (float newAmount)
    {
        amountSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, newAmount));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int channels = buffer.getNumChannels();
        const int samples  = buffer.getNumSamples();
        if (channels == 0 || samples == 0)
            return;

        const float targetAmount = amountSmoothed.getTargetValue();
        if (targetAmount <= 0.00001f && amountSmoothed.getCurrentValue() <= 0.00001f)
            return;

        scratch.makeCopyOf (buffer, true);

        float blockAmount = amountSmoothed.getNextValue();
        for (int i = 1; i < samples; ++i)
            blockAmount = amountSmoothed.getNextValue();

        updateFilters (blockAmount);

        // Adaptive pre-drive: more on quiet mixes, less on already hot material.
        double sumSq = 0.0;
        const int measureChannels = juce::jmin (2, channels);
        for (int ch = 0; ch < measureChannels; ++ch)
        {
            const float* p = buffer.getReadPointer (ch);
            for (int i = 0; i < samples; ++i)
                sumSq += static_cast<double> (p[i]) * p[i];
        }

        const double denom = juce::jmax (1, samples * measureChannels);
        const float rms = static_cast<float> (std::sqrt (sumSq / denom + 1.0e-12));
        const float rmsDb = juce::Decibels::gainToDecibels (rms, -120.0f);
        const float quietAssist = juce::jlimit (0.15f, 1.0f, (-10.0f - rmsDb) / 14.0f);
        const float driveDb = blockAmount * (2.2f + 5.8f * quietAssist);
        buffer.applyGain (juce::Decibels::decibelsToGain (driveDb));

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        lowShelf.process (ctx);
        midPeak.process (ctx);
        highShelf.process (ctx);

        compressor.setThreshold (-14.0f + 5.0f * blockAmount);
        compressor.setRatio (1.0f + 2.2f * blockAmount);
        compressor.process (ctx);

        if (channels >= 2)
            processStereoWidth (buffer, blockAmount);

        // Smooth parallel saturation. Wet amount is deliberately capped.
        const float satDrive = 1.0f + 1.55f * blockAmount;
        const float satWet = 0.10f + 0.25f * blockAmount;
        for (int ch = 0; ch < channels; ++ch)
        {
            float* p = buffer.getWritePointer (ch);
            for (int i = 0; i < samples; ++i)
            {
                const float x = p[i];
                const float shaped = std::tanh (x * satDrive) / std::tanh (satDrive);
                p[i] = juce::jmap (satWet, x, shaped);
            }
        }

        // Dry/wet scaling keeps low settings subtle.
        const float wet = std::sqrt (blockAmount);
        const float dry = 1.0f - wet;
        for (int ch = 0; ch < channels; ++ch)
        {
            float* out = buffer.getWritePointer (ch);
            const float* in = scratch.getReadPointer (ch);
            for (int i = 0; i < samples; ++i)
                out[i] = dry * in[i] + wet * out[i];
        }

        limiter.setThreshold (-0.85f);
        limiter.process (ctx);
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    void updateFilters (float a)
    {
        const float lowDb  = 1.6f * a;
        const float midDb  = 1.0f * a;
        const float highDb = 1.35f * a;

        *lowShelf.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 95.0f, 0.72f, juce::Decibels::decibelsToGain (lowDb));
        *midPeak.state   = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 900.0f, 0.85f, juce::Decibels::decibelsToGain (midDb));
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 7800.0f, 0.72f, juce::Decibels::decibelsToGain (highDb));
    }

    void processStereoWidth (juce::AudioBuffer<float>& b, float a)
    {
        float* l = b.getWritePointer (0);
        float* r = b.getWritePointer (1);
        const float width = 1.0f + 0.16f * a;

        // Keep low side information essentially unchanged, widen only the high-side component.
        const float cutoff = 120.0f;
        const float alpha = static_cast<float> (std::exp (-2.0 * juce::MathConstants<double>::pi * cutoff / sampleRate));

        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float mid  = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);

            sideLP_L = (1.0f - alpha) * side + alpha * sideLP_L;
            const float highSide = side - sideLP_L;
            const float newSide = sideLP_L + highSide * width;

            l[i] = mid + newSide;
            r[i] = mid - newSide;
        }
    }

    double sampleRate = 44100.0;
    juce::SmoothedValue<float> amountSmoothed;
    juce::AudioBuffer<float> scratch;
    Filter lowShelf, midPeak, highShelf;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Limiter<float> limiter;
    float sideLP_L = 0.0f, sideLP_R = 0.0f;
    float rmsEnvelope = 0.0f;
};
