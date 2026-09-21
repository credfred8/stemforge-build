#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

struct MasterSettings
{
    bool masterBypass = false;
    bool smartGain = true;
    bool cleanEqOn = true;
    bool dynamicEqOn = true;
    bool resonanceOn = true;
    bool glueOn = true;
    bool multibandOn = true;
    bool impactOn = true;
    bool analogOn = true;
    bool exciterOn = true;
    bool bassMonoOn = true;
    bool imagerOn = true;
    bool clipperOn = true;
    bool limiterOn = true;
    bool ditherOn = true;

    float inputTrimDb = 0.0f;
    float targetInputRmsDb = -18.0f;
    float lowShelfDb = 0.0f;
    float lowMidDb = 0.0f;
    float presenceDb = 0.0f;
    float airDb = 0.0f;
    float dynamicEq = 0.30f;
    float resonance = 0.22f;
    float glue = 0.35f;
    float multiband = 0.30f;
    float impact = 0.30f;
    float analog = 0.16f;
    float exciter = 0.12f;
    float bassMonoHz = 115.0f;
    float widthLow = 0.92f;
    float widthMid = 1.02f;
    float widthHigh = 1.08f;
    float dryWet = 1.0f;
    float clipDriveDb = 1.5f;
    float clipMix = 1.0f;
    float limiterDriveDb = 3.0f;
    float limiterCeilingDb = -0.9f;
    float outputTrimDb = 0.0f;
};

class MasterEngine
{
public:
    static constexpr int spectrumBins = 64;

    void prepare (double newSampleRate, int maximumBlockSize, int channels)
    {
        sampleRate = newSampleRate;
        numChannels = juce::jlimit (1, 2, channels);
        maxBlock = maximumBlockSize;

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maximumBlockSize, (juce::uint32) numChannels };
        lowCut.prepare (spec);
        lowShelf.prepare (spec);
        lowMid.prepare (spec);
        presence.prepare (spec);
        air.prepare (spec);
        resonanceFilter.prepare (spec);
        glueComp.prepare (spec);

        glueComp.setAttack (24.0f);
        glueComp.setRelease (180.0f);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, true);
        oversampling->initProcessing ((size_t) maximumBlockSize);

        dry.setSize (numChannels, maximumBlockSize);
        reset();
        updateFilters();
    }

    void reset()
    {
        lowCut.reset(); lowShelf.reset(); lowMid.reset(); presence.reset(); air.reset();
        resonanceFilter.reset(); glueComp.reset();
        if (oversampling) oversampling->reset();

        lowDynState.fill (0.0f);
        multibandLow.fill (0.0f);
        multibandHigh.fill (0.0f);
        bassLow.fill (0.0f);
        impactFast.fill (0.0f);
        impactSlow.fill (0.0f);
        smartGainDb = 0.0f;
        meterCounter = 0;
    }

    void setSettings (const MasterSettings& s)
    {
        settings = s;
        updateFilters();
        glueComp.setThreshold (-18.0f + 7.0f * settings.glue);
        glueComp.setRatio (1.5f + 2.5f * settings.glue);
        glueComp.setAttack (30.0f - 18.0f * settings.glue);
        glueComp.setRelease (220.0f - 90.0f * settings.glue);
    }

    int getLatencySamples() const
    {
        return oversampling ? (int) std::ceil (oversampling->getLatencyInSamples()) : 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() <= 0)
            return;

        measureInput (buffer);
        analyzeSpectrum (buffer, preSpectrum);

        if (settings.masterBypass)
        {
            measureOutput (buffer);
            analyzeSpectrum (buffer, postSpectrum);
            return;
        }

        dry.setSize (juce::jmin (2, buffer.getNumChannels()), buffer.getNumSamples(), false, false, true);
        dry.makeCopyOf (buffer, true);

        applyInputCoach (buffer);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        if (settings.cleanEqOn)
        {
            lowCut.process (ctx);
            lowShelf.process (ctx);
            lowMid.process (ctx);
            presence.process (ctx);
            air.process (ctx);
        }

        if (settings.dynamicEqOn)
            processDynamicEq (buffer);

        if (settings.resonanceOn)
            resonanceFilter.process (ctx);

        if (settings.glueOn)
            glueComp.process (ctx);

        if (settings.multibandOn)
            processMultiband (buffer);

        if (settings.impactOn)
            processImpact (buffer);

        if (settings.analogOn)
            processAnalog (buffer);

        if (settings.exciterOn)
            processExciter (buffer);

        if (settings.bassMonoOn)
            processBassMono (buffer);

        if (settings.imagerOn)
            processImager (buffer);

        if (settings.dryWet < 0.999f)
        {
            const float wet = juce::jlimit (0.0f, 1.0f, settings.dryWet);
            const float dryMix = 1.0f - wet;
            for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), dry.getNumChannels()); ++ch)
            {
                buffer.applyGain (ch, 0, buffer.getNumSamples(), wet);
                buffer.addFrom (ch, 0, dry, ch, 0, buffer.getNumSamples(), dryMix);
            }
        }

        if ((settings.clipperOn || settings.limiterOn) && oversampling)
            processOversampledFinal (buffer);

        buffer.applyGain (juce::Decibels::decibelsToGain (settings.outputTrimDb));

        if (settings.ditherOn)
            applyDither (buffer);

        measureOutput (buffer);
        analyzeSpectrum (buffer, postSpectrum);
    }

    std::atomic<float> inputRmsDb { -100.0f };
    std::atomic<float> inputPeakDb { -100.0f };
    std::atomic<float> outputRmsDb { -100.0f };
    std::atomic<float> outputPeakDb { -100.0f };
    std::atomic<float> loudnessEstimate { -100.0f };
    std::atomic<float> crestDb { 0.0f };
    std::atomic<float> correlation { 1.0f };
    std::atomic<float> smartGainAppliedDb { 0.0f };
    std::array<std::atomic<float>, spectrumBins> preSpectrum {};
    std::array<std::atomic<float>, spectrumBins> postSpectrum {};

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    void updateFilters()
    {
        if (sampleRate <= 0.0)
            return;

        *lowCut.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 20.0, 0.707);
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 105.0, 0.72, juce::Decibels::decibelsToGain (settings.lowShelfDb));
        *lowMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 320.0, 0.85, juce::Decibels::decibelsToGain (settings.lowMidDb));
        *presence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 3200.0, 0.90, juce::Decibels::decibelsToGain (settings.presenceDb));
        *air.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 10500.0, 0.70, juce::Decibels::decibelsToGain (settings.airDb));

        const float notchDb = -juce::jlimit (0.0f, 5.0f, settings.resonance * 5.0f);
        *resonanceFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 2850.0, 2.6, juce::Decibels::decibelsToGain (notchDb));
    }

    static float coeffForHz (float hz, double sr)
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
    }

    void applyInputCoach (juce::AudioBuffer<float>& buffer)
    {
        float applied = settings.inputTrimDb;
        if (settings.smartGain)
        {
            const float current = inputRmsDb.load();
            const float desired = juce::jlimit (-12.0f, 12.0f, settings.targetInputRmsDb - current);
            smartGainDb += 0.035f * (desired - smartGainDb);
            applied += smartGainDb;
        }
        else
        {
            smartGainDb *= 0.96f;
        }

        smartGainAppliedDb.store (applied);
        buffer.applyGain (juce::Decibels::decibelsToGain (applied));
    }

    void processDynamicEq (juce::AudioBuffer<float>& buffer)
    {
        const float a = coeffForHz (260.0f, sampleRate);
        const float amount = juce::jlimit (0.0f, 1.0f, settings.dynamicEq);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lp = lowDynState[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                lp += a * (x - lp);
                const float high = x - lp;

                const float lowAbs = std::abs (lp);
                const float highAbs = std::abs (high);
                const float lowReduction = 1.0f / (1.0f + amount * 3.2f * juce::jmax (0.0f, lowAbs - 0.22f));
                const float highReduction = 1.0f / (1.0f + amount * 2.4f * juce::jmax (0.0f, highAbs - 0.18f));
                d[i] = lp * lowReduction + high * highReduction;
            }

            lowDynState[(size_t) ch] = lp;
        }
    }

    void processMultiband (juce::AudioBuffer<float>& buffer)
    {
        const float lowA = coeffForHz (150.0f, sampleRate);
        const float highA = coeffForHz (4500.0f, sampleRate);
        const float amount = juce::jlimit (0.0f, 1.0f, settings.multiband);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lowState = multibandLow[(size_t) ch];
            float highState = multibandHigh[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                lowState += lowA * (x - lowState);
                highState += highA * (x - highState);

                const float low = lowState;
                const float high = x - highState;
                const float mid = x - low - high;

                auto tame = [amount](float v, float threshold, float strength)
                {
                    const float av = std::abs (v);
                    const float g = 1.0f / (1.0f + amount * strength * juce::jmax (0.0f, av - threshold));
                    return v * g;
                };

                d[i] = tame (low, 0.24f, 2.2f)
                     + tame (mid, 0.18f, 1.5f)
                     + tame (high, 0.12f, 1.2f);
            }

            multibandLow[(size_t) ch] = lowState;
            multibandHigh[(size_t) ch] = highState;
        }
    }

    void processImpact (juce::AudioBuffer<float>& buffer)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, settings.impact);
        const float fastRelease = std::exp (-1.0f / (0.012f * (float) sampleRate));
        const float slowRelease = std::exp (-1.0f / (0.160f * (float) sampleRate));

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float fast = impactFast[(size_t) ch];
            float slow = impactSlow[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float a = std::abs (x);
                fast = juce::jmax (a, fast * fastRelease);
                slow = juce::jmax (a, slow * slowRelease);
                const float transient = juce::jmax (0.0f, fast - slow);
                const float boost = 1.0f + amount * juce::jlimit (0.0f, 0.22f, transient);
                d[i] = x * boost;
            }

            impactFast[(size_t) ch] = fast;
            impactSlow[(size_t) ch] = slow;
        }
    }

    void processAnalog (juce::AudioBuffer<float>& buffer)
    {
        const float a = juce::jlimit (0.0f, 1.0f, settings.analog);
        const float drive = 1.0f + a * 3.2f;
        const float norm = std::tanh (drive);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float coloured = std::tanh (x * drive) / juce::jmax (0.001f, norm);
                d[i] = juce::jmap (a * 0.72f, x, coloured);
            }
        }
    }

    void processExciter (juce::AudioBuffer<float>& buffer)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, settings.exciter);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float prev = 0.0f;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float high = x - prev;
                prev += 0.32f * (x - prev);
                const float fizz = std::tanh (high * 7.0f) * amount * 0.075f;
                d[i] = x + fizz;
            }
        }
    }

    void processBassMono (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        const float a = coeffForHz (juce::jlimit (45.0f, 220.0f, settings.bassMonoHz), sampleRate);
        float ll = bassLow[0];
        float rr = bassLow[1];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            ll += a * (l[i] - ll);
            rr += a * (r[i] - rr);
            const float mono = 0.5f * (ll + rr);
            l[i] = (l[i] - ll) + mono;
            r[i] = (r[i] - rr) + mono;
        }

        bassLow[0] = ll;
        bassLow[1] = rr;
    }

    void processImager (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);

        const float lowA = coeffForHz (180.0f, sampleRate);
        const float highA = coeffForHz (5000.0f, sampleRate);
        float lowL = 0.0f, lowR = 0.0f, highLpL = 0.0f, highLpR = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float inL = l[i], inR = r[i];

            lowL += lowA * (inL - lowL);
            lowR += lowA * (inR - lowR);
            highLpL += highA * (inL - highLpL);
            highLpR += highA * (inR - highLpR);

            const float highL = inL - highLpL;
            const float highR = inR - highLpR;
            const float midL = inL - lowL - highL;
            const float midR = inR - lowR - highR;

            auto widenPair = [] (float a, float b, float width, float& oa, float& ob)
            {
                const float m = 0.5f * (a + b);
                const float s = 0.5f * (a - b) * width;
                oa = m + s;
                ob = m - s;
            };

            float loL, loR, miL, miR, hiL, hiR;
            widenPair (lowL, lowR, settings.widthLow, loL, loR);
            widenPair (midL, midR, settings.widthMid, miL, miR);
            widenPair (highL, highR, settings.widthHigh, hiL, hiR);

            l[i] = loL + miL + hiL;
            r[i] = loR + miR + hiR;
        }
    }

    void processOversampledFinal (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampling->processSamplesUp (block);

        const float clipDrive = juce::Decibels::decibelsToGain (settings.clipDriveDb);
        const float limDrive = juce::Decibels::decibelsToGain (settings.limiterDriveDb);
        const float ceiling = juce::Decibels::decibelsToGain (settings.limiterCeilingDb);

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float x = d[i];

                if (settings.clipperOn)
                {
                    const float driven = x * clipDrive;
                    const float clipped = std::tanh (driven * 1.35f) / std::tanh (1.35f);
                    x = juce::jmap (settings.clipMix, x, clipped);
                }

                if (settings.limiterOn)
                {
                    x *= limDrive;
                    const float normalized = std::tanh (x / juce::jmax (0.05f, ceiling));
                    x = normalized * ceiling;
                }

                d[i] = x;
            }
        }

        oversampling->processSamplesDown (block);
    }

    void applyDither (juce::AudioBuffer<float>& buffer)
    {
        const float lsb = 1.0f / 8388608.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] += (random.nextFloat() - random.nextFloat()) * lsb;
        }
    }

    void measureInput (const juce::AudioBuffer<float>& buffer)
    {
        float rms = 0.0f, peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        }
        rms /= (float) juce::jmax (1, buffer.getNumChannels());
        inputRmsDb.store (juce::Decibels::gainToDecibels (juce::jmax (rms, 1.0e-8f)));
        inputPeakDb.store (juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-8f)));
    }

    void measureOutput (const juce::AudioBuffer<float>& buffer)
    {
        float rms = 0.0f, peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        }
        rms /= (float) juce::jmax (1, buffer.getNumChannels());

        const float rmsDb = juce::Decibels::gainToDecibels (juce::jmax (rms, 1.0e-8f));
        const float peakDb = juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-8f));
        outputRmsDb.store (rmsDb);
        outputPeakDb.store (peakDb);
        loudnessEstimate.store (rmsDb - 0.7f);
        crestDb.store (peakDb - rmsDb);

        if (buffer.getNumChannels() >= 2)
        {
            const auto* l = buffer.getReadPointer (0);
            const auto* r = buffer.getReadPointer (1);
            double lr = 0.0, ll = 0.0, rr = 0.0;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                lr += (double) l[i] * (double) r[i];
                ll += (double) l[i] * (double) l[i];
                rr += (double) r[i] * (double) r[i];
            }
            correlation.store ((float) juce::jlimit (-1.0, 1.0, lr / std::sqrt (juce::jmax (1.0e-12, ll * rr))));
        }
        else
        {
            correlation.store (1.0f);
        }
    }

    void analyzeSpectrum (const juce::AudioBuffer<float>& buffer,
                          std::array<std::atomic<float>, spectrumBins>& target)
    {
        if ((++meterCounter % 3) != 0 || buffer.getNumSamples() < 16)
            return;

        const int n = buffer.getNumSamples();
        for (int b = 0; b < spectrumBins; ++b)
        {
            const float norm = (float) b / (float) (spectrumBins - 1);
            const float freq = 24.0f * std::pow (18000.0f / 24.0f, norm);
            const float w = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
            double re = 0.0, im = 0.0;

            for (int i = 0; i < n; i += 2)
            {
                float mono = 0.0f;
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    mono += buffer.getSample (ch, i);
                mono /= (float) juce::jmax (1, buffer.getNumChannels());

                const float phase = w * (float) i;
                re += mono * std::cos (phase);
                im -= mono * std::sin (phase);
            }

            const float mag = (float) std::sqrt (re * re + im * im) / (float) juce::jmax (1, n / 2);
            const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-7f));
            target[(size_t) b].store (db);
        }
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;
    int meterCounter = 0;
    float smartGainDb = 0.0f;
    MasterSettings settings;

    Filter lowCut, lowShelf, lowMid, presence, air, resonanceFilter;
    juce::dsp::Compressor<float> glueComp;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> dry;
    juce::Random random;

    std::array<float, 2> lowDynState {};
    std::array<float, 2> multibandLow {};
    std::array<float, 2> multibandHigh {};
    std::array<float, 2> bassLow {};
    std::array<float, 2> impactFast {};
    std::array<float, 2> impactSlow {};
};
