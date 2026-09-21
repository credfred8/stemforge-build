#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

struct VocalSettings
{
    float inputDb = 0.0f;
    float gateDb = -52.0f;
    float lowCutHz = 75.0f;
    float bodyDb = 0.0f;
    float presenceDb = 2.0f;
    float airDb = 2.5f;
    float deEss = 0.45f;
    float comp = 0.55f;
    float leveler = 0.40f;
    float parallel = 0.22f;
    float saturation = 0.16f;
    float grit = 0.10f;
    float doubler = 0.16f;
    float width = 1.05f;
    float delay = 0.05f;
    float reverb = 0.06f;
    float outputDb = -0.5f;
};

class VocalEngine
{
public:
    void prepare (double newSampleRate, int maximumBlockSize, int channels)
    {
        sampleRate = newSampleRate;
        numChannels = juce::jlimit (1, 2, channels);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maximumBlockSize, (juce::uint32) numChannels };

        hp.prepare (spec);
        body.prepare (spec);
        presence.prepare (spec);
        air.prepare (spec);
        compFast.prepare (spec);
        compSlow.prepare (spec);
        parallelComp.prepare (spec);
        limiter.prepare (spec);
        reverb.prepare (spec);

        compFast.setAttack (1.5f);
        compFast.setRelease (65.0f);
        compSlow.setAttack (25.0f);
        compSlow.setRelease (260.0f);
        parallelComp.setAttack (0.8f);
        parallelComp.setRelease (110.0f);

        reverbParams.roomSize = 0.16f;
        reverbParams.damping = 0.55f;
        reverbParams.wetLevel = 0.08f;
        reverbParams.dryLevel = 1.0f;
        reverb.setParameters (reverbParams);

        const int maxDelay = (int) std::ceil (sampleRate * 1.5);
        delayL.assign ((size_t) maxDelay, 0.0f);
        delayR.assign ((size_t) maxDelay, 0.0f);
        writePos = 0;

        temp.setSize (numChannels, maximumBlockSize);
        reset();
    }

    void reset()
    {
        hp.reset(); body.reset(); presence.reset(); air.reset();
        compFast.reset(); compSlow.reset(); parallelComp.reset();
        limiter.reset(); reverb.reset();
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        gateEnv = 0.0f;
        riderEnv = 0.0f;
    }

    void setSettings (const VocalSettings& s)
    {
        settings = s;
        updateFilters();
        compFast.setThreshold (-24.0f + settings.comp * 10.0f);
        compFast.setRatio (3.0f + settings.comp * 5.0f);
        compSlow.setThreshold (-20.0f + settings.leveler * 8.0f);
        compSlow.setRatio (2.0f + settings.leveler * 2.5f);
        parallelComp.setThreshold (-30.0f);
        parallelComp.setRatio (10.0f);
        limiter.setThreshold (-1.0f);
        limiter.setRelease (80.0f);

        reverbParams.roomSize = juce::jlimit (0.08f, 0.42f, 0.10f + settings.reverb * 1.4f);
        reverbParams.damping = 0.58f;
        reverbParams.wetLevel = settings.reverb * 0.42f;
        reverbParams.dryLevel = 1.0f;
        reverb.setParameters (reverbParams);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0) return;
        temp.setSize (juce::jmin (2, buffer.getNumChannels()), buffer.getNumSamples(), false, false, true);

        const float inputGain = juce::Decibels::decibelsToGain (settings.inputDb);
        const float outputGain = juce::Decibels::decibelsToGain (settings.outputDb);
        const float gateThreshold = juce::Decibels::decibelsToGain (settings.gateDb);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGain (ch, 0, buffer.getNumSamples(), inputGain);

        // gentle automatic rider + expander
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            riderEnv = juce::jmax (peak, riderEnv * 0.9975f);
            const float target = 0.16f;
            const float rider = juce::jlimit (0.72f, 1.55f, target / juce::jmax (0.045f, riderEnv));

            gateEnv += 0.015f * (((peak > gateThreshold) ? 1.0f : 0.18f) - gateEnv);
            const float gain = rider * gateEnv;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
        }

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        hp.process (ctx);
        body.process (ctx);
        presence.process (ctx);
        air.process (ctx);

        // adaptive de-esser based on high-frequency difference energy
        for (int i = 1; i < buffer.getNumSamples(); ++i)
        {
            float sib = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                sib += std::abs (buffer.getSample (ch, i) - buffer.getSample (ch, i - 1));
            sib /= (float) buffer.getNumChannels();
            const float reduction = 1.0f / (1.0f + settings.deEss * 8.0f * juce::jmax (0.0f, sib - 0.07f));
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * reduction);
        }

        compFast.process (ctx);
        compSlow.process (ctx);

        // parallel density
        temp.makeCopyOf (buffer, true);
        {
            juce::dsp::AudioBlock<float> pblock (temp);
            juce::dsp::ProcessContextReplacing<float> pctx (pblock);
            parallelComp.process (pctx);
        }
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom (ch, 0, temp, juce::jmin (ch, temp.getNumChannels() - 1), 0, buffer.getNumSamples(), settings.parallel * 0.55f);

        // harmonic colour / exciter
        const float sat = settings.saturation;
        const float grit = settings.grit;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float prev = 0.0f;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float warm = std::tanh (x * (1.0f + 4.0f * sat)) / std::tanh (1.0f + 4.0f * sat);
                const float high = x - prev;
                prev = x;
                const float sand = std::tanh (high * 7.0f) * grit * 0.13f;
                d[i] = juce::jlimit (-2.0f, 2.0f, juce::jmap (sat, x, warm) + sand);
            }
        }

        applyDoublerDelay (buffer);

        if (buffer.getNumChannels() >= 2)
        {
            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            const float width = juce::jlimit (0.55f, 1.65f, settings.width);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float mid = 0.5f * (l[i] + r[i]);
                const float side = 0.5f * (l[i] - r[i]) * width;
                l[i] = mid + side;
                r[i] = mid - side;
            }
        }

        reverb.process (ctx);
        limiter.process (ctx);
        buffer.applyGain (outputGain);

        float rms = 0.0f, peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        }
        meterRms.store (rms / (float) buffer.getNumChannels());
        meterPeak.store (peak);
    }

    std::atomic<float> meterRms { 0.0f };
    std::atomic<float> meterPeak { 0.0f };

private:
    void updateFilters()
    {
        if (sampleRate <= 0.0) return;
        *hp.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, juce::jlimit (45.0f, 180.0f, settings.lowCutHz), 0.707f);
        *body.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 180.0, 0.8f, juce::Decibels::decibelsToGain (settings.bodyDb));
        *presence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 3300.0, 0.95f, juce::Decibels::decibelsToGain (settings.presenceDb));
        *air.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 9500.0, 0.72f, juce::Decibels::decibelsToGain (settings.airDb));
    }

    void applyDoublerDelay (juce::AudioBuffer<float>& buffer)
    {
        if (delayL.empty()) return;
        const int size = (int) delayL.size();
        const int d1 = juce::jlimit (1, size - 2, (int) (sampleRate * 0.014));
        const int d2 = juce::jlimit (1, size - 2, (int) (sampleRate * 0.021));
        const int echo = juce::jlimit (1, size - 2, (int) (sampleRate * 0.118));
        const float dbl = settings.doubler;
        const float echoMix = settings.delay * 0.32f;

        auto* l = buffer.getWritePointer (0);
        float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float inL = l[i];
            const float inR = r ? r[i] : inL;
            const int rp1 = (writePos - d1 + size) % size;
            const int rp2 = (writePos - d2 + size) % size;
            const int rpe = (writePos - echo + size) % size;

            const float dl = delayL[(size_t) rp1];
            const float dr = delayR[(size_t) rp2];
            const float el = delayL[(size_t) rpe];
            const float er = delayR[(size_t) rpe];

            l[i] = inL + dbl * (0.33f * dl - 0.12f * dr) + echoMix * el;
            if (r)
                r[i] = inR + dbl * (0.33f * dr - 0.12f * dl) + echoMix * er;

            delayL[(size_t) writePos] = inL + el * 0.18f;
            delayR[(size_t) writePos] = inR + er * 0.18f;
            if (++writePos >= size) writePos = 0;
        }
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    VocalSettings settings;
    juce::dsp::IIR::Filter<float> hp, body, presence, air;
    juce::dsp::Compressor<float> compFast, compSlow, parallelComp;
    juce::dsp::Limiter<float> limiter;
    juce::dsp::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    juce::AudioBuffer<float> temp;
    std::vector<float> delayL, delayR;
    int writePos = 0;
    float gateEnv = 0.0f;
    float riderEnv = 0.0f;
};
